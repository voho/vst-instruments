#!/usr/bin/env python3
"""Measure the archtop's excitation per velocity layer, body-free.

Why a ratio and not an inverse filter. String, bridge, body and microphone are
linear and velocity-invariant, so two takes of the same note at two dynamics
pass through one and the same transfer. Divide one take's partial levels by the
other's and every linear stage cancels exactly, leaving the ratio of the two
excitations and nothing else. That identity is the whole method; it needs no
loop inversion, and unlike an inverse-filtered "excitation" (which is the pluck
convolved with the body -- Erkut, Valimaki, Karjalainen and Laurson, AES 108
preprint 5114, 2000; Laurson et al., CMJ 25(3), 2001) it has no body left in it.

What is measured, per take:

  onset          first 1 ms frame whose energy exceeds 1e-4 of the file's
                 largest, backed off one frame.
  f0             parabolic peak of a 0.5 s Hann spectrum within 6 % of the
                 SFZ pitch_keycenter.
  B              inharmonicity from the measured partial peaks, weighted
                 least squares on f_n = n f0 sqrt(1 + B n^2).
  A0_n, tau_n    each partial is heterodyned at f_n with a Hann window of
                 6/f0 (never under 46 ms, so the 4/W main lobe stays inside
                 f0) hopped by W/4; frames whose partial sits at least 12 dB
                 over the level measured half a partial away are fitted with
                 an iteratively reweighted straight line in log amplitude and
                 extrapolated back to the onset. Extrapolating removes the
                 80-250 ms window that earlier audits had to use, in which
                 the loop's own loss and the idle strings' partials sit.
  p              plucking point, from the comb the pluck leaves in A0_n
                 (Traube and Smith, "Estimating the plucking point on a
                 guitar string", Proc. COST G-6 Conference on Digital Audio
                 Effects, DAFx-00, Verona, 7-9 December 2000, pp. 153-158,
                 https://www.dafx.de/paper-archive/2000/pdf/Caroline_Traube.pdf):
                 the p in PLUCK_RANGE = [0.03, 0.36] whose |sin(n pi p)|
                 leaves the flattest residual (L1, cubic in log n for the
                 body) is taken, searched on a PLUCK_STEP = 0.0004 grid. The
                 comb is symmetric under p -> 1 - p, so that range is what
                 picks the reading as a distance from the bridge; it is a
                 choice of branch, not a measurement, and it is reported.
                 Within a root the takes share one excitation shape per
                 layer, so p is refined jointly: the layer's shape is the
                 median over its round robins of each take's comb-divided
                 level, and each take's p is then re-fitted against it.
                 Harmonics with |sin(n pi p)| below COMB_GUARD are dropped:
                 dividing by a comb null amplifies whatever filled it.
  aperiodic      the attack's energy in every bin further than two bins from
                 any f_n, over one 6/f0 (never under 46 ms) Hann window at the
                 onset, as a share of the whole 50 Hz-10 kHz attack, with that
                 share's own spectral centroid and its part above 3 kHz.
  level          peak and RMS over the first 250 ms. acoustic.sfz carries no
                 per-layer volume opcode (every group is amplitude=100), so
                 the raw files hold the recorded level law that the sample
                 bank's per-zone playback trim removes.

Pooling: for each layer/softest pair of round robins the two takes are fitted
over one common frame window, so a partial whose decay is not a single
exponential biases both ends of the ratio the same way. Each pair's ratio is
corrected by its own two plucking points, then pooled by harmonic with the
median over the round-robin pairs and roots together and the median absolute
deviation as the spread. A take that reaches digital full scale is left out of
every ratio: it is not a linear reading of its pluck.

Sources. The archive is sfzinstruments/karoryfer.shinyguitar at the commit
pinned in ThirdParty/Shinyguitar-README.txt (CC0 1.0): Samples/acoustic holds
the microphone takes ("..._1.wav") that Acustra's sample bank embeds, and
Samples/electric the magnetic-pickup signal captured at the same instant
("..._2.wav", identical frame counts, referenced by electric_*.sfz). The pickup
is a near-direct string sensor, so its layer ratio and the microphone's are two
independent readings of one excitation; they are reported separately and their
agreement is the check that no body is left in either.

Usage:

    python3 Tools/ExtractExcitation.py --shiny <checkout> --output out.json
    python3 Tools/ExtractExcitation.py --self-test --probe build-dsp/AcustraBridgeProbe

The self-test has one part that can fail and three that report. It fails if the
onset-extrapolated levels of a synthesised note whose excitation is known
exactly are not recovered within 1 dB per partial to H12. Beside that it renders
AcustraBridgeProbe at seven plucking-point control values, whose excitation
differs only in where the string is taken, and reports how nearly one excitation
comes back out of all seven, how nearly affine in the control the recovered
plucking point is, and how nearly the excitation ratio between the extreme
plucking points is the same with the bridge coupled, with the sympathetic sum
off and with the bridge decoupled -- body-freeness measured inside the engine.
The first of those three has a floor the tool cannot reach: the engine's own
released shape (initialisePluck's smoothed triangle, clipped at the endpoint it
subtracts) departs from |sin(n pi p)|/n^2 by up to 3.5 dB, and by an amount that
depends on p, so the seven renders' excitations are not in fact identical up to
their combs.
"""

from __future__ import annotations

import argparse
import json
import math
import re
import subprocess
import sys
from pathlib import Path

import numpy as np
from scipy.io import wavfile

# Harmonics whose comb factor is under this are dropped before dividing the
# plucking point out; at a comb null the measured level is whatever filled the
# null, not the excitation.
COMB_GUARD = 0.30
# Frame is usable when the partial stands this far over the level measured half
# a partial spacing away.
PARTIAL_SNR = 4.0
MIN_FIT_FRAMES = 6
MIN_FIT_SPAN_SECONDS = 0.12
TRACK_SECONDS = 1.6
ATTACK_SECONDS = 0.046
LEVEL_SECONDS = 0.250
RESIDUAL_TOP_HZ = 10000.0
RESIDUAL_SPLIT_HZ = 3000.0
REPORT_HARMONICS = 20
PLUCK_RANGE = (0.03, 0.36)
PLUCK_STEP = 0.0004


# ---------------------------------------------------------------- audio input

def read_mono(path: Path) -> tuple[float, np.ndarray]:
    rate, data = wavfile.read(str(path))
    x = np.asarray(data)
    if x.ndim > 1:
        x = x.mean(axis=1)
    x = x.astype(np.float64)
    if np.issubdtype(np.asarray(data).dtype, np.integer):
        x /= float(np.iinfo(np.asarray(data).dtype).max)
    return float(rate), x


def read_probe(path: Path) -> tuple[float, np.ndarray]:
    """BridgeProbe writes interleaved stereo float32 at 48 kHz."""
    x = np.fromfile(str(path), dtype="<f4").astype(np.float64)
    return 48000.0, x.reshape(-1, 2).mean(axis=1)


# ------------------------------------------------------------------ estimators

def onset_index(x: np.ndarray, rate: float) -> int:
    n = max(1, int(round(0.001 * rate)))
    frames = len(x) // n
    if frames < 2:
        return 0
    energy = (x[: frames * n].reshape(frames, n) ** 2).mean(axis=1)
    peak = energy.max()
    if not peak > 0.0:
        return 0
    return max(0, (int(np.argmax(energy > peak * 1e-4)) - 1) * n)


def _peak_frequency(spectrum: np.ndarray, k: int, rate: float, size: int) -> float:
    a, b, c = spectrum[k - 1], spectrum[k], spectrum[k + 1]
    denominator = a - 2.0 * b + c
    offset = 0.5 * (a - c) / denominator if denominator != 0.0 else 0.0
    return (k + offset) * rate / size


def refine_f0(x: np.ndarray, rate: float, guess: float) -> float:
    w = min(len(x), int(0.5 * rate))
    segment = x[:w] * np.hanning(w)
    size = 1 << int(math.ceil(math.log2(max(w * 8, 16))))
    spectrum = np.abs(np.fft.rfft(segment, size))
    freq = np.fft.rfftfreq(size, 1.0 / rate)
    band = np.where((freq >= guess * 0.94) & (freq <= guess * 1.06))[0]
    band = band[(band > 0) & (band < len(spectrum) - 1)]
    if len(band) == 0:
        return guess
    return _peak_frequency(spectrum, int(band[np.argmax(spectrum[band])]), rate, size)


def inharmonicity(x: np.ndarray, rate: float, f0: float, nmax: int) -> float:
    w = min(len(x), int(0.5 * rate))
    segment = x[:w] * np.hanning(w)
    size = 1 << int(math.ceil(math.log2(max(w * 8, 16))))
    spectrum = np.abs(np.fft.rfft(segment, size))
    freq = np.fft.rfftfreq(size, 1.0 / rate)
    orders, peaks, weights = [], [], []
    for n in range(2, nmax + 1):
        target = n * f0
        if target > 0.45 * rate:
            break
        band = np.where((freq >= target * 0.985) & (freq <= target * 1.03))[0]
        band = band[(band > 0) & (band < len(spectrum) - 1)]
        if len(band) < 3:
            continue
        k = int(band[np.argmax(spectrum[band])])
        if spectrum[k] < spectrum.max() * 3e-4:
            continue
        orders.append(float(n))
        peaks.append(_peak_frequency(spectrum, k, rate, size))
        weights.append(float(spectrum[k]))
    if len(orders) < 4:
        return 0.0
    n = np.array(orders)
    y = (np.array(peaks) / (n * f0)) ** 2 - 1.0
    w_ = np.array(weights)
    return float(np.clip(np.sum(w_ * y * n ** 2) / np.sum(w_ * n ** 4), 0.0, 5e-3))


def partial_tracks(x: np.ndarray, rate: float, f0: float, b: float,
                   nmax: int) -> tuple[np.ndarray, dict[int, tuple[np.ndarray, np.ndarray, float]]]:
    """Hann-windowed heterodyne of every partial, with a between-partial floor."""
    window_length = int(min(max(6.0 / f0, 0.046) * rate, max(len(x) // 4, 16)))
    hop = max(1, window_length // 4)
    window = np.hanning(window_length)
    weight = window.sum()
    starts = np.arange(0, max(1, min(len(x) - window_length,
                                     int(TRACK_SECONDS * rate))), hop)
    if len(starts) == 0:
        return np.zeros(0), {}
    frames = np.lib.stride_tricks.sliding_window_view(x, window_length)[starts]
    frames = frames * window
    times = (starts + window_length / 2.0) / rate
    sample = np.arange(window_length)
    tracks: dict[int, tuple[np.ndarray, np.ndarray, float]] = {}
    for n in range(1, nmax + 1):
        fn = n * f0 * math.sqrt(1.0 + b * n * n)
        if fn > 0.45 * rate:
            break
        probes = np.stack([
            np.exp(-2j * np.pi * fn * sample / rate),
            np.exp(-2j * np.pi * (fn + 0.5 * f0) * sample / rate),
            np.exp(-2j * np.pi * max(fn - 0.5 * f0, 0.6 * f0) * sample / rate),
        ])
        magnitude = 2.0 * np.abs(frames @ probes.conj().T) / weight
        tracks[n] = (magnitude[:, 0],
                     np.minimum(magnitude[:, 1], magnitude[:, 2]), fn)
    return times, tracks


def usable_range(times: np.ndarray, amplitude: np.ndarray,
                 floor: np.ndarray) -> tuple[float, float] | None:
    ok = (amplitude > PARTIAL_SNR * floor) & (amplitude > 0.0)
    if ok.sum() < MIN_FIT_FRAMES:
        return None
    index = np.where(ok)[0]
    keep = [int(index[0])]
    for j in index[1:]:
        if j - keep[-1] <= 2:
            keep.append(int(j))
        else:
            break
    if len(keep) < MIN_FIT_FRAMES:
        return None
    if times[keep[-1]] - times[keep[0]] < MIN_FIT_SPAN_SECONDS:
        return None
    return float(times[keep[0]]), float(times[keep[-1]])


def fit_onset_level(times: np.ndarray, amplitude: np.ndarray,
                    low: float, high: float) -> tuple[float, float] | None:
    mask = (times >= low) & (times <= high) & (amplitude > 0.0)
    if mask.sum() < 5:
        return None
    t = times[mask]
    y = np.log(amplitude[mask])
    design = np.vstack([np.ones_like(t), t]).T
    weight = np.ones_like(t)
    coefficients = np.zeros(2)
    for _ in range(4):
        coefficients, *_ = np.linalg.lstsq(design * weight[:, None],
                                           y * weight, rcond=None)
        residual = y - design @ coefficients
        scale = 1.4826 * np.median(np.abs(residual - np.median(residual))) + 1e-12
        weight = 1.0 / np.sqrt(1.0 + (residual / (2.0 * scale)) ** 2)
    if coefficients[1] >= 0.0:
        return None
    return float(np.exp(coefficients[0])), float(-1.0 / coefficients[1])


def comb_fit(orders, levels, order=3) -> float:
    """Traube-Smith plucking point: the comb that leaves the flattest residual."""
    n = np.asarray(orders, dtype=float)
    log_level = np.log(np.asarray(levels, dtype=float))
    basis = np.vander(np.log(n), order + 1)
    best_p, best_cost = float("nan"), float("inf")
    for p in np.arange(PLUCK_RANGE[0], PLUCK_RANGE[1], PLUCK_STEP):
        comb = np.abs(np.sin(np.pi * n * p))
        mask = comb > COMB_GUARD
        if mask.sum() < order + 3:
            continue
        d = log_level[mask] - np.log(comb[mask])
        coefficients, *_ = np.linalg.lstsq(basis[mask], d, rcond=None)
        cost = float(np.mean(np.abs(d - basis[mask] @ coefficients)))
        if cost < best_cost:
            best_p, best_cost = float(p), cost
    return best_p


# One player's hand stays in one place across a root's takes: the log measures
# the archtop's takes a median 0.02 of the string length apart. A second pass
# searches only this far from the root's own median, which is three times that
# spread, so a take whose single-take comb fit landed on a spurious minimum is
# re-fitted where the hand actually was rather than dropped.
PLUCK_SECOND_PASS_HALF_WIDTH = 0.06


def refine_pluck_points(levels: list[dict[int, float]], groups: list[int],
                        nmax: int, iterations: int = 12,
                        second_pass: bool = True) -> list[float]:
    """Alternate: shape per group as the median over its takes, then p per take.

    Takes in one group (a root's velocity layer) share an excitation shape, so
    what differs between them is where the string was taken. Fitting each take
    against the group's own shape resolves the comb far better than the single
    take's smoothness prior can.
    """
    orders = sorted({n for take in levels for n in take if n <= nmax})
    if not orders:
        return [float("nan")] * len(levels)
    n = np.array(orders, dtype=float)
    table = np.full((len(levels), len(orders)), np.nan)
    for i, take in enumerate(levels):
        for j, order in enumerate(orders):
            if order in take:
                table[i, j] = math.log(take[order])
    p = np.array([comb_fit(orders, [take.get(o, np.nan) for o in orders])
                  if all(o in take for o in orders[:6]) else float("nan")
                  for take in levels])
    p = np.where(np.isfinite(p), p, 0.5 * (PLUCK_RANGE[0] + PLUCK_RANGE[1]))
    gain = np.zeros(len(levels))
    grid = np.arange(PLUCK_RANGE[0], PLUCK_RANGE[1], PLUCK_STEP)
    combs = np.abs(np.sin(np.pi * np.outer(grid, n)))
    shape = np.full((max(groups) + 1, len(orders)), np.nan)
    for _ in range(iterations):
        for g in set(groups):
            rows = [i for i, gg in enumerate(groups) if gg == g]
            stack = np.full((len(rows), len(orders)), np.nan)
            for r, i in enumerate(rows):
                comb = np.abs(np.sin(np.pi * n * p[i]))
                ok = comb > COMB_GUARD
                stack[r, ok] = table[i, ok] - gain[i] - np.log(comb[ok])
            with np.errstate(invalid="ignore"):
                shape[g] = np.nanmedian(stack, axis=0)
        moved = 0.0
        for i, g in enumerate(groups):
            best = _best_comb(table[i], shape[g], grid, combs, p[i], gain[i])
            moved = max(moved, abs(best[0] - p[i]))
            p[i], gain[i] = best[0], best[2]
        if moved < PLUCK_STEP:
            break

    if second_pass:
        centre = float(np.median(p))
        window = ((grid >= centre - PLUCK_SECOND_PASS_HALF_WIDTH)
                  & (grid <= centre + PLUCK_SECOND_PASS_HALF_WIDTH))
        if window.sum() >= 3:
            for i, g in enumerate(groups):
                best = _best_comb(table[i], shape[g], grid[window],
                                  combs[window], p[i], gain[i])
                p[i], gain[i] = best[0], best[2]
    return [float(v) for v in p]


def _best_comb(observed: np.ndarray, target: np.ndarray, grid: np.ndarray,
               combs: np.ndarray, p: float, gain: float) -> tuple[float, float, float]:
    best = (p, float("inf"), gain)
    finite = np.isfinite(observed) & np.isfinite(target)
    for k, candidate in enumerate(grid):
        comb = combs[k]
        mask = (comb > COMB_GUARD) & finite
        if mask.sum() < 5:
            continue
        d = observed[mask] - target[mask] - np.log(comb[mask])
        offset = float(np.median(d))
        cost = float(np.mean(np.abs(d - offset)))
        if cost < best[1]:
            best = (float(candidate), cost, offset)
    return best


def aperiodic_descriptors(x: np.ndarray, rate: float, f0: float,
                          b: float) -> dict[str, float]:
    """What the attack holds between its partials.

    Subtracting a fitted harmonic model does not work here: over an attack the
    partials glide and their decay is not the one the sustain shows, so the
    model's own misfit lands in the residual at the partial frequencies (on a
    low steel note three quarters of it did). The aperiodic part is therefore
    read where no partial can be: every bin further than two bins from any
    f_n = n f0 sqrt(1 + B n^2). The window is the same 6/f0 (never under 46 ms)
    the partial tracking uses, so its 4/W Hann main lobe always fits inside the
    partial spacing and two thirds of the band stays between partials.
    """
    length = int(min(max(6.0 / f0, ATTACK_SECONDS) * rate, len(x)))
    if length < 64:
        return {}
    segment = x[:length]
    window = np.hanning(length)
    power = np.abs(np.fft.rfft(segment * window)) ** 2
    freq = np.fft.rfftfreq(length, 1.0 / rate)
    resolution = rate / length
    between = np.ones(len(freq), dtype=bool)
    n = 1
    while True:
        fn = n * f0 * math.sqrt(1.0 + b * n * n)
        if fn > min(RESIDUAL_TOP_HZ, 0.45 * rate) + 4.0 * resolution:
            break
        between &= np.abs(freq - fn) > 2.0 * resolution
        n += 1
    band = (freq >= 50.0) & (freq <= RESIDUAL_TOP_HZ)
    total = float(power[band].sum())
    aperiodic = power[band & between]
    if not total > 0.0 or aperiodic.sum() <= 0.0:
        return {}
    aperiodic_freq = freq[band & between]
    share = float(aperiodic.sum()) / total
    return {
        "residual_db": 10.0 * math.log10(share),
        "centroid_hz": float((aperiodic_freq * aperiodic).sum() / aperiodic.sum()),
        "above_3k_share": float(aperiodic[aperiodic_freq > RESIDUAL_SPLIT_HZ].sum()
                                / aperiodic.sum()),
    }


# --------------------------------------------------------------- corpus layout

NOTE_PATTERN = re.compile(r"^(?P<root>[a-g]b?\d)_vl(?P<layer>\d)_rr(?P<rr>\d)_[12]\.wav$")
RELEASE_PATTERN = re.compile(r"^(?P<root>[a-g]b?\d)_release_rr(?P<rr>\d)_[12]\.wav$")


def sfz_roots(program: Path) -> dict[str, int]:
    """Root name -> MIDI key centre, from acoustic.sfz (default keycentre 60)."""
    text = program.read_text()
    roots: dict[str, int] = {}
    for group in text.split("<group>")[1:]:
        head = group.split("<region>")[0]
        fields = dict(re.findall(r"(\w+)=(\S+)", head))
        centre = int(fields.get("pitch_keycenter", 60))
        for region in group.split("<region>")[1:]:
            sample = re.search(r"sample=(\S+)", region)
            if not sample:
                continue
            name = sample.group(1).replace("\\", "/").split("/")[-1]
            match = NOTE_PATTERN.match(name)
            if match:
                roots[match.group("root")] = centre
    return roots


def take_paths(root_dir: Path, sensor: str) -> dict[tuple[str, int, int], Path]:
    folder = root_dir / ("Samples/acoustic" if sensor == "mic" else "Samples/electric")
    out: dict[tuple[str, int, int], Path] = {}
    for path in sorted(folder.glob("*.wav")):
        match = NOTE_PATTERN.match(path.name)
        if match:
            out[(match.group("root"), int(match.group("layer")),
                 int(match.group("rr")))] = path
    return out


def release_paths(root_dir: Path, sensor: str) -> dict[tuple[str, int], Path]:
    folder = root_dir / ("Samples/acoustic" if sensor == "mic" else "Samples/electric")
    out: dict[tuple[str, int], Path] = {}
    for path in sorted(folder.glob("*.wav")):
        match = RELEASE_PATTERN.match(path.name)
        if match:
            out[(match.group("root"), int(match.group("rr")))] = path
    return out


# ---------------------------------------------------------------- measurement

class Take:
    """One recording (or one probe render): its partials, comb and residual."""

    def __init__(self, rate: float, x: np.ndarray, midi: int,
                 path: Path | None = None,
                 tuning: tuple[float, float] | None = None) -> None:
        self.path = path
        self.rate = rate
        self.raw_peak = float(np.abs(x).max())
        self.clipped = bool((np.abs(x) >= 0.999).sum() >= 3)
        start = onset_index(x, rate)
        self.x = x[start:]
        window = int(min(LEVEL_SECONDS * rate, len(self.x)))
        self.peak = float(np.abs(self.x[:window]).max())
        self.rms = float(np.sqrt(np.mean(self.x[:window] ** 2)))
        if tuning is not None:
            # The microphone and the pickup are one take, so they are one
            # pitch; the pickup resolves it without the body's own modes
            # sitting inside the search band.
            self.f0, self.b = tuning
        else:
            self.f0 = refine_f0(self.x, rate, 440.0 * 2.0 ** ((midi - 69) / 12.0))
            self.b = inharmonicity(self.x, rate, self.f0, REPORT_HARMONICS + 10)
        self.times, self.tracks = partial_tracks(
            self.x, rate, self.f0, self.b, REPORT_HARMONICS + 10)
        self.levels: dict[int, float] = {}
        self.decays: dict[int, float] = {}
        for n, (amplitude, floor, _) in self.tracks.items():
            span = usable_range(self.times, amplitude, floor)
            if span is None:
                continue
            fit = fit_onset_level(self.times, amplitude, span[0], span[1])
            if fit is None:
                continue
            self.levels[n], self.decays[n] = fit
        self.pluck = float("nan")
        self.aperiodic = aperiodic_descriptors(self.x, rate, self.f0, self.b)

    @classmethod
    def from_wav(cls, path: Path, midi: int,
                 tuning: tuple[float, float] | None = None) -> "Take":
        rate, x = read_mono(path)
        return cls(rate, x, midi, path, tuning)

    @classmethod
    def from_probe(cls, path: Path, midi: int) -> "Take":
        rate, x = read_probe(path)
        return cls(rate, x, midi, path)

    def ratio_db(self, other: "Take", n: int) -> float | None:
        """Comb-corrected level of this take over the other's, at harmonic n."""
        pair = self.paired_levels(other, n)
        if pair is None:
            return None
        mine = abs(math.sin(math.pi * n * self.pluck))
        theirs = abs(math.sin(math.pi * n * other.pluck))
        if min(mine, theirs) < COMB_GUARD:
            return None
        return 20.0 * math.log10((pair[0] / mine) / (pair[1] / theirs))

    def paired_levels(self, other: "Take", n: int) -> tuple[float, float] | None:
        if n not in self.tracks or n not in other.tracks:
            return None
        mine = usable_range(self.times, *self.tracks[n][:2])
        theirs = usable_range(other.times, *other.tracks[n][:2])
        if mine is None or theirs is None:
            return None
        low, high = max(mine[0], theirs[0]), min(mine[1], theirs[1])
        if high - low < MIN_FIT_SPAN_SECONDS:
            return None
        a = fit_onset_level(self.times, self.tracks[n][0], low, high)
        b = fit_onset_level(other.times, other.tracks[n][0], low, high)
        if a is None or b is None:
            return None
        return a[0], b[0]


def mad(values: np.ndarray) -> float:
    values = values[np.isfinite(values)]
    if values.size == 0:
        return float("nan")
    return float(np.median(np.abs(values - np.median(values))))


def measure_corpus(root_dir: Path, roots: dict[str, int],
                   limit: int | None) -> dict[str, dict]:
    paths = {sensor: take_paths(root_dir, sensor) for sensor in ("pickup", "mic")}
    names = sorted(roots, key=lambda r: roots[r])
    if limit:
        names = names[:limit]
    out: dict[str, dict] = {"pickup": {}, "mic": {}}
    for name in names:
        midi = roots[name]
        for sensor in ("pickup", "mic"):
            takes: dict[tuple[int, int], Take] = {}
            for layer in (1, 2, 3, 4):
                for rr in (1, 2, 3, 4):
                    path = paths[sensor].get((name, layer, rr))
                    if path is None:
                        continue
                    tuning = None
                    if sensor == "mic":
                        pickup = out["pickup"].get(name, {}).get(
                            "takes", {}).get((layer, rr))
                        if pickup is not None:
                            tuning = (pickup.f0, pickup.b)
                    takes[(layer, rr)] = Take.from_wav(path, midi, tuning)
            if not takes:
                continue
            keys = sorted(takes)
            if sensor == "mic" and name in out["pickup"]:
                # Where the string was taken is a property of the take, not of
                # the sensor that heard it. The pickup reads the comb without
                # the body's modes sitting on the partials, so the microphone
                # takes the pickup's plucking points rather than re-fitting a
                # second, noisier set for the same hand. The pickup lays its
                # own position comb |sin(n pi d)| over the string's motion, so
                # this could import a bias; measured, it does not. Fitting p on
                # the microphone takes alone over the five lowest roots and
                # differencing against the borrowed values gives medians
                # +0.0116, +0.0010, +0.0002, +0.0054 and +0.0166 of the string
                # length with MADs 0.0114 to 0.0358 -- at or inside the 0.02
                # take-to-take spread the decision log already measures, and so
                # under the second pass's half width below.
                pluck = [out["pickup"][name]["takes"][k].pluck
                         if k in out["pickup"][name]["takes"] else float("nan")
                         for k in keys]
            else:
                pluck = refine_pluck_points([takes[k].levels for k in keys],
                                            [k[0] for k in keys], REPORT_HARMONICS)
            for key, value in zip(keys, pluck):
                takes[key].pluck = value
            out[sensor][name] = {"midi": midi, "takes": takes}
            print(f"  {sensor} {name} (MIDI {midi}) f0={takes[keys[0]].f0:.2f} Hz "
                  f"B={takes[keys[0]].b:.3e} p={np.round(pluck, 3).tolist()}",
                  file=sys.stderr)
    return out


def _curve(pool: dict[int, list[float]], within_root: list[float]) -> dict:
    curve = {}
    for n, values in pool.items():
        if len(values) < 4:
            continue
        array = np.array(values)
        curve[str(n)] = {"median_db": float(np.median(array)),
                         "mad_db": mad(array), "pairs": int(array.size)}
    band = [curve[str(n)]["median_db"] for n in range(5, 13) if str(n) in curve]
    return {
        "per_harmonic": curve,
        "h5_h12_median_db": float(np.median(band)) if band else float("nan"),
        "round_robin_pair_mad_db": float(np.median(np.array(within_root)))
        if within_root else float("nan"),
    }


def layer_ratios(per_root: dict) -> dict:
    """Layer-over-softest excitation ratio per harmonic, pooled over takes.

    Every round-robin pair of one root contributes one ratio per harmonic; the
    pool is taken over pairs and roots together, with the median absolute
    deviation as its spread. Both the comb-divided ratio and the ratio with the
    plucking point left in are reported: dividing the comb out corrects a real
    difference between two takes but multiplies the plucking-point estimate's
    own error by n, so which of the two is the tighter estimate is a
    measurement, not a choice.
    """
    out: dict[str, dict] = {}
    for layer in (2, 3, 4):
        corrected: dict[int, list[float]] = {n: [] for n in range(1, REPORT_HARMONICS + 1)}
        raw: dict[int, list[float]] = {n: [] for n in corrected}
        within_corrected: list[float] = []
        within_raw: list[float] = []
        pluck_shift: list[float] = []
        for entry in per_root.values():
            takes = entry["takes"]
            root_corrected: dict[int, list[float]] = {n: [] for n in corrected}
            root_raw: dict[int, list[float]] = {n: [] for n in corrected}
            for rr_loud in (1, 2, 3, 4):
                loud = takes.get((layer, rr_loud))
                # A take that reaches digital full scale is not a linear
                # reading of the pluck, and the loudest layer has several.
                if loud is None or loud.clipped:
                    continue
                for rr_soft in (1, 2, 3, 4):
                    soft = takes.get((1, rr_soft))
                    if soft is None or soft.clipped:
                        continue
                    pluck_shift.append(loud.pluck - soft.pluck)
                    for n in corrected:
                        pair = loud.paired_levels(soft, n)
                        if pair is None:
                            continue
                        root_raw[n].append(20.0 * math.log10(pair[0] / pair[1]))
                        comb_loud = abs(math.sin(math.pi * n * loud.pluck))
                        comb_soft = abs(math.sin(math.pi * n * soft.pluck))
                        if min(comb_loud, comb_soft) < COMB_GUARD:
                            continue
                        root_corrected[n].append(
                            20.0 * math.log10((pair[0] / comb_loud)
                                              / (pair[1] / comb_soft)))
            for n in corrected:
                if len(root_corrected[n]) >= 4:
                    corrected[n].extend(root_corrected[n])
                    within_corrected.append(mad(np.array(root_corrected[n])))
                if len(root_raw[n]) >= 4:
                    raw[n].extend(root_raw[n])
                    within_raw.append(mad(np.array(root_raw[n])))
        entry = {"comb_divided": _curve(corrected, within_corrected),
                 "comb_left_in": _curve(raw, within_raw)}
        entry["pluck_point_shift"] = {
            "median": float(np.median(pluck_shift)) if pluck_shift else float("nan"),
            "mad": mad(np.array(pluck_shift)) if pluck_shift else float("nan"),
        }
        out[f"vl{layer}_over_vl1"] = entry
    return out


def layer_summary(per_root: dict) -> dict:
    out = {}
    for layer in (1, 2, 3, 4):
        residual, centroid, share, peak, rms, clipped = [], [], [], [], [], 0
        t60_low, t60_high = [], []
        for entry in per_root.values():
            softest = [entry["takes"][(1, rr)] for rr in (1, 2, 3, 4)
                       if (1, rr) in entry["takes"]]
            reference_peak = float(np.median([t.peak for t in softest])) if softest else float("nan")
            reference_rms = float(np.median([t.rms for t in softest])) if softest else float("nan")
            for rr in (1, 2, 3, 4):
                take = entry["takes"].get((layer, rr))
                if take is None:
                    continue
                if take.aperiodic:
                    residual.append(take.aperiodic["residual_db"])
                    centroid.append(take.aperiodic["centroid_hz"])
                    share.append(take.aperiodic["above_3k_share"])
                if reference_peak > 0:
                    peak.append(20.0 * math.log10(take.peak / reference_peak))
                if reference_rms > 0:
                    rms.append(20.0 * math.log10(take.rms / reference_rms))
                clipped += int(take.clipped)
                # T60 = 6.91 tau. A pluck that is only louder decays at the
                # same rate; one that drives the string harder need not.
                if 1 in take.decays:
                    t60_low.append(6.91 * take.decays[1])
                upper = [6.91 * take.decays[n] for n in range(5, 13)
                         if n in take.decays]
                if upper:
                    t60_high.append(float(np.median(upper)))
        out[f"vl{layer}"] = {
            "aperiodic_level_db": float(np.median(residual)) if residual else float("nan"),
            "aperiodic_level_mad_db": mad(np.array(residual)) if residual else float("nan"),
            "aperiodic_centroid_hz": float(np.median(centroid)) if centroid else float("nan"),
            "aperiodic_centroid_mad_hz": mad(np.array(centroid)) if centroid else float("nan"),
            "aperiodic_above_3k_share": float(np.median(share)) if share else float("nan"),
            "aperiodic_above_3k_mad": mad(np.array(share)) if share else float("nan"),
            "peak_over_vl1_db": float(np.median(peak)) if peak else float("nan"),
            "peak_over_vl1_mad_db": mad(np.array(peak)) if peak else float("nan"),
            "rms_over_vl1_db": float(np.median(rms)) if rms else float("nan"),
            "rms_over_vl1_mad_db": mad(np.array(rms)) if rms else float("nan"),
            "h1_t60_seconds": float(np.median(t60_low)) if t60_low else float("nan"),
            "h1_t60_mad_seconds": mad(np.array(t60_low)) if t60_low else float("nan"),
            "h5_h12_t60_seconds": float(np.median(t60_high)) if t60_high else float("nan"),
            "h5_h12_t60_mad_seconds": mad(np.array(t60_high)) if t60_high else float("nan"),
            "takes": sum(1 for e in per_root.values()
                         for rr in (1, 2, 3, 4) if (layer, rr) in e["takes"]),
            "full_scale_takes": clipped,
        }
    return out


def release_summary(root_dir: Path, sensor: str, per_root: dict) -> dict:
    paths = release_paths(root_dir, sensor)
    peak_db, decay_ms, centroid = [], [], []
    for name, entry in per_root.items():
        softest = [entry["takes"][(1, rr)] for rr in (1, 2, 3, 4)
                   if (1, rr) in entry["takes"]]
        if not softest:
            continue
        reference = float(np.median([t.peak for t in softest]))
        for rr in range(1, 9):
            path = paths.get((name, rr))
            if path is None:
                continue
            rate, x = read_mono(path)
            x = x[onset_index(x, rate):]
            if len(x) < 64 or reference <= 0:
                continue
            peak = float(np.abs(x).max())
            if peak <= 0:
                continue
            peak_db.append(20.0 * math.log10(peak / reference))
            # A release take is broadband, so its instantaneous magnitude
            # crosses any threshold within a cycle; the fall is read off a
            # 5 ms RMS envelope instead.
            frame = max(1, int(0.005 * rate))
            frames = len(x) // frame
            envelope = np.sqrt((x[:frames * frame].reshape(frames, frame) ** 2)
                               .mean(axis=1))
            index = int(np.argmax(envelope))
            after = np.where(envelope[index:] < envelope[index] * 10.0 ** (-40.0 / 20.0))[0]
            if after.size:
                decay_ms.append(1000.0 * float(after[0]) * frame / rate)
            window = np.hanning(len(x))
            spectrum = np.abs(np.fft.rfft(x * window)) ** 2
            freq = np.fft.rfftfreq(len(x), 1.0 / rate)
            band = (freq >= 50.0) & (freq <= RESIDUAL_TOP_HZ)
            total = float(spectrum[band].sum())
            if total > 0:
                centroid.append(float((freq[band] * spectrum[band]).sum() / total))
    return {
        "peak_over_vl1_peak_db": float(np.median(peak_db)) if peak_db else float("nan"),
        "peak_over_vl1_peak_mad_db": mad(np.array(peak_db)) if peak_db else float("nan"),
        "minus40_db_time_ms": float(np.median(decay_ms)) if decay_ms else float("nan"),
        "minus40_db_time_mad_ms": mad(np.array(decay_ms)) if decay_ms else float("nan"),
        "centroid_hz": float(np.median(centroid)) if centroid else float("nan"),
        "centroid_mad_hz": mad(np.array(centroid)) if centroid else float("nan"),
        "takes": len(peak_db),
    }


def paired_sensor_agreement(measured: dict[str, dict]) -> dict:
    """The same round-robin pair read twice: pickup ratio minus mic ratio.

    Both sensors hear one pluck, so their layer ratios differ only by their own
    linear transfers, which each ratio has already cancelled. Comparing the two
    on the same pair rather than on two pooled medians keeps the takes' own
    spread out of the comparison, which is what makes this a check on the
    method and not on the player.
    """
    out: dict[str, dict] = {}
    for layer in (2, 3, 4):
        differences: dict[int, list[float]] = {}
        for name, entry in measured["pickup"].items():
            mic_entry = measured["mic"].get(name)
            if mic_entry is None:
                continue
            for rr_loud in (1, 2, 3, 4):
                for rr_soft in (1, 2, 3, 4):
                    keys = ((layer, rr_loud), (1, rr_soft))
                    if any(k not in entry["takes"] or k not in mic_entry["takes"]
                           for k in keys):
                        continue
                    if any(entry["takes"][k].clipped or mic_entry["takes"][k].clipped
                           for k in keys):
                        continue
                    for n in range(1, REPORT_HARMONICS + 1):
                        a = entry["takes"][keys[0]].ratio_db(
                            entry["takes"][keys[1]], n)
                        b = mic_entry["takes"][keys[0]].ratio_db(
                            mic_entry["takes"][keys[1]], n)
                        if a is None or b is None:
                            continue
                        differences.setdefault(n, []).append(a - b)
        pooled = [v for values in differences.values() for v in values]
        per_harmonic = {}
        for n, values in sorted(differences.items()):
            if len(values) < 4:
                continue
            array = np.array(values)
            per_harmonic[str(n)] = {"median_db": float(np.median(array)),
                                    "mad_db": mad(array), "pairs": int(array.size)}
        out[f"vl{layer}_over_vl1"] = {
            "per_harmonic_difference": per_harmonic,
            "median_abs_difference_db": float(np.median(np.abs(np.array(pooled))))
            if pooled else float("nan"),
            "mad_db": mad(np.array(pooled)) if pooled else float("nan"),
            "pairs": len(pooled),
        }
    return out


# ------------------------------------------------------------------ self-test

PROBE_CONTROLS = (0.0, 0.2, 0.4, 0.5, 0.6, 0.8, 1.0)
PROBE_CONDITIONS = (("coupled", "on", "on"),
                    ("no-sympathy", "on", "off"),
                    ("decoupled", "off", "off"))


def probe_render(probe: Path, work: Path, condition: str, coupling: str,
                 sympathy: str, control: float, midi: int) -> Path:
    out = work / f"probe-{condition}-{control:.1f}.f32"
    subprocess.run([str(probe), str(out), "steel", coupling, f"{control}",
                    "dreadnought", str(midi), sympathy],
                   check=True, stdout=subprocess.DEVNULL)
    return out


def synthetic_check(rate: float = 44100.0) -> dict:
    """Exactly known excitation: decaying inharmonic partials at a known comb."""
    rng = np.random.default_rng(20260903)
    f0, b, p = 110.0, 4.0e-5, 0.1731
    duration = int(2.0 * rate)
    t = np.arange(duration) / rate
    body = np.exp(rng.normal(0.0, 0.35, REPORT_HARMONICS + 10))
    truth: dict[int, float] = {}
    x = np.zeros(duration)
    for n in range(1, REPORT_HARMONICS + 11):
        fn = n * f0 * math.sqrt(1.0 + b * n * n)
        if fn > 0.45 * rate:
            break
        level = abs(math.sin(math.pi * n * p)) / n ** 2 * body[n - 1]
        tau = 1.4 / (1.0 + 0.09 * n)
        truth[n] = level
        x += level * np.exp(-t / tau) * np.sin(2.0 * np.pi * fn * t
                                               + rng.uniform(0.0, 2.0 * np.pi))
    burst = rng.normal(0.0, 1.0, duration) * np.exp(-t / 0.0035)
    x += 0.02 * x.std() * burst / max(burst.std(), 1e-12)
    x += rng.normal(0.0, x.std() * 10.0 ** (-60.0 / 20.0), duration)
    times, tracks = partial_tracks(x, rate, refine_f0(x, rate, f0),
                                   inharmonicity(x, rate, f0, REPORT_HARMONICS + 10),
                                   REPORT_HARMONICS + 10)
    errors = []
    for n in range(1, 13):
        if n not in tracks:
            continue
        span = usable_range(times, *tracks[n][:2])
        if span is None:
            continue
        fit = fit_onset_level(times, tracks[n][0], span[0], span[1])
        if fit is None:
            continue
        errors.append(20.0 * math.log10(fit[0] / truth[n]))
    array = np.array(errors) - np.median(errors)
    return {"harmonics": int(array.size),
            "max_abs_error_db": float(np.max(np.abs(array))),
            "rms_error_db": float(np.sqrt(np.mean(array ** 2)))}


def self_test(probe: Path, work: Path, midi: int = 40) -> dict:
    work.mkdir(parents=True, exist_ok=True)
    report = {"synthetic": synthetic_check()}
    recovered: dict[str, dict] = {}
    for condition, coupling, sympathy in PROBE_CONDITIONS:
        takes = []
        for control in PROBE_CONTROLS:
            path = probe_render(probe, work, condition, coupling, sympathy,
                                control, midi)
            takes.append(Take.from_probe(path, midi))
        # The probe sweep moves the plucking point on purpose across the whole
        # of the engine's range, which is what the corpus pass's "one hand, one
        # place" second pass exists to rule out, so it is switched off here.
        pluck = refine_pluck_points([t.levels for t in takes],
                                    [0] * len(takes), REPORT_HARMONICS,
                                    second_pass=False)
        for take, value in zip(takes, pluck):
            take.pluck = value
        # Every render has the same excitation but its own plucking point, so
        # each one's ratio to the first must come out flat once the two combs
        # are divided out. The pairing uses one common frame window, as the
        # corpus ratios do.
        table = np.full((len(takes), 12), np.nan)
        for i, take in enumerate(takes):
            for n in range(1, 13):
                value = take.ratio_db(takes[0], n)
                if value is not None:
                    table[i, n - 1] = value
        # Referencing every row to the column median rather than to the first
        # render keeps one render's own error out of all the others' rows.
        with np.errstate(invalid="ignore"):
            deviation = table - np.nanmedian(table, axis=0)
        deviation = deviation - np.nanmedian(deviation, axis=1)[:, None]
        control = np.array(PROBE_CONTROLS)
        design = np.vstack([np.ones_like(control), control]).T
        coefficients, *_ = np.linalg.lstsq(design, np.array(pluck), rcond=None)
        recovered[condition] = {
            "pluck_points": [round(v, 4) for v in pluck],
            "pluck_affine_intercept": float(coefficients[0]),
            "pluck_affine_slope": float(coefficients[1]),
            "pluck_affine_max_deviation": float(
                np.max(np.abs(np.array(pluck) - design @ coefficients))),
            "recovery_max_abs_db": float(np.nanmax(np.abs(deviation))),
            "recovery_rms_db": float(np.sqrt(np.nanmean(deviation ** 2))),
            "recovery_harmonics": int(np.isfinite(deviation).sum()),
            "_ratio": table[-1].tolist(),
        }
    base = np.array(recovered["coupled"]["_ratio"])
    body_free = {}
    for condition, _, _ in PROBE_CONDITIONS[1:]:
        other = np.array(recovered[condition]["_ratio"])
        difference = other - base
        difference -= np.nanmedian(difference)
        body_free[condition] = {
            "max_abs_db": float(np.nanmax(np.abs(difference))),
            "rms_db": float(np.sqrt(np.nanmean(difference ** 2))),
            "harmonics": int(np.isfinite(difference).sum()),
        }
    for entry in recovered.values():
        entry.pop("_ratio")
    report["engine"] = recovered
    report["body_free_ratio_agreement"] = body_free
    return report


# ------------------------------------------------------------------------ main

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--shiny", type=Path,
                        help="karoryfer.shinyguitar checkout at the pinned commit")
    parser.add_argument("--output", type=Path, help="JSON report to write")
    parser.add_argument("--limit", type=int, default=0,
                        help="measure only the lowest N roots")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--probe", type=Path, help="AcustraBridgeProbe binary")
    parser.add_argument("--work", type=Path, default=Path("."),
                        help="directory for self-test renders")
    args = parser.parse_args()

    if args.self_test:
        if args.probe is None:
            parser.error("--self-test needs --probe")
        report = self_test(args.probe, args.work)
        text = json.dumps(report, indent=2, sort_keys=True)
        if args.output:
            args.output.write_text(text + "\n")
        print(text)
        # Only the synthetic case has an exactly known excitation, so only it
        # can fail the tool. The engine numbers beside it are a reading of what
        # the engine's own released shape does, not a pass or a fail.
        failed = report["synthetic"]["max_abs_error_db"] > 1.0
        if failed:
            print("FAIL: known excitation not recovered within 1 dB to H12",
                  file=sys.stderr)
        return 1 if failed else 0

    if args.shiny is None or args.output is None:
        parser.error("--shiny and --output are required")
    roots = sfz_roots(args.shiny / "Programs/acoustic.sfz")
    if not roots:
        raise SystemExit("no sustain regions found in Programs/acoustic.sfz")
    measured = measure_corpus(args.shiny, roots, args.limit or None)
    reports: dict[str, dict] = {}
    for sensor in ("pickup", "mic"):
        per_root = measured[sensor]
        reports[sensor] = {
            "layer_ratio_db": layer_ratios(per_root),
            "per_layer": layer_summary(per_root),
            "release": release_summary(args.shiny, sensor, per_root),
            "pluck_points": {name: {f"vl{l}_rr{r}": round(t.pluck, 4)
                                    for (l, r), t in entry["takes"].items()}
                             for name, entry in per_root.items()},
            "roots": {name: {"midi": entry["midi"],
                             "f0_hz": round(entry["takes"][sorted(entry["takes"])[0]].f0, 3),
                             "inharmonicity_b": float(
                                 entry["takes"][sorted(entry["takes"])[0]].b)}
                      for name, entry in per_root.items()},
        }
    reports["sensor_agreement"] = paired_sensor_agreement(measured)
    reports["protocol"] = {
        "comb_guard": COMB_GUARD,
        "partial_snr": PARTIAL_SNR,
        "track_seconds": TRACK_SECONDS,
        "attack_seconds": ATTACK_SECONDS,
        "level_seconds": LEVEL_SECONDS,
        "residual_band_hz": [50.0, RESIDUAL_TOP_HZ],
        "residual_split_hz": RESIDUAL_SPLIT_HZ,
        "harmonics": REPORT_HARMONICS,
        "min_fit_frames": MIN_FIT_FRAMES,
        "min_fit_span_seconds": MIN_FIT_SPAN_SECONDS,
        # The comb is symmetric under p -> 1 - p, so this range is what makes
        # every reported plucking point a distance from the bridge rather than
        # from the nut. It bounds a reported result, so it is reported with it.
        "pluck_range": list(PLUCK_RANGE),
        "pluck_step": PLUCK_STEP,
        "pluck_second_pass_half_width": PLUCK_SECOND_PASS_HALF_WIDTH,
    }
    args.output.write_text(json.dumps(reports, indent=2, sort_keys=True) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
