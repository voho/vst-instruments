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

This exists because a single number cannot say where an attack differs. A
spectral centroid can be dragged down by an excess at the bottom or a shortfall
at the top, and those call for opposite corrections; reading one as the other
sent an earlier investigation after the pluck's brightness when the difference
was in the body's low end. Bands separate the two.

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


def _read(path: Path, channels: int) -> np.ndarray:
    data = np.fromfile(path, dtype="<f4")
    if channels == 2:
        data = data.reshape(-1, 2).mean(axis=1)
    return data.astype(np.float64)


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
    peak = float(np.max(np.abs(signal))) if signal.size else 0.0
    if peak <= 0.0:
        return None
    onset = int(np.argmax(np.abs(signal) > ONSET_FRACTION * peak))
    length = round(ATTACK_SECONDS * rate)
    first = _shape_once(signal, rate, onset, length)
    second = _shape_once(signal, rate, onset, round(1.05 * length))
    if first is None or second is None:
        return None
    unstable = np.abs(first - second) > STABILITY_TOLERANCE_DB
    return np.where(unstable, np.nan, first)


def collect(directory: Path) -> dict[str, list[tuple[np.ndarray, np.ndarray]]]:
    pooled: dict[str, list[tuple[np.ndarray, np.ndarray]]] = {}
    for split in ("train", "validation"):
        manifest_path = directory / f"{split}.json"
        if not manifest_path.is_file():
            continue
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        for example in manifest["examples"]:
            pair = []
            for role in ("target", "model"):
                entry = example[role]
                pair.append(band_shape_db(
                    _read(directory / entry["path"], int(entry["channels"])),
                    int(entry["sample_rate"]),
                ))
            if pair[0] is None or pair[1] is None:
                continue
            pooled.setdefault(example["material"], []).append((pair[0], pair[1]))
    return pooled


def _median(values: np.ndarray) -> float | None:
    """None unless at least a third of the examples measured the band stably."""
    finite = values[np.isfinite(values)]
    if finite.size * 3 < values.size or finite.size == 0:
        return None
    return float(np.median(finite))


def report(pooled: dict[str, list[tuple[np.ndarray, np.ndarray]]]) -> dict:
    out: dict = {"band_edges_hz": BAND_EDGES.tolist(), "materials": {}}
    for material, rows in sorted(pooled.items()):
        recorded = np.asarray([a for a, _ in rows])
        modelled = np.asarray([b for _, b in rows])
        out["materials"][material] = {
            "examples": len(rows),
            "bands": [
                {
                    "low_hz": float(low), "high_hz": float(high),
                    "recording_db": _median(recorded[:, index]),
                    "model_db": _median(modelled[:, index]),
                    "difference_db": (
                        None if _median(modelled[:, index]) is None
                        or _median(recorded[:, index]) is None
                        else _median(modelled[:, index])
                        - _median(recorded[:, index])),
                    "stable_examples": int(np.count_nonzero(
                        np.isfinite(recorded[:, index])
                        & np.isfinite(modelled[:, index]))),
                }
                for index, (low, high)
                in enumerate(zip(BAND_EDGES[:-1], BAND_EDGES[1:]))
            ],
        }
    return out


def _print(result: dict) -> None:
    for material, node in result["materials"].items():
        print(f"== {material}   attack band shape, first "
              f"{ATTACK_SECONDS * 1000:.0f} ms, n={node['examples']}")
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


def self_test() -> int:
    """A tone moved one octave up must move its energy one band up, and level
    alone must leave the shape untouched."""
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
    print(f"self-test passed: shape is level-invariant, bands track pitch, and "
          f"a pure tone reports only {int(np.isfinite(a).sum())} of "
          f"{a.size} bands")
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
