#!/usr/bin/env python3
"""Generate Acustra's two passive driving-point bridge mobilities.

The input is Robert Mores' ``qualified_selected_impulses.mat``.  For each
guitar this tool uses the treble bridge impact and the co-located treble
accelerometer, converts acceleration/force to velocity/force exactly as the
archive's MATLAB script does (differentiate the hammer instead of integrating
the sensor), finds 65 measured modal candidates through 10 kHz, and projects
their residues onto the nonnegative cone.  Every emitted term is therefore
positive real, so the waveguide termination can use the passive reflectance
construction in Bank and Karjalainen, DAFx-10, Eq. (17):
https://www.dafx.de/paper-archive/2010/DAFx10/BankKarjalainen_DAFx10_P60.pdf

Two guitars are emitted, one per string material, because a steel-string and a
classical are different instruments and neither bridge describes the other.
Both fits must meet the same relative-complex and median-magnitude limits;
those limits were pinned on the g21 fit and are not relaxed for a candidate.

NumPy and SciPy are required.  Regenerate or verify with:

    python3 Tools/GenerateMeasuredBridge.py --raw-mat /path/to/qualified_selected_impulses.mat
    python3 Tools/GenerateMeasuredBridge.py --raw-mat /path/to/qualified_selected_impulses.mat --check

``--nylon-guitar`` selects a different archive record for the nylon bank; it
exists so the choice between measured classicals can be screened, and the
committed header is the default.
"""

from __future__ import annotations

import argparse
import difflib
import hashlib
from pathlib import Path
import sys
import textwrap

import numpy as np
from scipy.io import loadmat
from scipy.ndimage import gaussian_filter1d
from scipy.optimize import nnls
from scipy.signal import find_peaks


SAMPLE_RATE = 48_000.0
FFT_SIZE = 65_536
RECORD_SAMPLES = 48_000
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
# Standard tuning at A = 440 Hz; the guitars were measured strung.
OPEN_STRING_HZ = (82.40689, 110.0, 146.83238, 195.99772, 246.94165, 329.62756)
MAX_COMPLEX_RELATIVE_ERROR = 0.24
MAX_MEDIAN_MAGNITUDE_ERROR_DB = 1.6
RAW_MD5 = "733cb10baf5ce36d8bf333610ffbb260"
HAMMER_NEWTONS_PER_FULL_SCALE = (10_000.0 / 92.90) * 4.4482
ACCELERATION_MPS2_PER_FULL_SCALE = (10_000.0 / 10.64) * 9.80665
DEFAULT_NYLON_GUITAR = 34
# Provenance from the archive's List_of_guitars_description.pdf.
GUITAR_DESCRIPTION = {
    21: "a 2018 Lester DeVoe flamenca blanca, spruce/cypress, measured in a "
        "school music room in Freiburg",
    34: "a 1971 Manuel Contreras classical Spanish, cedar/Rio palisander, "
        "measured anechoic in the class-1 free-field laboratory of the "
        "Hamburg University of Applied Sciences",
    35: "a 1978 Manuel Lopez Bellido classical Spanish, cedar/Rio palisander, "
        "measured anechoic in the class-1 free-field laboratory of the "
        "Hamburg University of Applied Sciences",
    36: "a 2001 Jose Lopez Bellido classical Spanish, spruce/Indian "
        "palisander, measured anechoic in the class-1 free-field laboratory "
        "of the Hamburg University of Applied Sciences",
}
DEFAULT_OUTPUT = (
    Path(__file__).resolve().parents[1] / "Source" / "DSP" / "MeasuredBridgeData.h"
)


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "md5").hexdigest()


def load_matrix(path: Path) -> np.ndarray:
    actual_digest = digest(path)
    if actual_digest != RAW_MD5:
        raise ValueError(f"{path}: MD5 {actual_digest}, expected {RAW_MD5}")

    values = loadmat(path, variable_names=["qualified_selected_impulses"])[
        "qualified_selected_impulses"
    ]
    if values.shape != (65, 144_000, 6):
        raise ValueError(f"{path}: unexpected source matrix shape {values.shape}")
    return values


def extract_mobility(
    path: Path, guitar: int = 21
) -> tuple[np.ndarray, np.ndarray]:
    values = load_matrix(path)
    start = IMPACT_INDEX * RECORD_SAMPLES
    record = values[guitar - 1, start : start + RECORD_SAMPLES, :]
    if not np.all(np.isfinite(record)):
        raise ValueError(f"{path}: g{guitar} treble-impact record is non-finite")

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
    if (relative_error > MAX_COMPLEX_RELATIVE_ERROR
            or median_magnitude_error > MAX_MEDIAN_MAGNITUDE_ERROR_DB):
        raise ValueError(
            "passive bridge fit missed its regression limits: "
            f"complex={relative_error:.6f}, magnitude={median_magnitude_error:.6f} dB"
        )
    if any(weight < 0.0 for _, _, weight in fitted_modes):
        raise ValueError("passive bridge fit contains a negative modal weight")
    # The archive's setup photographs show installed, undamped strings, so a
    # retained mode sitting on an open string could be that string rather than
    # the body. A guitar's low air and top modes have Q in the tens while an
    # open string's is near a thousand, and this estimator clips at
    # Q_MAXIMUM = 80, so a mode near an open string is cleared only if its Q is
    # resolved below that clip. (2026-08-30: g21's one such candidate, 82.764
    # Hz, reads Q 16.6 and is a body mode.)
    for mode_frequency, q, _ in fitted_modes:
        for open_frequency in OPEN_STRING_HZ:
            cents = 1200.0 * np.log2(mode_frequency / open_frequency)
            if abs(cents) < 25.0 and q >= Q_MAXIMUM:
                raise ValueError(
                    f"retained mode {mode_frequency:.3f} Hz sits {cents:+.1f} "
                    f"cents from an open string with an unresolved Q {q:.1f}"
                )
    return fitted_modes, phase_advance, relative_error, median_magnitude_error


def fit_bank(path: Path, guitar: int) -> dict:
    frequency, mobility = extract_mobility(path, guitar)
    modes = candidate_modes(frequency, mobility)
    fitted, phase_advance, relative_error, magnitude_error = positive_real_fit(
        frequency, mobility, modes
    )
    return {
        "guitar": guitar,
        "modes": fitted,
        "phase_advance": phase_advance,
        "relative_error": relative_error,
        "magnitude_error": magnitude_error,
    }


def cpp_float(value: float) -> str:
    text = format(float(np.float32(value)), ".9g")
    if "." not in text and "e" not in text:
        text += ".0"
    return text + "f"


def bank_block(name: str, bank: dict) -> str:
    rows = [
        "    { " + ", ".join(cpp_float(value) for value in mode) + " },"
        for mode in bank["modes"]
    ]
    guitar = bank["guitar"]
    comment = textwrap.fill(
        f'g{guitar}, {GUITAR_DESCRIPTION[guitar]}. A nonnegative least-squares'
        f' projection retained {len(bank["modes"])} of {CANDIDATE_COUNT}'
        f' measured modal candidates after a {bank["phase_advance"]}-sample'
        f' instrumentation alignment; relative complex error'
        f' {bank["relative_error"]:.6f}, median magnitude error'
        f' {bank["magnitude_error"]:.6f} dB over 60--10000 Hz.',
        width=76, initial_indent="// ", subsequent_indent="// ")
    return f'''{comment}
inline constexpr std::array<MeasuredBridgeMode, {len(bank["modes"])}> {name} {{{{
{chr(10).join(rows)}
}}}};'''


def render_header(steel: dict, nylon: dict) -> str:
    return f'''// Generated by Tools/GenerateMeasuredBridge.py; do not hand-edit.
// Passive driving-point mobilities fitted to each guitar's treble-side bridge
// impact and co-located treble accelerometer. Acceleration/force is converted
// to velocity/force by differentiating the hammer, following the archive
// script. Each positive weight multiplies s/(s^2 + 2 damping s + omega^2),
// making the sum positive real. One bank per string material: a steel-string
// and a classical are different instruments, and the engine selects by
// StringMaterial.
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

{bank_block("measuredSteelBridgeModes", steel)}

{bank_block("measuredNylonBridgeModes", nylon)}
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
        "--nylon-guitar", type=int, default=DEFAULT_NYLON_GUITAR,
        choices=sorted(GUITAR_DESCRIPTION),
        help="archive record for the nylon bank (default: %(default)s)",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="compare generated content with --output without writing it",
    )
    arguments = parser.parse_args()

    steel = fit_bank(arguments.raw_mat, 21)
    nylon = fit_bank(arguments.raw_mat, arguments.nylon_guitar)
    header = render_header(steel, nylon)
    if arguments.check:
        return 0 if check_output(arguments.output, header) else 1
    with arguments.output.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write(header)
    for name, bank in (("steel", steel), ("nylon", nylon)):
        print(
            f"generated {name} bank g{bank['guitar']}: {len(bank['modes'])} "
            f"passive modes, complex error {bank['relative_error']:.4f}, "
            f"median magnitude error {bank['magnitude_error']:.3f} dB"
        )
    print(f"wrote {arguments.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
