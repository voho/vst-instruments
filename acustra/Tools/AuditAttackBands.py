#!/usr/bin/env python3
"""Compare modelled and recorded attack spectra band by band, shape only.

Protocol. For every example in a rendered physical-fit corpus this reads the
recording and the model render the fit renderer wrote side by side, takes the
first 350 ms from each signal's own 2%-of-peak onset, and sums a Hann-windowed
power spectrum into octave bands from 80 Hz to 10 kHz.

Each render is then normalised to its own band total, so what is reported is
spectral shape and never level: the fit already scores level through its
dynamics term, and the recordings carry a per-zone playback trim that would
otherwise dominate. A positive difference means the model puts more of its
attack energy in that band than the recording does.

Everything is reported per captured velocity layer. A pooled figure over the
corpus's four archtop layers is not evidence about any one of them. Pooled over
the 68 archtop rows the 2560-5120 Hz band reads -0.2 dB against the recordings
on the shipping corpus (+0.1 dB on the 2026-09-01 one), which says the model
has that band right; per layer the same rows run +4.4 dB at the softest to
-8.5 dB at the loudest (+3.3 to -10.3), so it has that band right at no
velocity. The band above it is not hidden the same way -- 5120-10000 Hz pools
to -8.6 dB (-7.9) and runs -4.2 to -14.2 dB (-2.7 to -17.5) -- but its pooled
figure still averages a deficit that trebles with velocity. A mechanism that
grows with velocity is exactly what a pooled median cannot show, and the
2026-09-02 audit entry asked for this split after one was read as though it
described the loudest layer.

The difference column is the median of the per-example model-minus-recording
differences, not the difference of the two medians beside it: the comparison is
paired, one render against the recording it was rendered for.

This exists because a single number cannot say where an attack differs. A
spectral centroid can be dragged down by an excess at the bottom or a shortfall
at the top, and those call for opposite corrections; reading one as the other
sent an earlier investigation after the pluck's brightness when the difference
was in the body's low end. Bands separate the two. Two scalars are reported
beside the bands because the log quotes them and they are cheap here:

* the 0-12 ms spectral centroid over 80 Hz to 10 kHz, in cents, which is the
  descriptor the 2026-08-30 take-to-take entry measured; the band limit is not
  cosmetic, since targets are 44.1 kHz and model renders 48 kHz and an
  unlimited centroid would compare two different Nyquist frequencies;
* the H5-H12 balance over 80-250 ms: the mean level of partials 5 to 12 minus
  the mean level of partials 1 to 4, each peak-picked within 35 cents of its
  harmonic. It reproduces the audit's per-layer reading of the recordings
  (-20.4, -15.9, -13.2, -11.3 dB, softest layer first), which do not move.
  The model reads -21.6, -21.3, -21.3, -22.4 dB on the shipping corpus; the
  -23.7, -23.3, -22.5, -22.8 dB the log quotes are the 2026-09-01 corpus,
  before the two-way junction and the stub anchor.

Usage:

    python3 Tools/AuditAttackBands.py /path/to/rendered-fit-corpus
    python3 Tools/AuditAttackBands.py --self-test

NumPy is required.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np

BAND_EDGES = np.asarray([80, 160, 320, 640, 1280, 2560, 5120, 10000], dtype=float)
ATTACK_SECONDS = 0.35
ONSET_FRACTION = 0.02
STABILITY_TOLERANCE_DB = 1.0
CENTROID_SECONDS = 0.012
CENTROID_STABILITY_CENTS = 50.0
BALANCE_WINDOW = (0.080, 0.250)
BALANCE_LOW_PARTIALS = 4
BALANCE_MAX_PARTIAL = 12
PARTIAL_HALF_WIDTH_CENTS = 35.0


def _read(path: Path, channels: int) -> np.ndarray:
    data = np.fromfile(path, dtype="<f4")
    if channels == 2:
        data = data.reshape(-1, 2).mean(axis=1)
    return data.astype(np.float64)


def _onset(signal: np.ndarray) -> int | None:
    peak = float(np.max(np.abs(signal))) if signal.size else 0.0
    if peak <= 0.0:
        return None
    return int(np.argmax(np.abs(signal) > ONSET_FRACTION * peak))


def _power_spectrum(signal: np.ndarray, rate: int, first: int,
                    last: int) -> tuple[np.ndarray, np.ndarray] | None:
    segment = signal[max(0, first) : min(signal.size, last)]
    if segment.size < 32:
        return None
    segment = segment - float(np.mean(segment))
    padded = 1 << max(8, int(np.ceil(np.log2(segment.size * 4))))
    spectrum = np.abs(np.fft.rfft(segment * np.hanning(segment.size), padded)) ** 2
    return np.fft.rfftfreq(padded, 1.0 / rate), spectrum


def _shape_once(signal: np.ndarray, rate: int, onset: int,
                length: int) -> np.ndarray | None:
    segment = signal[onset : onset + length]
    if segment.size < 4096:
        return None
    spectrum = np.abs(np.fft.rfft(segment * np.hanning(segment.size))) ** 2
    frequency = np.fft.rfftfreq(segment.size, 1.0 / rate)
    bands = np.asarray([
        spectrum[(frequency >= low) & (frequency < high)].sum()
        for low, high in zip(BAND_EDGES[:-1], BAND_EDGES[1:])
    ])
    total = bands.sum()
    if not np.isfinite(total) or total <= 0.0:
        return None
    return 10.0 * np.log10(np.maximum(bands, 1.0e-30) / total)


def band_shape_db(signal: np.ndarray, rate: int) -> np.ndarray | None:
    """Octave-band attack energy in dB, normalised to the signal's own total.

    A band whose reading depends on the analysis window is not a measurement of
    the signal, and this refuses to report one. Where a band sits far below a
    loud neighbour its content is that neighbour's leakage skirt, and the skirt
    moves with the bin grid: on a low steel E, one sample of window length moved
    the 80-160 Hz band by 19 dB with a Hann window and 13 dB with a
    Blackman-Harris one. The shape is therefore computed at two window lengths
    5% apart and a band is returned as NaN unless they agree within
    STABILITY_TOLERANCE_DB.
    """
    onset = _onset(signal)
    if onset is None:
        return None
    length = round(ATTACK_SECONDS * rate)
    first = _shape_once(signal, rate, onset, length)
    second = _shape_once(signal, rate, onset, round(1.05 * length))
    if first is None or second is None:
        return None
    unstable = np.abs(first - second) > STABILITY_TOLERANCE_DB
    return np.where(unstable, np.nan, first)


def _centroid_once(signal: np.ndarray, rate: int, onset: int,
                   length: int) -> float:
    measured = _power_spectrum(signal, rate, onset, onset + length)
    if measured is None:
        return float("nan")
    frequency, spectrum = measured
    selected = (frequency >= BAND_EDGES[0]) & (frequency < BAND_EDGES[-1])
    total = float(spectrum[selected].sum())
    if not np.isfinite(total) or total <= 0.0:
        return float("nan")
    return float((frequency[selected] * spectrum[selected]).sum() / total)


def attack_centroid_hz(signal: np.ndarray, rate: int) -> float:
    """Power centroid of the first 12 ms over 80 Hz to 10 kHz, or NaN.

    Same window-stability rule as the bands: two lengths 5% apart must agree,
    here within CENTROID_STABILITY_CENTS.
    """
    onset = _onset(signal)
    if onset is None:
        return float("nan")
    length = round(CENTROID_SECONDS * rate)
    first = _centroid_once(signal, rate, onset, length)
    second = _centroid_once(signal, rate, onset, round(1.05 * length))
    if not (np.isfinite(first) and np.isfinite(second)):
        return float("nan")
    if abs(1200.0 * np.log2(second / first)) > CENTROID_STABILITY_CENTS:
        return float("nan")
    return first


def harmonic_balance_db(signal: np.ndarray, rate: int, midi: int) -> float:
    """Mean level of partials 5-12 minus the mean level of partials 1-4, dB."""
    onset = _onset(signal)
    if onset is None:
        return float("nan")
    measured = _power_spectrum(
        signal, rate,
        onset + round(BALANCE_WINDOW[0] * rate),
        onset + round(BALANCE_WINDOW[1] * rate),
    )
    if measured is None:
        return float("nan")
    frequency, spectrum = measured
    fundamental = 440.0 * 2.0 ** ((midi - 69.0) / 12.0)
    ratio = 2.0 ** (PARTIAL_HALF_WIDTH_CENTS / 1200.0)
    levels = np.full(BALANCE_MAX_PARTIAL, np.nan)
    for index in range(BALANCE_MAX_PARTIAL):
        expected = (index + 1) * fundamental
        if expected >= 0.46 * rate:
            continue
        selected = (frequency >= expected / ratio) & (frequency <= expected * ratio)
        if np.any(selected):
            levels[index] = 10.0 * np.log10(
                max(float(spectrum[selected].max()), 1.0e-30))
    low = levels[:BALANCE_LOW_PARTIALS]
    high = levels[BALANCE_LOW_PARTIALS:]
    if not np.any(np.isfinite(low)) or not np.any(np.isfinite(high)):
        return float("nan")
    return float(np.nanmean(high) - np.nanmean(low))


def describe(signal: np.ndarray, rate: int, midi: int) -> dict | None:
    shape = band_shape_db(signal, rate)
    if shape is None:
        return None
    return {
        "bands": shape,
        "centroid_hz": attack_centroid_hz(signal, rate),
        "balance_db": harmonic_balance_db(signal, rate, midi),
    }


def collect(directory: Path) -> dict[tuple[str, float], list[tuple[dict, dict]]]:
    pooled: dict[tuple[str, float], list[tuple[dict, dict]]] = {}
    for split in ("train", "validation"):
        manifest_path = directory / f"{split}.json"
        if not manifest_path.is_file():
            continue
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        for example in manifest["examples"]:
            pair = []
            for role in ("target", "model"):
                entry = example[role]
                pair.append(describe(
                    _read(directory / entry["path"], int(entry["channels"])),
                    int(entry["sample_rate"]),
                    int(example["midi"]),
                ))
            if pair[0] is None or pair[1] is None:
                continue
            key = (example["material"], float(example["velocity"]))
            pooled.setdefault(key, []).append((pair[0], pair[1]))
    return pooled


def _median(values: np.ndarray) -> float | None:
    """None unless at least a third of the examples measured the band stably."""
    finite = values[np.isfinite(values)]
    if finite.size * 3 < values.size or finite.size == 0:
        return None
    return float(np.median(finite))


def _layer_report(rows: list[tuple[dict, dict]]) -> dict:
    recorded = np.asarray([a["bands"] for a, _ in rows])
    modelled = np.asarray([b["bands"] for _, b in rows])
    difference = modelled - recorded
    node: dict = {
        "examples": len(rows),
        "bands": [
            {
                "low_hz": float(low), "high_hz": float(high),
                "recording_db": _median(recorded[:, index]),
                "model_db": _median(modelled[:, index]),
                "difference_db": _median(difference[:, index]),
                "stable_examples": int(np.count_nonzero(
                    np.isfinite(difference[:, index]))),
            }
            for index, (low, high)
            in enumerate(zip(BAND_EDGES[:-1], BAND_EDGES[1:]))
        ],
    }
    recorded_centroid = np.asarray([a["centroid_hz"] for a, _ in rows])
    modelled_centroid = np.asarray([b["centroid_hz"] for _, b in rows])
    node["centroid"] = {
        "recording_hz": _median(recorded_centroid),
        "model_hz": _median(modelled_centroid),
        "difference_cents": _median(
            1200.0 * np.log2(modelled_centroid / recorded_centroid)),
    }
    recorded_balance = np.asarray([a["balance_db"] for a, _ in rows])
    modelled_balance = np.asarray([b["balance_db"] for _, b in rows])
    node["balance"] = {
        "recording_db": _median(recorded_balance),
        "model_db": _median(modelled_balance),
        "difference_db": _median(modelled_balance - recorded_balance),
    }
    return node


def report(pooled: dict[tuple[str, float], list[tuple[dict, dict]]]) -> dict:
    out: dict = {"band_edges_hz": BAND_EDGES.tolist(), "materials": {}}
    for material, velocity in sorted(pooled):
        out["materials"].setdefault(material, {})[f"{velocity:g}"] = (
            _layer_report(pooled[(material, velocity)]))
    return out


def _print(result: dict) -> None:
    for material, layers in result["materials"].items():
        for velocity, node in layers.items():
            print(f"== {material} velocity {velocity}   attack band shape, "
                  f"first {ATTACK_SECONDS * 1000:.0f} ms, n={node['examples']}")
            print(f'{"band Hz":>14}{"recording":>12}{"model":>10}'
                  f'{"model-rec":>12}{"stable n":>8}')
            for row in node["bands"]:
                if row["difference_db"] is None:
                    print(f'{row["low_hz"]:6.0f}-{row["high_hz"]:6.0f}'
                          f'{"below the analysis floor":>34}')
                    continue
                print(f'{row["low_hz"]:6.0f}-{row["high_hz"]:6.0f}'
                      f'{row["recording_db"]:12.1f}{row["model_db"]:10.1f}'
                      f'{row["difference_db"]:+12.1f}'
                      f'{row["stable_examples"]:8d}')
            centroid = node["centroid"]
            if centroid["difference_cents"] is not None:
                print(f'{"0-12 ms centroid":>14}'
                      f'{centroid["recording_hz"]:10.0f} Hz'
                      f'{centroid["model_hz"]:8.0f} Hz'
                      f'{centroid["difference_cents"]:+10.0f} cents')
            balance = node["balance"]
            if balance["difference_db"] is not None:
                print(f'{"H5-H12 balance":>14}{balance["recording_db"]:12.1f}'
                      f'{balance["model_db"]:10.1f}'
                      f'{balance["difference_db"]:+12.1f}')


def _harmonic_note(rate: int, fundamental: float, seconds: float,
                   high_gain: float) -> np.ndarray:
    """A decaying harmonic note whose partials above 2560 Hz are scaled."""
    time = np.arange(int(seconds * rate)) / rate
    signal = np.zeros_like(time)
    for index in range(1, 41):
        frequency = fundamental * index
        if frequency >= 0.45 * rate:
            break
        amplitude = 1.0 / index ** 2
        if frequency >= 2560.0:
            amplitude *= high_gain
        signal += amplitude * np.exp(-3.0 * time) * np.sin(
            2.0 * np.pi * frequency * time)
    return signal


def self_test() -> int:
    """Shape must be level-invariant and track pitch, and a velocity-dependent
    difference must survive per layer while a pooled median hides it."""
    rate = 48000
    time = np.arange(int(1.0 * rate)) / rate
    quiet = np.sin(2.0 * np.pi * 220.0 * time)
    loud = 8.0 * quiet
    shifted = np.sin(2.0 * np.pi * 440.0 * time)
    a, b, c = (band_shape_db(x, rate) for x in (quiet, loud, shifted))
    if a is None or b is None or c is None:
        print("self-test failed: a band shape could not be measured")
        return 1
    if float(np.nanmax(np.abs(a - b))) > 1.0e-9:
        print("self-test failed: level changed the reported shape")
        return 1
    # 220 Hz belongs to the 160-320 band and 440 Hz to the 320-640 one. A pure
    # tone leaves every other band below the analysis floor, so they come back
    # NaN, which is the point: only the band that holds signal is reported.
    if int(np.nanargmax(a)) != 1 or int(np.nanargmax(c)) != 2:
        print(f"self-test failed: 220 Hz landed in band {int(np.nanargmax(a))} "
              f"and 440 Hz in band {int(np.nanargmax(c))}")
        return 1
    if np.isfinite(a).sum() > 3:
        print("self-test failed: a pure tone reported signal in "
              f"{int(np.isfinite(a).sum())} bands")
        return 1

    # Two synthetic velocity layers, three notes each. The recording is the same
    # note in both; the model loses 2 dB above 2560 Hz in the soft layer and
    # 12 dB in the loud one, so the two layers disagree by 10 dB and their pool
    # reports neither.
    soft_deficit, loud_deficit = -2.0, -12.0
    pooled: dict[tuple[str, float], list[tuple[dict, dict]]] = {}
    for velocity, deficit in ((16.0, soft_deficit), (112.0, loud_deficit)):
        for midi in (57, 60, 64):
            fundamental = 440.0 * 2.0 ** ((midi - 69.0) / 12.0)
            recording = describe(
                _harmonic_note(rate, fundamental, 1.0, 1.0), rate, midi)
            model = describe(
                _harmonic_note(rate, fundamental, 1.0,
                               10.0 ** (deficit / 20.0)), rate, midi)
            if recording is None or model is None:
                print("self-test failed: a synthetic layer could not be measured")
                return 1
            pooled.setdefault(("synthetic", velocity), []).append((recording, model))
    layers = report(pooled)["materials"]["synthetic"]
    measured = {}
    for name, node in layers.items():
        for row in node["bands"]:
            if row["low_hz"] == 5120.0:
                measured[name] = row["difference_db"]
    if len(measured) != 2 or any(value is None for value in measured.values()):
        print("self-test failed: the 5120-10000 Hz band was not reported "
              "for both layers")
        return 1
    if abs(measured["16"] - soft_deficit) > 0.5 or abs(
            measured["112"] - loud_deficit) > 0.5:
        print(f"self-test failed: per-layer 5120-10000 Hz read "
              f"{measured['16']:+.2f} and {measured['112']:+.2f} dB against "
              f"{soft_deficit:+.1f} and {loud_deficit:+.1f}")
        return 1
    combined = _layer_report(pooled[("synthetic", 16.0)]
                             + pooled[("synthetic", 112.0)])
    pooled_difference = [row["difference_db"] for row in combined["bands"]
                         if row["low_hz"] == 5120.0][0]
    if abs(pooled_difference - measured["112"]) < 3.0:
        print("self-test failed: pooling the two layers did not hide the "
              "loud layer's deficit")
        return 1
    print(f"self-test passed: shape is level-invariant, bands track pitch, a "
          f"pure tone reports only {int(np.isfinite(a).sum())} of {a.size} "
          f"bands, and two velocity layers read {measured['16']:+.2f} and "
          f"{measured['112']:+.2f} dB where their pool reads "
          f"{pooled_difference:+.2f}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("corpus", type=Path, nargs="?")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--json", action="store_true")
    arguments = parser.parse_args()
    if arguments.self_test:
        return self_test()
    if arguments.corpus is None:
        parser.error("a rendered fit-corpus directory is required")
    result = report(collect(arguments.corpus.resolve()))
    if arguments.json:
        json.dump(result, sys.stdout, indent=2)
        sys.stdout.write("\n")
    else:
        _print(result)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
