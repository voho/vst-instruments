#!/usr/bin/env python3
"""Compare modelled and recorded string damping as a curve against frequency.

Protocol. For every example in a rendered physical-fit corpus this reads the
recording and the model render that the fit renderer wrote side by side, and
estimates one early decay rate per partial:

* the signal is aligned on its own 2%-of-peak onset and analysed with an
  8192-point Hann STFT at 7/8 overlap;
* a partial contributes only where its band peak stays above twelve times the
  local interharmonic floor (the median power between that partial and the
  next) and within 45 dB of its own peak, which keeps a recording's noise floor
  and a room tail out of the estimate;
* the rate is the least-squares slope of that band's level in dB over
  0.15-1.2 s, so it measures early decay, not the settled tail;
* rates are pooled by absolute partial frequency, never by harmonic number,
  because string damping is a property of frequency rather than of which note
  produced it. Recording and model are pooled independently, so a band is only
  comparable where both report a usable count.

Reported medians are dB/s: larger means faster decay. The fitter's own decay
term also works in dB/s, but it reports one robust rate per partial over
0.12-4.0 s and compares it note by note, so it cannot show where on the
frequency axis a damping error sits; this can.

Usage:

    python3 Tools/AuditDampingCurve.py /path/to/rendered-fit-corpus
    python3 Tools/AuditDampingCurve.py --self-test

NumPy and SciPy are required.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np
from scipy.signal import stft

BAND_EDGES = np.asarray(
    [80, 120, 180, 270, 400, 600, 900, 1350, 2000, 3000, 4500, 6800, 9000],
    dtype=float,
)
MINIMUM_BAND_COUNT = 8
WINDOW = 8192
# A band is a measurement only if it survives its own analysis window.
CHECK_WINDOW = 4096
STABILITY_TOLERANCE_DB_PER_SECOND = 3.0
ANALYSIS_SECONDS = 3.2
RATE_WINDOW_SECONDS = (0.15, 1.2)
FLOOR_MARGIN = 12.0
PEAK_RANGE_DB = 45.0


def _read(path: Path, channels: int) -> np.ndarray:
    data = np.fromfile(path, dtype="<f4")
    if channels == 2:
        data = data.reshape(-1, 2).mean(axis=1)
    return data.astype(np.float64)


def _onset(signal: np.ndarray) -> int:
    peak = float(np.max(np.abs(signal))) if signal.size else 0.0
    if peak <= 0.0:
        return 0
    return int(np.argmax(np.abs(signal) > 0.02 * peak))


def partial_rates(
    signal: np.ndarray, rate: int, fundamental: float, maximum_partial: int = 40,
    window: int | None = None,
) -> list[tuple[float, float]]:
    """Return (partial frequency, decay rate in dB/s) for usable partials."""
    tail = signal[_onset(signal) : _onset(signal) + round(ANALYSIS_SECONDS * rate)]
    window = min(window or WINDOW,
                 1 << int(np.floor(np.log2(max(tail.size, 2)))))
    if tail.size < 4 * window or window < 1024:
        return []
    frequency, times, spectrum = stft(
        tail, fs=rate, window="hann", nperseg=window,
        noverlap=7 * window // 8, nfft=window, boundary=None, padded=False,
    )
    power = np.abs(spectrum) ** 2
    resolution = float(frequency[1] - frequency[0])
    result: list[tuple[float, float]] = []
    for index in range(1, maximum_partial + 1):
        centre = fundamental * index
        if centre > min(9000.0, 0.45 * rate):
            break
        half_width = max(resolution, centre * 0.010)
        band = (frequency >= centre - half_width) & (frequency <= centre + half_width)
        gap = (frequency >= centre + 2 * half_width) & (
            frequency <= centre + fundamental - 2 * half_width
        )
        if not np.any(band) or not np.any(gap):
            continue
        curve = power[band].max(axis=0)
        floor = float(np.median(power[gap].mean(axis=1)))
        levels = 10.0 * np.log10(np.maximum(curve, 1.0e-30))
        usable = (
            (times >= RATE_WINDOW_SECONDS[0])
            & (times <= RATE_WINDOW_SECONDS[1])
            & (curve > FLOOR_MARGIN * floor)
            & (levels > levels.max() - PEAK_RANGE_DB)
        )
        if int(np.count_nonzero(usable)) < 10:
            continue
        slope = float(np.polyfit(times[usable], levels[usable], 1)[0])
        if slope < -1.0:
            result.append((centre, -slope))
    return result


def _midi_hz(midi: int) -> float:
    return 440.0 * 2.0 ** ((midi - 69) / 12.0)


def collect(directory: Path,
            splits: tuple[str, ...] = ("train", "validation"),
            window: int | None = None,
            ) -> dict[tuple[str, str], list[tuple[float, float]]]:
    pooled: dict[tuple[str, str], list[tuple[float, float]]] = {}
    for split in splits:
        manifest_path = directory / f"{split}.json"
        if not manifest_path.is_file():
            continue
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        for example in manifest["examples"]:
            fundamental = _midi_hz(int(example["midi"]))
            for role, side in (("target", "recording"), ("model", "model")):
                entry = example[role]
                signal = _read(directory / entry["path"], int(entry["channels"]))
                pooled.setdefault((example["material"], side), []).extend(
                    partial_rates(signal, int(entry["sample_rate"]), fundamental,
                                  window=window)
                )
    return pooled


def _band_medians(pooled, material, side, low, high):
    values = [rate for centre, rate in pooled.get((material, side), [])
              if low <= centre < high]
    return (float(np.median(values)) if len(values) >= MINIMUM_BAND_COUNT
            else None, len(values))


def report(pooled: dict[tuple[str, str], list[tuple[float, float]]],
           check: dict | None = None) -> dict:
    materials = sorted({material for material, _ in pooled})
    out: dict = {"bands_hz": BAND_EDGES.tolist(), "materials": {}}
    for material in materials:
        rows = []
        for low, high in zip(BAND_EDGES[:-1], BAND_EDGES[1:]):
            entry: dict = {"low_hz": low, "high_hz": high}
            for side in ("recording", "model"):
                entry[side], entry[side + "_count"] = _band_medians(
                    pooled, material, side, low, high)
            if entry["recording"] is not None and entry["model"] is not None:
                difference = entry["model"] - entry["recording"]
                if check is not None:
                    other = [_band_medians(check, material, side, low, high)[0]
                             for side in ("recording", "model")]
                    if other[0] is None or other[1] is None or abs(
                            (other[1] - other[0]) - difference
                    ) > STABILITY_TOLERANCE_DB_PER_SECOND:
                        # The band's answer depends on the analysis window, so
                        # it is the floor being measured, not the instrument.
                        entry["difference"] = None
                        entry["unstable"] = True
                        rows.append(entry)
                        continue
                entry["difference"] = difference
            rows.append(entry)
        out["materials"][material] = rows
    return out


def _print(result: dict) -> None:
    for material, rows in result["materials"].items():
        print(f"== {material}   median early decay, dB/s")
        print(f"{'band Hz':>14}{'recording':>12}{'model':>10}{'model-rec':>11}"
              f"{'n rec':>8}{'n mod':>7}")
        for row in rows:
            if row.get("unstable"):
                print(f'{row["low_hz"]:6.0f}-{row["high_hz"]:6.0f}'
                      f'{"depends on the analysis window":>44}')
                continue
            if row["recording"] is None or row["model"] is None:
                continue
            print(f'{row["low_hz"]:6.0f}-{row["high_hz"]:6.0f}'
                  f'{row["recording"]:12.1f}{row["model"]:10.1f}'
                  f'{row["difference"]:+11.1f}'
                  f'{row["recording_count"]:8d}{row["model_count"]:7d}')


def self_test() -> int:
    """A synthetic tone with a known per-partial decay must be recovered."""
    rate = 48000
    fundamental = 220.0
    seconds = 3.2
    time = np.arange(int(seconds * rate)) / rate
    signal = np.zeros_like(time)
    expected = {}
    for index in range(1, 13):
        decay_db_s = 6.0 * index
        expected[fundamental * index] = decay_db_s
        amplitude = 10.0 ** (-decay_db_s * time / 20.0) / index
        signal += amplitude * np.sin(2.0 * np.pi * fundamental * index * time)
    signal += 1.0e-6 * np.sin(2.0 * np.pi * 11111.0 * time)
    measured = dict(partial_rates(signal, rate, fundamental))
    if len(measured) < 10:
        print(f"self-test failed: only {len(measured)} partials recovered")
        return 1
    worst = 0.0
    for centre, want in expected.items():
        got = measured.get(centre)
        if got is None:
            continue
        worst = max(worst, abs(got - want))
    if worst > 1.0:
        print(f"self-test failed: worst decay-rate error {worst:.3f} dB/s")
        return 1
    print(f"self-test passed: worst decay-rate error {worst:.4f} dB/s "
          f"over {len(measured)} partials")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("corpus", type=Path, nargs="?")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--json", action="store_true")
    parser.add_argument(
        "--split", choices=("train", "validation", "both"), default="both",
        help="restrict the audit to one split (default: both)",
    )
    arguments = parser.parse_args()
    if arguments.self_test:
        return self_test()
    if arguments.corpus is None:
        parser.error("a rendered fit-corpus directory is required")
    splits = (("train", "validation") if arguments.split == "both"
              else (arguments.split,))
    corpus = arguments.corpus.resolve()
    result = report(collect(corpus, splits),
                    collect(corpus, splits, window=CHECK_WINDOW))
    if arguments.json:
        json.dump(result, sys.stdout, indent=2)
        sys.stdout.write("\n")
    else:
        _print(result)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
