#!/usr/bin/env python3
"""Rank temporary E2 pluck sweeps against the real-reference spectrum."""

from pathlib import Path
import re

import numpy as np
from scipy.signal import windows, zoom_fft


RATE = 48_000
TARGET = np.array([4.7, -5.1, -15.1])


def metrics(path: Path):
    signal = np.fromfile(path, dtype=np.float32).reshape(-1, 2).mean(axis=1)
    segment = signal[int(0.05 * RATE):int(0.20 * RATE)].astype(np.float64)
    segment -= np.mean(segment)
    taper = windows.hann(segment.size, sym=False)
    fft = np.fft.rfft(segment * taper)
    frequency = np.fft.rfftfreq(segment.size, 1.0 / RATE)
    audible = (frequency >= 20.0) & (frequency <= 10_000.0)
    power = np.abs(fft) ** 2
    centroid = np.sum(frequency[audible] * power[audible]) / np.sum(power[audible])
    levels = []
    for harmonic in range(1, 5):
        target = harmonic * 82.406889
        half_width = target * (2.0 ** (25.0 / 1200.0) - 1.0)
        spectrum = np.abs(zoom_fft(
            segment * taper, [target - half_width, target + half_width],
            m=2001, fs=RATE, endpoint=True))
        levels.append(20.0 * np.log10(max(np.max(spectrum), 1.0e-30)))
    relative = np.array(levels[1:]) - levels[0]
    spectral_error = float(np.sqrt(np.mean((relative - TARGET) ** 2)))
    score = spectral_error + abs(centroid - 153.0) / 20.0
    return centroid, relative, spectral_error, score


rows = []
for path in Path("/tmp").glob("acustra-sweep-a*-d*.f32"):
    match = re.search(r"-a([0-9.]+)-d([0-9]+)\.f32$", path.name)
    if match is None:
        continue
    rows.append((metrics(path), float(match.group(1)), int(match.group(2))))

for (centroid, relative, error, score), aperture, distance in sorted(rows):
    print(f"a={aperture:4.2f}ms d={distance/10:4.1f}cm: "
          f"H2/H3/H4={relative[0]:+5.1f}/{relative[1]:+5.1f}/"
          f"{relative[2]:+5.1f}dB centroid={centroid:5.0f}Hz "
          f"harmonic-RMSE={error:4.1f} score={score:4.1f}")
