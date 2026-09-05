#!/usr/bin/env python3
"""Generate Acustra's measured microphone modal coefficient header.

This is the complete deterministic path from Robert Mores'
``qualified_selected_impulses.mat`` to ``MeasuredBodyData.h``. For each guitar
it selects the treble-side bridge impact, estimates calibrated force-to-pressure
H1 responses for the treble and bass microphones, reconstructs minimum phase,
selects shared pole pairs, and fits one complex residue per output and pole.
The generated model must also remain within fixed complex, magnitude and stereo
balance regression limits; finiteness alone is not accepted.

The upper-east microphone is then fitted without changing any existing rounded
pole or treble/bass residue. Method.pdf section 2c/Figure 4 places it 20 cm from
the treble microphone toward the upper bout; all three are 10 cm above the top.
If the existing poles do not meet all current output and pair-balance limits,
append the smallest prominence-ordered prefix of distinct poles measured in
that microphone. Added poles have exactly zero treble/bass residues. This is
another measured output, not a second mechanical input or a phase-preserving
force/moment radiation matrix: each microphone still uses minimum phase.

Two measured nylon-string guitars are emitted for the engine's two material
settings. The g21 DeVoe flamenco is adapted for steel; it was not steel-strung.
The archive's physical-measures table lists Savarez Tomatito strings for g21:
https://www.savarez.com/tomatito-normal-tension-t50r (nylon/KF trebles).

NumPy and SciPy are required. Regenerate or check the committed header with:

    python3 Tools/GenerateMeasuredBody.py --raw-mat /path/to/qualified_selected_impulses.mat
    python3 Tools/GenerateMeasuredBody.py --raw-mat /path/to/qualified_selected_impulses.mat --check

Window. DAFx-26 specifies a 3000-sample truncated impulse with a raised-cosine
fade-out, but does not publish the fade's inner length. Acustra authors the
final tenth of the retained response as the fade. Robert Mores' archive script
separately suggests a 240-sample microphone taper, explicitly noting that it
was not used in the reference plots; it is supporting scale evidence, not the
source of Acustra's exact choice. For a record measured in a room, 3000 samples
is the length DAFx-26 gives and a longer window would fit the room: g21's first
reflection returns after a 6 m loop, 17.6 ms, already inside 3000 samples. For
a record measured anechoically there is no such ceiling, and the window is
instead the shortest of 3000, 6000, 12000, 24000 and 48000 samples at which
every mode below 700 Hz has its Q within 10 percent of its value at twice that
window -- the convergence rule the 2026-09-01 decision-log entry established.
Below that length the reported Q is the window's own bandwidth, not the guitar's.

Mode count. 96 poles resolve the truncated g21 response within the four
regression limits below. A resolved anechoic response has sharper and more
numerous peaks, so the bank grows: each guitar takes the smallest count, from
96 upward, at which its fit meets those same limits. No limit is relaxed for a
candidate.
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
from scipy.signal import find_peaks


SAMPLE_RATE = 48_000.0
FFT_SIZE = 65_536
RECORD_SAMPLES = 48_000
IMPACT_INDEX = 2  # Third one-second segment: treble-side bridge impact.
FORCE_CHANNEL = 0
TREBLE_MIC_CHANNEL = 4  # MATLAB channel 5.
BASS_MIC_CHANNEL = 5  # MATLAB channel 6.
UPPER_MIC_CHANNEL = 3  # MATLAB channel 4, Method.pdf section 2c/Figure 4.
ROOM_KEEP_SAMPLES = 3_000  # DAFx-26's truncated response.
CANDIDATE_KEEP_SAMPLES = (3_000, 6_000, 12_000, 24_000, 48_000)
CONVERGENCE_BAND_HZ = 700.0
CONVERGENCE_TOLERANCE = 0.10
BASE_MODE_COUNT = 96
MINIMUM_FREQUENCY = 80.0
# The fit's band is the one the 2026-08-30 body regression gate was set over,
# 80 Hz to 10 kHz, and the archive gives no reason to widen it: what thins out
# above it is the excitation. Measuring the force channel of the same
# one-second segment, shaped by the same full_taper this file applies to the
# whole record, mean power per band against its own 80-1000 Hz mean, the
# hammer is 5.4 dB (g21) and 4.4 dB (g34) down over 5-8 kHz, 15.9 and 13.5 dB
# down over 10-12 kHz and 31.6 and 27.8 dB down over 16-20 kHz, and H1 divides
# by it. The microphone noise floor is not the limit: taking the segment's
# first 3000 samples as signal (the impact lands at sample 102), its last 3000
# as noise, that same taper shape on both windows and mean power per band, the
# treble/bass microphones read 56.6/55.1 dB (g21) and 62.1/61.6 dB (g34) over
# 5-8 kHz and still 28.1/32.3 and 44.1/40.9 dB over 16-20 kHz. So the ceiling
# rests on the thinning excitation and on the existing gate's band, not on any
# claim that the record goes silent up there.
MAXIMUM_FREQUENCY = 10_000.0
PEAK_PROMINENCE_DB = 0.5
Q_MINIMUM = 2.0
Q_MAXIMUM = 80.0
RIDGE = 1.0e-7
MAX_COMPLEX_RELATIVE_ERROR = 0.30
MAX_MEDIAN_MAGNITUDE_ERROR_DB = 1.25
MAX_P90_MAGNITUDE_ERROR_DB = 3.75
MAX_STEREO_RATIO_P90_ERROR_DB = 5.0
# Above the modal-overlap frequency the peaks stop being separable and only the
# band level survives. Elie, Gautier and David, "Macro parameters describing
# the mechanical behavior of classical guitars", JASA 132 (2012) 4013-4024,
# Eq. (1), put the modal overlap factor at the half-power bandwidth over the
# spacing to the next mode and name f30 the frequency from which it exceeds
# 0.3; their Table IV measures f30 at 465-910 Hz over nine luthier-made and
# 506-635 Hz over three industrial classical guitars. Applying Eq. (1) to the
# g34 bank below puts its own first crossing at 503 Hz, inside that range.
# Everything from 5 kHz up is therefore a decade into the statistical regime,
# where the audible unit is the auditory filter: Glasberg and Moore,
# "Derivation of auditory filter shapes from notched-noise data", Hearing
# Research 47 (1990) 103-138, give it as ERB(F) = 24.7 (4.37 F + 1) Hz for F
# in kHz, about a sixth of an octave there. So the bank must hold the measured
# level in every whole ERB band from 5 kHz to MAXIMUM_FREQUENCY and not merely
# in aggregate; a fit that scattered its error across those bands would be
# heard as a change of timbre while still passing the p90 above. The committed
# banks reach 0.34/0.46 dB (g21 treble/bass mic) and 0.62/0.37 dB (g34), which
# is what rules out splitting the body into a modal part below f30 and a
# separately convolved measured residual above it: from 5 kHz up that
# residual is the quotient measured here, a filter within 0.62 dB of unity
# in every whole ERB band. Below 5 kHz, which such a residual stage would
# also have to cover down to f30, the quotient is not that flat -- over
# whole ERBs from 503 Hz it reaches -2.05 dB (g21 treble, 1125-1271 Hz) and
# -2.01 dB (g21 bass, 1612-1811 Hz), with 2 and 3 of 19 bands over 1 dB;
# g34 reaches -1.16 and -1.02 dB. There the modes are still separable, so
# that miss is pole placement in the modal region rather than the
# statistical behaviour a residual stage models, and the conclusion rests
# on the 5 kHz-and-up band where a residual stage would earn its taps.
BAND_ERROR_FLOOR_HZ = 5_000.0
MAX_BAND_MAGNITUDE_ERROR_DB = 1.0
RAW_MD5 = "733cb10baf5ce36d8bf333610ffbb260"
HAMMER_NEWTONS_PER_FULL_SCALE = (10_000.0 / 92.90) * 4.4482
MIC_PASCALS_PER_FULL_SCALE = 10_000.0 / 50.0
DEFAULT_NYLON_GUITAR = 34
# Provenance and measurement conditions from the archive's
# List_of_guitars_description.pdf.
GUITAR_DESCRIPTION = {
    21: "a 2018 Lester DeVoe flamenca blanca, spruce/cypress, measured in a "
        "school music room in Freiburg (first reflection after a 6 m loop)",
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
ANECHOIC_GUITARS = frozenset({34, 35, 36})
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


def extract_targets(
    path: Path, guitar: int = 21, keep_samples: int = ROOM_KEEP_SAMPLES,
    channels: tuple[int, ...] = (TREBLE_MIC_CHANNEL, BASS_MIC_CHANNEL),
) -> list[np.ndarray]:
    values = load_matrix(path)
    start = IMPACT_INDEX * RECORD_SAMPLES
    record = values[guitar - 1, start : start + RECORD_SAMPLES, :]
    if not np.all(np.isfinite(record)):
        raise ValueError(f"{path}: g{guitar} treble-impact record is non-finite")

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

    fade_samples = keep_samples // 10
    impulse_taper = np.ones(keep_samples)
    impulse_taper[-fade_samples:] = 0.5 + 0.5 * np.cos(
        np.linspace(0.0, np.pi, fade_samples)
    )

    targets: list[np.ndarray] = []
    for channel in channels:
        pressure = record[:, channel] * full_taper * MIC_PASCALS_PER_FULL_SCALE
        spectrum = np.fft.rfft(pressure, FFT_SIZE)
        h1 = spectrum * np.conj(force) / denominator
        impulse = np.fft.irfft(h1, FFT_SIZE)
        windowed = np.zeros(FFT_SIZE)
        windowed[:keep_samples] = impulse[:keep_samples] * impulse_taper
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


def converged_keep_samples(path: Path, guitar: int) -> tuple[int, float]:
    """Shortest window whose low-frequency Q agrees with twice that window.

    A mode is matched to its counterpart at the longer window within
    max(3 Hz, 3 percent); a mode with no counterpart has not converged either.
    """
    poles = {
        keep: [item for item in candidate_poles(extract_targets(path, guitar, keep))
               if item[0] < CONVERGENCE_BAND_HZ]
        for keep in CANDIDATE_KEEP_SAMPLES
    }
    for keep in CANDIDATE_KEEP_SAMPLES[:-1]:
        worst = 0.0
        for frequency, q, _ in poles[keep]:
            near = [(abs(other - frequency), other_q)
                    for other, other_q, _ in poles[2 * keep]
                    if abs(other - frequency) <= max(3.0, 0.03 * frequency)]
            if not near:
                worst = float("inf")
                break
            worst = max(worst, abs(min(near)[1] - q) / max(q, 1.0e-9))
        if worst <= CONVERGENCE_TOLERANCE:
            return keep, worst
    raise ValueError(f"g{guitar}: no window below 1 s converged below 700 Hz")


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


def erb_bands(low: float, high: float) -> list[tuple[float, float]]:
    """Whole Glasberg-Moore ERBs tiled from low, dropping a partial last band."""
    bands: list[tuple[float, float]] = []
    edge = low
    while True:
        width = 24.7 * (4.37 * edge / 1000.0 + 1.0)
        if edge + width > high:
            return bands
        bands.append((edge, edge + width))
        edge += width


def fit_errors(
    targets: list[np.ndarray],
    modes: list[tuple[float, float, float]],
    residues: list[np.ndarray],
) -> tuple[list[tuple[float, float, float]], float, float]:
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

    band_frequency = frequency[indices]
    band_error_db = 0.0
    for low, high in erb_bands(BAND_ERROR_FLOOR_HZ, MAXIMUM_FREQUENCY):
        selected = np.flatnonzero(
            (band_frequency >= low) & (band_frequency < high)
        )
        for target, model in zip(targets, models):
            desired_level = np.sqrt(
                np.mean(np.abs(target[indices][selected]) ** 2)
            )
            model_level = np.sqrt(np.mean(np.abs(model[selected]) ** 2))
            band_error_db = max(band_error_db, abs(20.0 * np.log10(
                max(model_level, 1.0e-30) / max(desired_level, 1.0e-30))))
    return errors, stereo_ratio_p90_error_db, float(band_error_db)


def within_limits(
    errors: list[tuple[float, float, float]], stereo: float, band: float
) -> bool:
    return (
        all(error[0] <= MAX_COMPLEX_RELATIVE_ERROR for error in errors)
        and all(error[1] <= MAX_MEDIAN_MAGNITUDE_ERROR_DB for error in errors)
        and all(error[2] <= MAX_P90_MAGNITUDE_ERROR_DB for error in errors)
        and stereo <= MAX_STEREO_RATIO_P90_ERROR_DB
        and band <= MAX_BAND_MAGNITUDE_ERROR_DB
    )


def modal_fit(path: Path, guitar: int) -> dict:
    if guitar in ANECHOIC_GUITARS:
        keep_samples, convergence = converged_keep_samples(path, guitar)
    else:
        keep_samples, convergence = ROOM_KEEP_SAMPLES, float("nan")
    targets = extract_targets(path, guitar, keep_samples)
    candidates = candidate_poles(targets)
    candidates.sort(key=lambda item: (-item[2], item[0]))
    if len(candidates) < BASE_MODE_COUNT:
        raise ValueError(
            f"g{guitar}: only {len(candidates)} shared pole candidates, "
            f"expected at least {BASE_MODE_COUNT}"
        )
    for count in range(BASE_MODE_COUNT, len(candidates) + 1):
        modes = sorted(candidates[:count], key=lambda item: item[0])
        residues = [fit_residues(target, modes) for target in targets]
        values = np.concatenate(
            (
                np.array([(mode[0], mode[1]) for mode in modes]).ravel(),
                residues[0].view(np.float64),
                residues[1].view(np.float64),
            )
        )
        if not np.all(np.isfinite(values)):
            continue
        errors, stereo, band = fit_errors(targets, modes, residues)
        if within_limits(errors, stereo, band):
            return {
                "guitar": guitar,
                "keep_samples": keep_samples,
                "convergence": convergence,
                "modes": modes,
                "residues": residues,
                "errors": errors,
                "stereo": stereo,
                "band": band,
            }
    raise ValueError(
        f"g{guitar}: no mode count up to {len(candidates)} met the fit limits"
    )


def add_upper_microphone(path: Path, bank: dict) -> dict:
    """Append a measured output while freezing the existing rounded model.

    Candidate ordering and duplicate tolerance use the existing peak selector.
    All gates run on emitted float32 poles/residues, including upper/treble and
    upper/bass balance. No failed gate changes a limit or an existing output.
    """
    targets = extract_targets(path, bank["guitar"], bank["keep_samples"],
                              (TREBLE_MIC_CHANNEL, BASS_MIC_CHANNEL, UPPER_MIC_CHANNEL))
    modes = [(float(np.float32(frequency)), float(np.float32(q)), prominence)
             for frequency, q, prominence in bank["modes"]]
    base_count = len(modes)
    rounded = lambda values: (values.real.astype(np.float32).astype(float)
                              + 1j * values.imag.astype(np.float32).astype(float))
    original_residues = [rounded(values) for values in bank["residues"]]
    candidates = sorted(candidate_poles([targets[2]]), key=lambda item: (-item[2], item[0]))
    trials = []
    while True:
        upper = rounded(fit_residues(targets[2], modes))
        residues = [np.pad(values, (0, len(modes) - base_count)) for values in original_residues]
        checks = [fit_errors([targets[2], target], modes, [upper, other])
                  for target, other in zip(targets, residues)]
        passed = all(within_limits(*check) for check in checks)
        trials.append({"added_modes": len(modes) - base_count,
                       "upper_error": checks[0][0][0],
                       "pair_ratio_p90": [check[1] for check in checks],
                       "worst_pair_erb": max(check[2] for check in checks),
                       "passed": passed})
        if passed:
            return {**bank, "modes": modes, "residues": [*residues, upper],
                    "base_mode_count": base_count, "upper_trials": trials}
        for candidate in candidates:
            # Same frequency/bandwidth tolerance as candidate_poles' merge.
            if all(abs(candidate[0] - other[0]) > max(
                    2.0, 0.5 * min(candidate[0] / candidate[1], other[0] / other[1]))
                   for other in modes):
                modes.append((float(np.float32(candidate[0])),
                              float(np.float32(candidate[1])), candidate[2]))
                break
        else:
            raise ValueError(f"g{bank['guitar']}: upper microphone exhausted distinct measured poles "
                             f"without passing existing limits; last trial {trials[-1]}")


def cpp_float(value: float) -> str:
    text = format(float(np.float32(value)), ".9g")
    if "." not in text and "e" not in text:
        text += ".0"
    return text + "f"


def bank_block(name: str, bank: dict) -> str:
    rows = []
    for mode, treble, bass, upper in zip(bank["modes"], *bank["residues"]):
        values = (mode[0], mode[1], treble.real, treble.imag,
                  bass.real, bass.imag, upper.real, upper.imag)
        rows.append("    { " + ", ".join(cpp_float(value) for value in values)
                    + " },")
    guitar = bank["guitar"]
    keep = bank["keep_samples"]
    if guitar in ANECHOIC_GUITARS:
        window = (
            f"{keep} samples ({keep / 48.0:.1f} ms), the shortest window whose "
            f"modes below 700 Hz hold their Q to "
            f"{bank['convergence'] * 100.0:.1f} percent at twice that length"
        )
    else:
        window = f"{keep} samples ({keep / 48.0:.1f} ms), DAFx-26's length"
    errors = "; ".join(
        f"channel {index} complex {error[0]:.4f}, median {error[1]:.3f} dB, "
        f"p90 {error[2]:.3f} dB"
        for index, error in enumerate(bank["errors"])
    )
    comment = textwrap.fill(
        f'g{guitar}, {GUITAR_DESCRIPTION[guitar]}. Window {window}, with the'
        f' final tenth as the raised-cosine fade. {bank["base_mode_count"]} original'
        f' poles: {errors}; stereo-ratio p90 {bank["stereo"]:.3f} dB;'
        f' worst ERB-band level error above 5 kHz {bank["band"]:.3f} dB.',
        width=76, initial_indent="// ", subsequent_indent="// ")
    upper = bank["upper_trials"][-1]
    upper_comment = textwrap.fill(
        f'Upper-east microphone: {upper["added_modes"]} appended output-only poles;'
        f' existing rounded poles and treble/bass residues unchanged. Upper complex'
        f' {upper["upper_error"][0]:.4f}, median {upper["upper_error"][1]:.3f} dB,'
        f' p90 {upper["upper_error"][2]:.3f} dB; upper/treble and upper/bass ratio'
        f' p90 {upper["pair_ratio_p90"][0]:.3f}/{upper["pair_ratio_p90"][1]:.3f} dB;'
        f' worst pair ERB error {upper["worst_pair_erb"]:.3f} dB. Each smaller'
        f' prominence-ordered prefix failed at least one unchanged gate.',
        width=76, initial_indent="// ", subsequent_indent="// ")
    return f'''{comment}
{upper_comment}
inline constexpr std::array<MeasuredBodyMode, {len(bank["modes"])}> {name} {{{{
{chr(10).join(rows)}
}}}};'''


def render_header(steel: dict, nylon: dict) -> str:
    return f'''// Generated by Tools/GenerateMeasuredBody.py; do not hand-edit.
// The treble-side bridge impact drives three calibrated H1 paths per guitar:
// left is the treble microphone, right the bass microphone, upper the
// upper-east microphone (20 cm from the treble mic toward the upper bout,
// all 10 cm above the top; Method.pdf section 2c/Figure 4). Each causal
// response is converted independently to minimum phase. DAFx-26 specifies the
// retained length and raised-cosine taper, but not its inner length; Acustra
// authors the final tenth as that fade. The shared frequency/Q pairs and
// independent complex residues are an authored regularised fit, not
// coefficients published by DAFx-26. The original material settings select
// two measured nylon-string guitars: g21 flamenco is adapted for steel,
// g34 classical for nylon. No steel-strung body radiation was measured here.
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
    float upperReal;
    float upperImaginary;
}};

{bank_block("measuredSteelBodyModes", steel)}

{bank_block("measuredNylonBodyModes", nylon)}
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

    steel = add_upper_microphone(arguments.raw_mat, modal_fit(arguments.raw_mat, 21))
    nylon = add_upper_microphone(arguments.raw_mat, modal_fit(arguments.raw_mat, arguments.nylon_guitar))
    header = render_header(steel, nylon)
    if arguments.check:
        return 0 if check_output(arguments.output, header) else 1
    with arguments.output.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write(header)
    for name, bank in (("steel", steel), ("nylon", nylon)):
        print(
            f"generated {name} bank g{bank['guitar']}: {len(bank['modes'])} "
            f"modes ({len(bank['modes']) - bank['base_mode_count']} upper-only), "
            f"window {bank['keep_samples']} samples"
        )
        print(f"upper microphone gates: {bank['upper_trials'][-1]}")
    print(f"wrote {arguments.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
