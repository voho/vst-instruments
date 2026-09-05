#!/usr/bin/env python3
"""Fit one measured steel-string bridge; never select against guitar audio.

Source: Carcagno, Bucknall, Woodhouse, Fritz and Plack (2018),
https://doi.org/10.1121/1.5084735; CC-BY-4.0 data https://osf.io/f4pqa/.
Download guitar_back_wood_code_data_v1.0.1.zip and extract
 datasets/bridge_admittance_all.mat (SHA-256 verified below).

The first, predeclared specSet column is the Brazilian-rosewood instrument:
a custom Fylde Falstaff, Sitka spruce top, ebony bridge, Elixir Nanoweb Light
80/20 Bronze steel strings. Manufacture year is not specified. The experiment
measured normal driving-point velocity/force in m/s/N between strings 5 and 6,
with damped strings, foam at the end button and a foam-lined neck clamp.
There is no measured cross-admittance, rocking response or microphone transfer.
A scalar bank applied to all six strings is an approximation, and combining it
with another instrument's radiation does not make a measured Fylde body model.
The fitted 65.9-Hz peak may include support motion; the data do not separate it.

Reuse the Mores generator's 47 most prominent measured peaks. Pin the first
three body frequencies/Qs to this paper's Table I (97/34, 177/18, 336/36).
Fit nonnegative scalar residues to complex measured mobility over 60 Hz-10 kHz.
Infer a polarity and bounded +/-1 ms phase delay by the same constrained
residual. Scan 81 equally spaced delays per polarity before refining the best
neighbouring interval; do not assume the full interval is unimodal. This is a
model alignment, not independent proof of an instrumentation delay. Preserve
the measured SI magnitude and keep non-passive delay out of the termination.
No audio corpus, amplitude normalization or listening choice enters the fit.

    python3 Tools/GenerateMeasuredSteelBridge.py --raw-mat /path/to/bridge_admittance_all.mat
    python3 Tools/GenerateMeasuredSteelBridge.py --raw-mat /path/to/bridge_admittance_all.mat --check
    python3 Tools/GenerateMeasuredSteelBridge.py --self-test
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from scipy.io import loadmat
from scipy.optimize import minimize_scalar, nnls

from GenerateMeasuredBridge import candidate_modes, cpp_float

RAW_SHA256 = "c306e2c78e72e1526a931b16967dbc4f87c1bc7ecf2c928ff8d492a64e318288"
DEFAULT_OUTPUT = Path(__file__).resolve().parents[1] / "Source/DSP/MeasuredSteelBridgeData.h"


def passive_fit(frequency: np.ndarray, raw: np.ndarray, modes: np.ndarray) -> tuple:
    s = 2j * np.pi * frequency[:, None]
    omega = 2 * np.pi * modes[:, 0]
    basis = s / (s * s + omega / modes[:, 1] * s + omega * omega)
    real_basis = np.concatenate((basis.real, basis.imag))

    def solve(delay: float, polarity: int) -> tuple:
        target = polarity * raw * np.exp(2j * np.pi * frequency * delay)
        residues = nnls(real_basis, np.concatenate((target.real, target.imag)),
                        maxiter=10_000)[0]
        error = np.linalg.norm(basis @ residues - target) / np.linalg.norm(target)
        return float(error), residues, target

    choices = []
    for polarity in (-1, 1):
        grid = np.linspace(-.001, .001, 81)
        coarse = [(solve(delay, polarity)[0], float(delay), polarity)
                  for delay in grid]
        best = min(range(len(grid)), key=lambda i: coarse[i][0])
        result = minimize_scalar(lambda delay: solve(delay, polarity)[0],
                                 bounds=(grid[max(0, best - 1)],
                                         grid[min(len(grid) - 1, best + 1)]),
                                 method="bounded",
                                 options={"xatol": 1e-10})
        choices.append(coarse[best])
        choices.append((float(result.fun), float(result.x), polarity))
    _, delay, polarity = min(choices)
    error, residues, target = solve(delay, polarity)
    keep = residues > residues.max() * 1e-8
    model = basis[:, keep] @ residues[keep]
    return modes[keep], residues[keep], model, target, delay, polarity, error


def fit(path: Path) -> tuple[list, dict]:
    if hashlib.sha256(path.read_bytes()).hexdigest() != RAW_SHA256:
        raise ValueError("unexpected source SHA-256")
    data = loadmat(path, squeeze_me=True)
    frequency, raw = data["freqArrSet"], data["specSet"]
    if frequency.shape != (16385,) or raw.shape != (16385, 6):
        raise ValueError("unexpected source shape")
    raw = raw[:, 0]
    assert candidate_modes(frequency, raw) == candidate_modes(frequency, raw, 65)
    modes = np.array(candidate_modes(frequency, raw, 47))[:, :2]
    for f, q in ((97., 34.), (177., 18.), (336., 36.)):
        modes[np.argmin(abs(modes[:, 0] - f))] = f, q
    selected = (frequency >= 60) & (frequency <= 10000)
    frequency, raw = frequency[selected], raw[selected]
    modes, residues, model, target, delay, polarity, error = passive_fit(
        frequency, raw, modes)
    report = {"phase_polarity": polarity, "phase_advance_seconds": delay,
              "mode_count": len(modes)}
    for low, high in ((60, 200), (60, 500), (60, 10000), (1000, 10000)):
        band = (frequency >= low) & (frequency <= high)
        magnitude = abs(20 * np.log10(np.maximum(abs(model[band]), 1e-30)
                                    / np.maximum(abs(target[band]), 1e-30)))
        report[f"fit_{low}_{high}"] = {
            "complex_relative": float(np.linalg.norm(model[band] - target[band])
                                      / np.linalg.norm(target[band])),
            "median_magnitude_db": float(np.median(magnitude)),
            "p90_magnitude_db": float(np.percentile(magnitude, 90))}
    # Same overall fit gates as GenerateMeasuredBridge, without inventing an
    # unmeasured second driving point to satisfy its two-point assertions.
    if error > .24 or report["fit_60_10000"]["median_magnitude_db"] > 1.6:
        raise ValueError(f"passive fit missed its accuracy limits: {report}")
    if not np.all(residues > 0) or np.any(model.real < -1e-12):
        raise ValueError("non-passive fitted mobility")
    return [[float(f), float(q), float(r), 0., 0.]
            for (f, q), r in zip(modes, residues)], report


def header(rows: list, report: dict) -> str:
    summary = report["fit_60_10000"]
    prefix = f'''// Generated by Tools/GenerateMeasuredSteelBridge.py; do not hand-edit.
// Carcagno, Bucknall, Woodhouse, Fritz and Plack (2018),
// https://doi.org/10.1121/1.5084735; data https://osf.io/f4pqa/, CC BY 4.0.
// Adapted from the first specSet column: custom Fylde Falstaff, Sitka spruce
// top/Brazilian rosewood back and sides, ebony bridge, steel 80/20 strings.
// Manufacture year unspecified. See THIRD_PARTY_NOTICES.md.
// Measured normal velocity/force (m/s/N) between strings 5 and 6, strings
// damped. Scalar heave only: cross/rocking and radiation were not measured.
// The 65.9-Hz peak may include support motion. This is a bridge alternative,
// not a measured full-body or microphone model.
// Y(s) = sum heave*s/(s*s + omega/q*s + omega*omega), omega=2*pi*frequency.
// All residues positive: the mobility is passive at every string position.
// Fit 60 Hz-10 kHz: relative complex error {summary['complex_relative']:.6f},
// median magnitude error {summary['median_magnitude_db']:.6f} dB. Measured SI gain retained;
// Polarity {report['phase_polarity']} and phase advance {report['phase_advance_seconds'] * 1e6:.3f} us inferred by constrained fit;
// the data do not independently identify this as an instrumentation delay.
// Source MAT SHA-256: {RAW_SHA256}
#pragma once

// Include the selected bridge header defining MeasuredBridgeMode first.
#include <array>

namespace acustra::detail
{{
inline constexpr std::array<MeasuredBridgeMode, {len(rows)}> measuredFyldeBridgeModes {{{{
'''
    return prefix + "".join("    { " + ", ".join(map(cpp_float, row)) + " },\n"
                            for row in rows) + "}};\n} // namespace acustra::detail\n"


def self_test() -> None:
    frequency = np.linspace(60, 5000, 4000)
    modes = np.array([[97., 34.], [177., 18.], [336., 36.]])
    s = 2j * np.pi * frequency[:, None]
    w = 2 * np.pi * modes[:, 0]
    target = (s / (s*s + w/modes[:, 1]*s + w*w)) @ np.array([1., 5., 2.])
    for wanted_delay in (35e-6, 735e-6):
        raw = -target * np.exp(-2j * np.pi * frequency * wanted_delay)
        _, residues, model, _, delay, polarity, error = passive_fit(frequency, raw, modes)
        assert error < 1e-6 and polarity == -1 and abs(delay - wanted_delay) < 1e-9
        assert np.allclose(residues, [1., 5., 2.], rtol=1e-6)
        assert np.all(model.real >= 0)
    print("Measured steel bridge synthetic polarity/delay/passivity check passed")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raw-mat", type=Path)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if args.raw_mat is None:
        parser.error("--raw-mat is required")
    rows, report = fit(args.raw_mat)
    expected = header(rows, report)
    if args.check:
        if not args.output.exists() or args.output.read_text() != expected:
            raise SystemExit("measured steel bridge header differs; regenerate")
    else:
        args.output.write_text(expected)
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
