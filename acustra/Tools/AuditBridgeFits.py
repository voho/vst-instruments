#!/usr/bin/env python3
"""Compare passive bridge-fit elbows without changing the shipping header."""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
from scipy.optimize import nnls

import GenerateMeasuredBridge as bridge


def fit(frequency: np.ndarray, mobility: np.ndarray, candidate_count: int,
        maximum_frequency: float):
    bridge.CANDIDATE_COUNT = candidate_count
    bridge.MAXIMUM_FREQUENCY = maximum_frequency
    bridge.PEAK_PROMINENCE_DB = 0.25 if candidate_count >= 96 else 0.5
    candidates = bridge.candidate_modes(frequency, mobility)
    indices = np.flatnonzero(
        (frequency >= bridge.MINIMUM_FREQUENCY)
        & (frequency <= maximum_frequency))[::3]
    fitted_frequency = frequency[indices]
    s = 2j * bridge.SAMPLE_RATE * np.tan(
        np.pi * fitted_frequency / bridge.SAMPLE_RATE)
    columns = []
    for mode_frequency, q, _ in candidates:
        omega = 2.0 * bridge.SAMPLE_RATE * np.tan(
            np.pi * mode_frequency / bridge.SAMPLE_RATE)
        damping = omega / (2.0 * q)
        columns.append(s / (s * s + 2.0 * damping * s + omega * omega))
    basis = np.column_stack(columns)
    matrix = np.vstack((basis.real, basis.imag))
    best = None
    for advance in range(-4, 9):
        target = mobility[indices] * np.exp(
            2j * np.pi * fitted_frequency * advance / bridge.SAMPLE_RATE)
        weights, _ = nnls(matrix, np.concatenate((target.real, target.imag)))
        model = basis @ weights
        error = np.linalg.norm(model - target) / np.linalg.norm(target)
        if best is None or error < best[0]:
            best = (float(error), advance, weights, target, model)
    assert best is not None
    error, advance, weights, target, model = best
    keep = weights > np.max(weights) * 1.0e-8
    # This screen fits the treble driving point alone, so every retained mode
    # is heave-only in the two-point header's terms: a rigid bridge.
    retained = [(mode[0], mode[1], float(weight), 0.0, 0.0)
                for mode, weight, use in zip(candidates, weights, keep) if use]
    model = basis[:, keep] @ weights[keep]
    magnitude_error = np.abs(20.0 * np.log10(
        np.maximum(np.abs(model), 1.0e-30)
        / np.maximum(np.abs(target), 1.0e-30)))
    low = fitted_frequency <= 4200.0
    return {
        "candidates": candidate_count,
        "maximum": maximum_frequency,
        "retained": retained,
        "advance": advance,
        "complex": error,
        "median": float(np.median(magnitude_error)),
        "p90": float(np.percentile(magnitude_error, 90.0)),
        "low_complex": float(np.linalg.norm((model - target)[low])
                             / np.linalg.norm(target[low])),
        "low_median": float(np.median(magnitude_error[low])),
        "low_p90": float(np.percentile(magnitude_error[low], 90.0)),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raw-mat", required=True, type=Path)
    parser.add_argument("--output-directory", type=Path)
    arguments = parser.parse_args()
    frequency, mobility = bridge.extract_mobility(arguments.raw_mat)
    # Only the steel (g21) bank is screened here. Fit the nylon bank first,
    # while the module's candidate count and band are still the shipping ones,
    # so a written header stays the one the engine would compile.
    nylon = (bridge.fit_bank(arguments.raw_mat, bridge.DEFAULT_NYLON_GUITAR)
             if arguments.output_directory is not None else None)
    configurations = ((30, 4200.0), (48, 10_000.0),
                      (64, 10_000.0), (96, 10_000.0))
    for candidates, maximum in configurations:
        result = fit(frequency, mobility, candidates, maximum)
        print(
            f"{candidates:3d} candidates/{maximum/1000:.1f}k: "
            f"{len(result['retained']):3d} retained, phase "
            f"{result['advance']:+d}, complex {result['complex']:.5f}, "
            f"median/p90 {result['median']:.3f}/{result['p90']:.3f} dB; "
            f"60-4.2k complex {result['low_complex']:.5f}, "
            f"median/p90 {result['low_median']:.3f}/"
            f"{result['low_p90']:.3f} dB")
        if arguments.output_directory is not None:
            arguments.output_directory.mkdir(parents=True, exist_ok=True)
            header = bridge.render_header(
                {
                    "guitar": 21,
                    "modes": result["retained"],
                    "phase_advance": result["advance"],
                    "relative_error": result["complex"],
                    "magnitude_error": result["median"],
                    "corner": bridge.MINIMUM_FREQUENCY,
                    "rocking": 0,
                },
                nylon,
            )
            path = arguments.output_directory / (
                f"MeasuredBridgeData-{candidates}c-{len(result['retained'])}r.h")
            path.write_text(header, encoding="utf-8")
            print(f"  wrote {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
