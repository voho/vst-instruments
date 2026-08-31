#!/usr/bin/env python3
"""Generate Acustra's passive g21 driving-point bridge mobility.

The input is Robert Mores' ``qualified_selected_impulses.mat``.  This tool
uses the treble bridge impact and the co-located treble accelerometer from
g21, converts acceleration/force to velocity/force exactly as the archive's
MATLAB script does (differentiate the hammer instead of integrating the
sensor), finds 65 measured modal candidates through 10 kHz, and projects their
residues onto the nonnegative cone.  Every emitted term is therefore positive
real, so the waveguide termination can use the passive reflectance construction
in Bank and Karjalainen, DAFx-10, Eq. (17):
https://www.dafx.de/paper-archive/2010/DAFx10/BankKarjalainen_DAFx10_P60.pdf

NumPy and SciPy are required.  Regenerate or verify with:

    python3 Tools/GenerateMeasuredBridge.py --raw-mat /path/to/qualified_selected_impulses.mat
    python3 Tools/GenerateMeasuredBridge.py --raw-mat /path/to/qualified_selected_impulses.mat --check
"""

from __future__ import annotations

import argparse
import difflib
import hashlib
from pathlib import Path
import sys

import numpy as np
from scipy.io import loadmat
from scipy.ndimage import gaussian_filter1d
from scipy.optimize import nnls
from scipy.signal import find_peaks


SAMPLE_RATE = 48_000.0
FFT_SIZE = 65_536
RECORD_SAMPLES = 48_000
GUITAR_INDEX = 20  # MATLAB g21.
IMPACT_INDEX = 2  # Treble-side bridge impact.
FORCE_CHANNEL = 0
TREBLE_ACCELEROMETER_CHANNEL = 1
MINIMUM_FREQUENCY = 60.0
MAXIMUM_FREQUENCY = 10_000.0
# This is the smallest prominence pool that retains 50 positive-real modes
# from g21. The DAFx-26 DeVoe model uses 50--200 bridge modes.
CANDIDATE_COUNT = 65
PEAK_PROMINENCE_DB = 0.5
Q_MINIMUM = 2.0
Q_MAXIMUM = 80.0
RAW_MD5 = "733cb10baf5ce36d8bf333610ffbb260"
HAMMER_NEWTONS_PER_FULL_SCALE = (10_000.0 / 92.90) * 4.4482
ACCELERATION_MPS2_PER_FULL_SCALE = (10_000.0 / 10.64) * 9.80665
DEFAULT_OUTPUT = (
    Path(__file__).resolve().parents[1] / "Source" / "DSP" / "MeasuredBridgeData.h"
)


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "md5").hexdigest()


def extract_mobility(path: Path) -> tuple[np.ndarray, np.ndarray]:
    actual_digest = digest(path)
    if actual_digest != RAW_MD5:
        raise ValueError(f"{path}: MD5 {actual_digest}, expected {RAW_MD5}")

    values = loadmat(path, variable_names=["qualified_selected_impulses"])[
        "qualified_selected_impulses"
    ]
    if values.shape != (65, 144_000, 6):
        raise ValueError(f"{path}: unexpected source matrix shape {values.shape}")

    start = IMPACT_INDEX * RECORD_SAMPLES
    record = values[GUITAR_INDEX, start : start + RECORD_SAMPLES, :]
    if not np.all(np.isfinite(record)):
        raise ValueError(f"{path}: g21 treble-impact record contains non-finite data")

    taper = 0.5 * np.cos(
        np.arange(1, RECORD_SAMPLES + 1) * np.pi / RECORD_SAMPLES
    ) + 0.5
    force = record[:, FORCE_CHANNEL] * taper * HAMMER_NEWTONS_PER_FULL_SCALE
    differentiated_force = np.diff(force) * SAMPLE_RATE
    force_spectrum = np.fft.rfft(
        np.pad(differentiated_force, (0, FFT_SIZE - len(differentiated_force)))
    )
    acceleration = (
        record[:, TREBLE_ACCELEROMETER_CHANNEL]
        * taper
        * ACCELERATION_MPS2_PER_FULL_SCALE
    )
    acceleration_spectrum = np.fft.rfft(acceleration, FFT_SIZE)
    denominator = np.abs(force_spectrum) ** 2
    denominator += np.max(denominator) * 1.0e-14
    mobility = acceleration_spectrum * np.conj(force_spectrum) / denominator
    frequency = np.fft.rfftfreq(FFT_SIZE, 1.0 / SAMPLE_RATE)
    return frequency, mobility


def crossing(
    frequency: np.ndarray,
    magnitude_db: np.ndarray,
    peak: int,
    direction: int,
) -> float:
    threshold = magnitude_db[peak] - 3.0
    limit = frequency[peak] * (0.85 if direction < 0 else 1.15)
    index = peak
    while 0 <= index + direction < len(magnitude_db):
        following = index + direction
        if direction < 0 and frequency[following] < limit:
            break
        if direction > 0 and frequency[following] > limit:
            break
        if magnitude_db[following] <= threshold:
            x0, x1 = frequency[index], frequency[following]
            y0, y1 = magnitude_db[index], magnitude_db[following]
            if y1 == y0:
                return float(x1)
            return float(x0 + (threshold - y0) * (x1 - x0) / (y1 - y0))
        index = following
    return float(limit)


def candidate_modes(
    frequency: np.ndarray, mobility: np.ndarray
) -> list[tuple[float, float, float]]:
    useful = np.flatnonzero(
        (frequency >= MINIMUM_FREQUENCY) & (frequency <= MAXIMUM_FREQUENCY)
    )
    magnitude_db = gaussian_filter1d(
        20.0 * np.log10(np.maximum(np.abs(mobility), 1.0e-30)), 1.5
    )
    peaks, properties = find_peaks(
        magnitude_db[useful], prominence=PEAK_PROMINENCE_DB, distance=3
    )
    modes: list[tuple[float, float, float]] = []
    for local_peak, prominence in zip(peaks, properties["prominences"]):
        peak = int(useful[local_peak])
        left = crossing(frequency, magnitude_db, peak, -1)
        right = crossing(frequency, magnitude_db, peak, 1)
        q = np.clip(
            frequency[peak] / max(right - left, frequency[1]),
            Q_MINIMUM,
            Q_MAXIMUM,
        )
        modes.append((float(frequency[peak]), float(q), float(prominence)))

    modes.sort(key=lambda item: (-item[2], item[0]))
    if len(modes) < CANDIDATE_COUNT:
        raise ValueError(
            f"only {len(modes)} modal candidates, expected {CANDIDATE_COUNT}"
        )
    return sorted(modes[:CANDIDATE_COUNT], key=lambda item: item[0])


def positive_real_fit(
    frequency: np.ndarray,
    mobility: np.ndarray,
    modes: list[tuple[float, float, float]],
) -> tuple[list[tuple[float, float, float]], int, float, float]:
    fit_indices = np.flatnonzero(
        (frequency >= MINIMUM_FREQUENCY) & (frequency <= MAXIMUM_FREQUENCY)
    )[::3]
    fit_frequency = frequency[fit_indices]

    # Evaluate the continuous mobility resonators through the same bilinear
    # frequency map used by the runtime.  Positive weights make their sum PR.
    s = 2j * SAMPLE_RATE * np.tan(np.pi * fit_frequency / SAMPLE_RATE)
    columns: list[np.ndarray] = []
    for mode_frequency, q, _ in modes:
        omega = 2.0 * SAMPLE_RATE * np.tan(
            np.pi * mode_frequency / SAMPLE_RATE
        )
        damping = omega / (2.0 * q)
        columns.append(s / (s * s + 2.0 * damping * s + omega * omega))
    basis = np.column_stack(columns)
    matrix = np.vstack((basis.real, basis.imag))

    best: tuple[float, int, np.ndarray, np.ndarray] | None = None
    # The accelerometer peak follows the differentiated hammer by two samples.
    # Select the integer alignment by the constrained complex residual rather
    # than embedding a non-passive measurement delay in the termination.
    for phase_advance in range(-4, 9):
        target = mobility[fit_indices] * np.exp(
            2j * np.pi * fit_frequency * phase_advance / SAMPLE_RATE
        )
        weights, _ = nnls(matrix, np.concatenate((target.real, target.imag)))
        relative_error = float(
            np.linalg.norm(basis @ weights - target) / np.linalg.norm(target)
        )
        if best is None or relative_error < best[0]:
            best = (relative_error, phase_advance, weights, target)
    assert best is not None

    relative_error, phase_advance, weights, target = best
    keep = weights > np.max(weights) * 1.0e-8
    fitted_modes = [
        (mode[0], mode[1], float(weight))
        for mode, weight, retained in zip(modes, weights, keep)
        if retained
    ]
    fitted = basis[:, keep] @ weights[keep]
    magnitude_error_db = np.abs(
        20.0
        * np.log10(
            np.maximum(np.abs(fitted), 1.0e-30)
            / np.maximum(np.abs(target), 1.0e-30)
        )
    )
    median_magnitude_error = float(np.median(magnitude_error_db))
    if phase_advance != 2:
        raise ValueError(f"unexpected fitted instrumentation delay {phase_advance}")
    if relative_error > 0.24 or median_magnitude_error > 1.6:
        raise ValueError(
            "passive bridge fit missed its regression limits: "
            f"complex={relative_error:.6f}, magnitude={median_magnitude_error:.6f} dB"
        )
    if any(weight < 0.0 for _, _, weight in fitted_modes):
        raise ValueError("passive bridge fit contains a negative modal weight")
    return fitted_modes, phase_advance, relative_error, median_magnitude_error


def cpp_float(value: float) -> str:
    text = format(float(np.float32(value)), ".9g")
    if "." not in text and "e" not in text:
        text += ".0"
    return text + "f"


def render_header(
    modes: list[tuple[float, float, float]],
    phase_advance: int,
    relative_error: float,
    median_magnitude_error: float,
) -> str:
    rows = [
        "    { " + ", ".join(cpp_float(value) for value in mode) + " },"
        for mode in modes
    ]
    return f'''// Generated by Tools/GenerateMeasuredBridge.py; do not hand-edit.
// Passive driving-point mobility fitted to g21's treble-side bridge impact and
// co-located treble accelerometer. Acceleration/force is converted to
// velocity/force by differentiating the hammer, following the archive script.
// A nonnegative least-squares projection retained {len(modes)} of
// {CANDIDATE_COUNT} measured modal candidates after a {phase_advance}-sample
// instrumentation alignment;
// relative complex error {relative_error:.6f}, median magnitude error
// {median_magnitude_error:.6f} dB over 60--10000 Hz. Each positive weight
// multiplies s/(s^2 + 2 damping s + omega^2), making the sum positive real.
// Adapted from Robert Mores, "Archive for the acoustical documentation of
// classical Spanish guitars, flamenco guitars and romantic guitars from
// private and public collections -- bridge mobility" (2021),
// https://doi.org/10.5281/zenodo.4604577, licensed CC BY 4.0; see
// THIRD_PARTY_NOTICES.md.

#pragma once

#include <array>

namespace acustra::detail
{{
struct MeasuredBridgeMode
{{
    float frequency;
    float q;
    float weight;
}};

inline constexpr std::array<MeasuredBridgeMode, {len(modes)}> measuredBridgeModes {{{{
{chr(10).join(rows)}
}}}};
}} // namespace acustra::detail
'''


def check_output(path: Path, expected: str) -> bool:
    try:
        actual = path.read_text(encoding="utf-8")
    except FileNotFoundError:
        print(f"error: generated header does not exist: {path}", file=sys.stderr)
        return False
    if actual == expected:
        print(f"verified generated header: {path}")
        return True
    sys.stderr.writelines(
        difflib.unified_diff(
            actual.splitlines(keepends=True),
            expected.splitlines(keepends=True),
            fromfile=str(path),
            tofile="generated",
        )
    )
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raw-mat", required=True, type=Path)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument(
        "--check",
        action="store_true",
        help="compare generated content with --output without writing it",
    )
    arguments = parser.parse_args()

    frequency, mobility = extract_mobility(arguments.raw_mat)
    modes = candidate_modes(frequency, mobility)
    fitted, phase_advance, relative_error, magnitude_error = positive_real_fit(
        frequency, mobility, modes
    )
    header = render_header(
        fitted, phase_advance, relative_error, magnitude_error
    )
    if arguments.check:
        return 0 if check_output(arguments.output, header) else 1
    with arguments.output.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write(header)
    print(
        f"generated {arguments.output}: {len(fitted)} passive modes, "
        f"median magnitude error {magnitude_error:.3f} dB"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
