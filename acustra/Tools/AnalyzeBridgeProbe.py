#!/usr/bin/env python3
"""Report matched-window bridge-stage harmonic magnitudes and relative phase."""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
from scipy.io import wavfile
from scipy.signal import find_peaks, resample_poly, windows, zoom_fft


RATE = 48_000
WINDOWS = ((0.05, 0.20), (0.5, 1.0), (1.0, 2.0), (2.0, 4.0))
POINTS = 22_001
MINIMUM_SAMPLES = round(max(end for _, end in WINDOWS) * RATE)


def midi_frequency(midi: int) -> float:
    return 440.0 * 2.0 ** ((midi - 69.0) / 12.0)


def wrap_phase_degrees(value: float) -> float:
    return (value + 180.0) % 360.0 - 180.0


def read(path: Path) -> np.ndarray:
    if path.suffix == ".f32":
        values = read_raw(path, 2)
        return values.mean(axis=1)
    rate, values = wavfile.read(path)
    if np.issubdtype(values.dtype, np.integer):
        scale = float(max(abs(np.iinfo(values.dtype).min),
                          np.iinfo(values.dtype).max))
    else:
        scale = 1.0
    mono = values.astype(np.float64)
    if mono.ndim == 2:
        mono = mono.mean(axis=1)
    mono /= scale
    if rate != RATE:
        mono = resample_poly(mono, RATE, rate)
    validate_signal(path, mono)
    return mono


def read_raw(path: Path, channels: int) -> np.ndarray:
    values = np.fromfile(path, dtype="<f4")
    if values.size % channels != 0:
        raise ValueError(
            f"{path}: sample count is not divisible by {channels} channels")
    values = values.reshape(-1, channels)
    validate_signal(path, values)
    return values


def validate_signal(path: Path, values: np.ndarray) -> None:
    if values.shape[0] < MINIMUM_SAMPLES:
        raise ValueError(
            f"{path}: needs at least {MINIMUM_SAMPLES} samples per channel")
    if not np.all(np.isfinite(values)):
        raise ValueError(f"{path}: contains non-finite samples")


def analyse_signal(signal: np.ndarray, label: str, midi: int) -> None:
    print(label)
    midi_fundamental = midi_frequency(midi)
    search_ratio = 2.0 ** (220.0 / 1200.0)
    low = midi_fundamental / search_ratio
    high = midi_fundamental * search_ratio
    primary_db = None
    for begin, end in WINDOWS:
        segment = signal[int(begin * RATE) : int(end * RATE)].astype(np.float64)
        segment -= np.mean(segment)
        taper = windows.hann(segment.size, sym=False)
        broad_spectrum = np.abs(np.fft.rfft(segment * taper))
        broad_power = broad_spectrum * broad_spectrum
        broad_frequency = np.fft.rfftfreq(segment.size, 1.0 / RATE)
        audible = (broad_frequency >= 20.0) & (broad_frequency <= 10_000.0)
        centroid = float(np.sum(
            broad_frequency[audible] * broad_power[audible])
            / max(np.sum(broad_power[audible]), 1.0e-30))
        complex_spectrum = 2.0 * zoom_fft(
            segment * taper, [low, high], m=POINTS, fs=RATE,
            endpoint=True) / np.sum(taper)
        spectrum = np.abs(complex_spectrum)
        frequencies = np.linspace(low, high, POINTS)
        peaks, _ = find_peaks(spectrum, distance=300)
        ranked = peaks[np.argsort(spectrum[peaks])[::-1]][:5]
        if ranked.size == 0:
            continue
        level = 20.0 * np.log10(max(spectrum[ranked[0]], 1.0e-30))
        if primary_db is None:
            primary_db = level
        formatted = []
        for index in ranked:
            db = 20.0 * np.log10(max(spectrum[index], 1.0e-30))
            cents = 1200.0 * np.log2(frequencies[index] / midi_fundamental)
            formatted.append(
                f"{frequencies[index]:.3f}Hz {cents:+.1f}c "
                f"{db:.1f}dBFS ({db - level:+.1f}dB)")
        print(f"  {begin:.2f}-{end:.2f}s centroid {centroid:.0f}Hz "
              f"primary {level:.1f}dBFS "
              f"({level - primary_db:+.1f}dB): " + "; ".join(formatted))

        fundamental = frequencies[ranked[0]]
        fundamental_phase = np.angle(complex_spectrum[ranked[0]])
        harmonics = []
        for number in range(1, 13):
            target = number * fundamental
            half_width = target * (2.0 ** (25.0 / 1200.0) - 1.0)
            if target + half_width >= 0.5 * RATE:
                break
            coefficients = 2.0 * zoom_fft(
                segment * taper, [target - half_width, target + half_width],
                m=2001, fs=RATE, endpoint=True) / np.sum(taper)
            magnitudes = np.abs(coefficients)
            peak = int(np.argmax(magnitudes))
            harmonic_level = 20.0 * np.log10(max(magnitudes[peak], 1.0e-30))
            relative_phase = wrap_phase_degrees(np.degrees(
                np.angle(coefficients[peak]) - number * fundamental_phase))
            harmonics.append(
                f"H{number} {harmonic_level - level:+.1f}dB "
                f"phase-re-H1 {relative_phase:+.1f}deg")
        print("    " + ", ".join(harmonics))


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--midi", type=int, default=40)
parser.add_argument("paths", nargs="+", type=Path)
arguments = parser.parse_args()
if not 0 <= arguments.midi <= 127:
    parser.error("--midi must be between 0 and 127")

try:
    for path in arguments.paths:
        if path.name.endswith(".telemetry.f32"):
            channels = read_raw(path, 5)
            for index, name in enumerate(("main-force", "body-force",
                                          "tail-force", "velocity",
                                          "sympathetic-force")):
                analyse_signal(
                    channels[:, index], f"{path}:{name}", arguments.midi)
        else:
            analyse_signal(read(path), str(path), arguments.midi)
except (OSError, ValueError) as error:
    parser.error(str(error))
