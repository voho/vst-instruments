#!/usr/bin/env python3
"""Generate Acustra's g21 dual-radiation modal coefficient header.

This is the complete deterministic path from Robert Mores'
``qualified_selected_impulses.mat`` to ``MeasuredBodyData.h``. It selects the
treble-side bridge impact on guitar g21, estimates calibrated force-to-pressure
H1 responses for the treble and bass microphones, reconstructs minimum phase,
selects 96 shared pole pairs, and fits one complex residue per output and pole.
The generated model must also remain within fixed complex, magnitude and stereo
balance regression limits; finiteness alone is not accepted.

NumPy and SciPy are required. Regenerate or check the committed header with:

    python3 Tools/GenerateMeasuredBody.py --raw-mat /path/to/qualified_selected_impulses.mat
    python3 Tools/GenerateMeasuredBody.py --raw-mat /path/to/qualified_selected_impulses.mat --check

DAFx-26 specifies a 3000-sample truncated impulse with a raised-cosine
fade-out, but does not publish the fade's inner length. Acustra authors the
final 300 samples (6.25 ms, 10 percent of the retained response) as the fade.
Robert Mores' archive script separately suggests a 240-sample microphone taper,
explicitly noting that it was not used in the reference plots; it is supporting
scale evidence, not the source of Acustra's exact 300-sample choice.
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
from scipy.signal import find_peaks


SAMPLE_RATE = 48_000.0
FFT_SIZE = 65_536
RECORD_SAMPLES = 48_000
GUITAR_INDEX = 20  # MATLAB g21.
IMPACT_INDEX = 2  # Third one-second segment: treble-side bridge impact.
FORCE_CHANNEL = 0
TREBLE_MIC_CHANNEL = 4  # MATLAB channel 5.
BASS_MIC_CHANNEL = 5  # MATLAB channel 6.
KEEP_SAMPLES = 3_000
FADE_SAMPLES = 300  # Authored adaptation; DAFx-26 does not give this length.
MODE_COUNT = 96
MINIMUM_FREQUENCY = 80.0
MAXIMUM_FREQUENCY = 10_000.0
PEAK_PROMINENCE_DB = 0.5
Q_MINIMUM = 2.0
Q_MAXIMUM = 80.0
RIDGE = 1.0e-7
MAX_COMPLEX_RELATIVE_ERROR = 0.30
MAX_MEDIAN_MAGNITUDE_ERROR_DB = 1.25
MAX_P90_MAGNITUDE_ERROR_DB = 3.75
MAX_STEREO_RATIO_P90_ERROR_DB = 5.0
RAW_MD5 = "733cb10baf5ce36d8bf333610ffbb260"
HAMMER_NEWTONS_PER_FULL_SCALE = (10_000.0 / 92.90) * 4.4482
MIC_PASCALS_PER_FULL_SCALE = 10_000.0 / 50.0
DEFAULT_OUTPUT = (
    Path(__file__).resolve().parents[1] / "Source" / "DSP" / "MeasuredBodyData.h"
)


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "md5").hexdigest()


def minimum_phase_from_magnitude(magnitude: np.ndarray) -> np.ndarray:
    floor = max(float(np.max(magnitude)) * 1.0e-12, 1.0e-30)
    log_half = np.log(np.maximum(magnitude, floor))
    log_full = np.concatenate((log_half, log_half[-2:0:-1]))
    cepstrum = np.fft.ifft(log_full).real
    causal = np.zeros_like(cepstrum)
    causal[0] = cepstrum[0]
    causal[1 : FFT_SIZE // 2] = 2.0 * cepstrum[1 : FFT_SIZE // 2]
    causal[FFT_SIZE // 2] = cepstrum[FFT_SIZE // 2]
    return np.exp(np.fft.fft(causal))[: FFT_SIZE // 2 + 1]


def extract_targets(path: Path) -> list[np.ndarray]:
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

    full_taper = 0.5 * np.cos(
        np.arange(1, RECORD_SAMPLES + 1) * np.pi / RECORD_SAMPLES
    ) + 0.5
    hammer = (
        record[:, FORCE_CHANNEL]
        * full_taper
        * HAMMER_NEWTONS_PER_FULL_SCALE
    )
    force = np.fft.rfft(hammer, FFT_SIZE)
    denominator = np.abs(force) ** 2
    denominator += np.max(denominator) * 1.0e-12

    impulse_taper = np.ones(KEEP_SAMPLES)
    impulse_taper[-FADE_SAMPLES:] = 0.5 + 0.5 * np.cos(
        np.linspace(0.0, np.pi, FADE_SAMPLES)
    )

    targets: list[np.ndarray] = []
    for channel in (TREBLE_MIC_CHANNEL, BASS_MIC_CHANNEL):
        pressure = record[:, channel] * full_taper * MIC_PASCALS_PER_FULL_SCALE
        spectrum = np.fft.rfft(pressure, FFT_SIZE)
        h1 = spectrum * np.conj(force) / denominator
        impulse = np.fft.irfft(h1, FFT_SIZE)
        windowed = np.zeros(FFT_SIZE)
        windowed[:KEEP_SAMPLES] = impulse[:KEEP_SAMPLES] * impulse_taper
        magnitude = np.abs(np.fft.rfft(windowed))
        targets.append(minimum_phase_from_magnitude(magnitude))
    return targets


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


def candidate_poles(targets: list[np.ndarray]) -> list[tuple[float, float, float]]:
    frequency = np.fft.rfftfreq(FFT_SIZE, 1.0 / SAMPLE_RATE)
    useful_indices = np.flatnonzero(
        (frequency >= MINIMUM_FREQUENCY) & (frequency <= MAXIMUM_FREQUENCY)
    )
    bin_width = frequency[1]
    candidates: list[tuple[float, float, float]] = []
    for target in targets:
        magnitude_db = 20.0 * np.log10(np.maximum(np.abs(target), 1.0e-30))
        smoothed = gaussian_filter1d(magnitude_db, 1.5)
        peaks, properties = find_peaks(
            smoothed[useful_indices],
            prominence=PEAK_PROMINENCE_DB,
            distance=max(1, int(round(2.0 / bin_width))),
        )
        for local_peak, prominence in zip(peaks, properties["prominences"]):
            peak = int(useful_indices[local_peak])
            left = crossing(frequency, smoothed, peak, -1)
            right = crossing(frequency, smoothed, peak, 1)
            q = np.clip(
                frequency[peak] / max(right - left, bin_width),
                Q_MINIMUM,
                Q_MAXIMUM,
            )
            candidates.append(
                (float(frequency[peak]), float(q), float(prominence))
            )

    candidates.sort(key=lambda item: item[0])
    merged: list[list[tuple[float, float, float]]] = []
    for item in candidates:
        if merged:
            group = merged[-1]
            weights = [member[2] for member in group]
            group_frequency = float(
                np.average([member[0] for member in group], weights=weights)
            )
            group_bandwidth = float(
                np.average(
                    [member[0] / member[1] for member in group], weights=weights
                )
            )
            item_bandwidth = item[0] / item[1]
            tolerance = max(2.0, 0.5 * min(group_bandwidth, item_bandwidth))
        else:
            group_frequency = -1.0
            tolerance = 0.0
        if merged and abs(item[0] - group_frequency) <= tolerance:
            merged[-1].append(item)
        else:
            merged.append([item])

    result: list[tuple[float, float, float]] = []
    for group in merged:
        weights = np.array([item[2] for item in group])
        result.append(
            (
                float(np.average([item[0] for item in group], weights=weights)),
                float(
                    np.exp(
                        np.average(
                            np.log([item[1] for item in group]), weights=weights
                        )
                    )
                ),
                float(np.sum(weights)),
            )
        )
    return result


def fit_residues(
    target: np.ndarray, modes: list[tuple[float, float, float]]
) -> np.ndarray:
    all_frequency = np.fft.rfftfreq(FFT_SIZE, 1.0 / SAMPLE_RATE)
    fit_indices = np.flatnonzero(
        (all_frequency >= MINIMUM_FREQUENCY)
        & (all_frequency <= MAXIMUM_FREQUENCY)
    )
    # Nearest FFT-bin stride to 4 Hz: five bins = 3.662109375 Hz.
    stride = max(1, int(round(4.0 / (all_frequency[1] - all_frequency[0]))))
    fit_indices = fit_indices[::stride]
    frequency = all_frequency[fit_indices]
    z_inverse = np.exp(-2j * np.pi * frequency / SAMPLE_RATE)
    columns: list[np.ndarray] = []
    for mode_frequency, q, _ in modes:
        pole = np.exp(-np.pi * mode_frequency / (q * SAMPLE_RATE)) * np.exp(
            2j * np.pi * mode_frequency / SAMPLE_RATE
        )
        positive = 1.0 / (1.0 - pole * z_inverse)
        negative = 1.0 / (1.0 - np.conj(pole) * z_inverse)
        columns.extend((positive + negative, 1j * (positive - negative)))
    basis = np.column_stack(columns)
    desired = target[fit_indices]
    envelope = gaussian_filter1d(np.abs(target), 128)[fit_indices]
    weight = 1.0 / np.maximum(envelope, np.max(envelope) * 1.0e-3)
    weighted_basis = basis * weight[:, None]
    weighted_desired = desired * weight
    matrix = np.vstack((weighted_basis.real, weighted_basis.imag))
    vector = np.concatenate((weighted_desired.real, weighted_desired.imag))
    normal = matrix.T @ matrix
    penalty = RIDGE * np.trace(normal) / normal.shape[0]
    solution = np.linalg.solve(
        normal + penalty * np.eye(normal.shape[0]), matrix.T @ vector
    )
    return solution[0::2] + 1j * solution[1::2]


def validate_fit(
    targets: list[np.ndarray],
    modes: list[tuple[float, float, float]],
    residues: list[np.ndarray],
) -> None:
    frequency = np.fft.rfftfreq(FFT_SIZE, 1.0 / SAMPLE_RATE)
    indices = np.flatnonzero(
        (frequency >= MINIMUM_FREQUENCY) & (frequency <= MAXIMUM_FREQUENCY)
    )
    z_inverse = np.exp(-2j * np.pi * frequency[indices] / SAMPLE_RATE)
    models: list[np.ndarray] = []
    for channel_residues in residues:
        model = np.zeros(len(indices), dtype=complex)
        for (mode_frequency, q, _), residue in zip(modes, channel_residues):
            pole = np.exp(-np.pi * mode_frequency / (q * SAMPLE_RATE)) * np.exp(
                2j * np.pi * mode_frequency / SAMPLE_RATE
            )
            positive = 1.0 / (1.0 - pole * z_inverse)
            negative = 1.0 / (1.0 - np.conj(pole) * z_inverse)
            model += residue * positive + np.conj(residue) * negative
        models.append(model)

    errors: list[tuple[float, float, float]] = []
    for target, model in zip(targets, models):
        desired = target[indices]
        complex_error = float(
            np.linalg.norm(model - desired) / np.linalg.norm(desired)
        )
        magnitude_error_db = np.abs(
            20.0
            * np.log10(
                np.maximum(np.abs(model), 1.0e-30)
                / np.maximum(np.abs(desired), 1.0e-30)
            )
        )
        errors.append(
            (
                complex_error,
                float(np.median(magnitude_error_db)),
                float(np.percentile(magnitude_error_db, 90.0)),
            )
        )

    desired_ratio_db = 20.0 * np.log10(
        np.maximum(np.abs(targets[0][indices]), 1.0e-30)
        / np.maximum(np.abs(targets[1][indices]), 1.0e-30)
    )
    model_ratio_db = 20.0 * np.log10(
        np.maximum(np.abs(models[0]), 1.0e-30)
        / np.maximum(np.abs(models[1]), 1.0e-30)
    )
    stereo_ratio_p90_error_db = float(
        np.percentile(np.abs(model_ratio_db - desired_ratio_db), 90.0)
    )
    if (
        any(error[0] > MAX_COMPLEX_RELATIVE_ERROR for error in errors)
        or any(error[1] > MAX_MEDIAN_MAGNITUDE_ERROR_DB for error in errors)
        or any(error[2] > MAX_P90_MAGNITUDE_ERROR_DB for error in errors)
        or stereo_ratio_p90_error_db > MAX_STEREO_RATIO_P90_ERROR_DB
    ):
        formatted = ", ".join(
            f"channel {index}: complex={complex_error:.6f}, "
            f"median={median_error:.6f} dB, p90={p90_error:.6f} dB"
            for index, (complex_error, median_error, p90_error) in enumerate(errors)
        )
        raise ValueError(
            "measured body fit missed its regression limits: "
            f"{formatted}, stereo-ratio p90="
            f"{stereo_ratio_p90_error_db:.6f} dB"
        )


def modal_fit(path: Path) -> tuple[list[tuple[float, float, float]], list[np.ndarray]]:
    targets = extract_targets(path)
    candidates = candidate_poles(targets)
    candidates.sort(key=lambda item: (-item[2], item[0]))
    if len(candidates) < MODE_COUNT:
        raise ValueError(
            f"only {len(candidates)} shared pole candidates, expected {MODE_COUNT}"
        )
    modes = sorted(candidates[:MODE_COUNT], key=lambda item: item[0])
    residues = [fit_residues(target, modes) for target in targets]
    values = np.concatenate(
        (
            np.array([(mode[0], mode[1]) for mode in modes]).ravel(),
            residues[0].view(np.float64),
            residues[1].view(np.float64),
        )
    )
    if not np.all(np.isfinite(values)):
        raise ValueError("modal fit contains non-finite values")
    validate_fit(targets, modes, residues)
    return modes, residues


def cpp_float(value: float) -> str:
    text = format(float(np.float32(value)), ".9g")
    if "." not in text and "e" not in text:
        text += ".0"
    return text + "f"


def render_header(
    modes: list[tuple[float, float, float]], residues: list[np.ndarray]
) -> str:
    rows = []
    for mode, treble, bass in zip(modes, residues[0], residues[1]):
        values = (
            mode[0],
            mode[1],
            treble.real,
            treble.imag,
            bass.real,
            bass.imag,
        )
        rows.append("    { " + ", ".join(cpp_float(value) for value in values) + " },")

    return f'''// Generated by Tools/GenerateMeasuredBody.py; do not hand-edit.
// Individual-guitar target: g21, a 2018 Lester DeVoe flamenca blanca with a
// spruce top and cypress back/sides. The treble-side bridge impact drives two
// calibrated H1 paths: left is the treble microphone, right the bass microphone.
// Each 3000-sample causal response is converted independently to minimum phase.
// DAFx-26 specifies the retained length and raised-cosine taper, but not its
// inner length; Acustra authors the final 300 samples as that fade. The 96
// shared frequency/Q pairs and independent complex residues are an authored
// regularised fit, not coefficients published by DAFx-26.
// Adapted from Robert Mores, "Archive for the acoustical documentation of
// classical Spanish guitars, flamenco guitars and romantic guitars from
// private and public collections -- bridge mobility" (2021),
// https://doi.org/10.5281/zenodo.4604577, licensed CC BY 4.0; see
// THIRD_PARTY_NOTICES.md.

#pragma once

#include <array>

namespace acustra::detail
{{
struct MeasuredBodyMode
{{
    float frequency;
    float q;
    float leftReal;
    float leftImaginary;
    float rightReal;
    float rightImaginary;
}};

inline constexpr std::array<MeasuredBodyMode, {MODE_COUNT}> measuredBodyModes {{{{
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

    modes, residues = modal_fit(arguments.raw_mat)
    header = render_header(modes, residues)
    if arguments.check:
        return 0 if check_output(arguments.output, header) else 1
    with arguments.output.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write(header)
    print(f"generated {arguments.output}: {MODE_COUNT} shared modes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
