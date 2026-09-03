#!/usr/bin/env python3
"""Compare modelled and recorded string damping as a curve against frequency.

Protocol. For every example in a rendered physical-fit corpus this reads the
recording and the model render that the fit renderer wrote side by side, and
estimates one early decay rate per partial:

* the signal is aligned on its own 2%-of-peak onset and analysed with an
  8192-point Hann STFT at 7/8 overlap;
* a partial contributes only where its band peak stays above twelve times the
  local interharmonic floor (the median power between that partial and the
  next) and within 45 dB of its own peak, which keeps a recording's noise floor
  and a room tail out of the estimate;
* the rate is the least-squares slope of that band's level in dB over
  0.15-1.2 s, so it measures early decay, not the settled tail;
* rates are pooled by absolute partial frequency, never by harmonic number,
  because string damping is a property of frequency rather than of which note
  produced it. Recording and model are pooled independently, so a band is only
  comparable where both report a usable count.

Reported medians are dB/s: larger means faster decay. The fitter's own decay
term also works in dB/s, but it reports one robust rate per partial over
0.12-4.0 s and compares it note by note, so it cannot show where on the
frequency axis a damping error sits; this can.

A partial that lands on one of the termination's own artefact frequencies is
skipped on both sides, so no bridge or anchor candidate can be rewarded for a
coincidence with a partial; ARTEFACT_HZ says which and why.

Usage:

    python3 Tools/AuditDampingCurve.py /path/to/rendered-fit-corpus
    python3 Tools/AuditDampingCurve.py --doublets /path/to/corpus
    python3 Tools/AuditDampingCurve.py --self-test

``--doublets`` replaces the single slope with one or two damped exponentials
per partial, chosen by an F-test: a coupled string does not decay along one
line and a slope through a beating envelope is neither member's rate. See
``partial_components``.

NumPy and SciPy are required.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np
from scipy.signal import stft
from scipy.stats import f as fisher

BAND_EDGES = np.asarray(
    [80, 120, 180, 270, 400, 600, 900, 1350, 2000, 3000, 4500, 6800, 9000],
    dtype=float,
)
MINIMUM_BAND_COUNT = 8
WINDOW = 8192
# A band is a measurement only if it survives its own analysis window.
CHECK_WINDOW = 4096
STABILITY_TOLERANCE_DB_PER_SECOND = 3.0
ANALYSIS_SECONDS = 3.2
RATE_WINDOW_SECONDS = (0.15, 1.2)
FLOOR_MARGIN = 12.0
PEAK_RANGE_DB = 45.0

# The shipping termination is DAFx-26's 3.25 mm anchor stub, and the 2026-09-02
# entry measured where the lumped spring in series with the bridge's mass-like
# mobility resonates for each candidate length: 102 and 233 Hz at the fitted
# 17.2 mm, 306, 602 and 668 Hz at the stub. Those are an artefact of the
# termination, not string damping, and a partial that lands on one of them
# decays at the artefact's rate. Both sides are masked there so a band's
# comparison is over the same frequency support, and so no future termination
# can be rewarded for a coincidence with a partial.
ARTEFACT_HZ = (306.0, 602.0, 668.0)
ARTEFACT_MASK_CENTS = 50.0
# Two damped exponentials cost four more real parameters than one; a partial is
# called a doublet only when that buys a residual drop this unlikely by chance.
DOUBLET_SIGNIFICANCE = 0.01
# An F-test alone is not enough where the band is nearly noiseless, because the
# second exponential can then reduce an already numerical residual and be
# significant while sitting 200 dB down. A doublet is two lines of comparable
# strength; below this the weaker line is the fit chasing the floor.
DOUBLET_MAX_LEVEL_DIFFERENCE_DB = 40.0
# The band is taken as a slice of the record's own spectrum, so it is circular:
# the loud start and quiet end of a decaying partial meet, and that step is a
# short broadband transient at the edges of the returned envelope. Dropping a
# few samples from each end removes it; a damped exponential's rate does not
# depend on where the record starts.
BASEBAND_EDGE_SAMPLES = 4
# A doublet member outside the middle of the pass band is the band filter's own
# edge rather than a mode. Real coupled splits are small: the 2026-09-02 entry
# measured -40/+40 cents at 233 Hz, about 5 Hz.
DOUBLET_IN_BAND_FRACTION = 0.5


def _read(path: Path, channels: int) -> np.ndarray:
    data = np.fromfile(path, dtype="<f4")
    if channels == 2:
        data = data.reshape(-1, 2).mean(axis=1)
    return data.astype(np.float64)


def _onset(signal: np.ndarray) -> int:
    peak = float(np.max(np.abs(signal))) if signal.size else 0.0
    if peak <= 0.0:
        return 0
    return int(np.argmax(np.abs(signal) > 0.02 * peak))


def measurable_partials(
    signal: np.ndarray, rate: int, fundamental: float, maximum_partial: int = 40,
    window: int | None = None,
) -> list[tuple[int, float, np.ndarray, np.ndarray]]:
    """Per partial that is above the record's own noise: (index, Hz, t, dB).

    This is the analysis floor both estimators need and neither may skip. A
    partial whose band never rises FLOOR_MARGIN above the interharmonic floor
    beside it is the record's noise, and anything fitted to it is a rate for
    noise -- with two exponentials, a doublet for noise. The returned times and
    levels are the frames that survive the floor, the peak range and the rate
    window, so a caller either fits those or, like the doublet estimator, takes
    the partial's admission and re-reads the waveform its own way.
    """
    tail = signal[_onset(signal) : _onset(signal) + round(ANALYSIS_SECONDS * rate)]
    window = min(window or WINDOW,
                 1 << int(np.floor(np.log2(max(tail.size, 2)))))
    if tail.size < 4 * window or window < 1024:
        return []
    frequency, times, spectrum = stft(
        tail, fs=rate, window="hann", nperseg=window,
        noverlap=7 * window // 8, nfft=window, boundary=None, padded=False,
    )
    power = np.abs(spectrum) ** 2
    resolution = float(frequency[1] - frequency[0])
    result: list[tuple[int, float, np.ndarray, np.ndarray]] = []
    for index in range(1, maximum_partial + 1):
        centre = fundamental * index
        if centre > min(9000.0, 0.45 * rate):
            break
        if masked_by_artefact(centre):
            continue
        half_width = max(resolution, centre * 0.010)
        band = (frequency >= centre - half_width) & (frequency <= centre + half_width)
        gap = (frequency >= centre + 2 * half_width) & (
            frequency <= centre + fundamental - 2 * half_width
        )
        if not np.any(band) or not np.any(gap):
            continue
        curve = power[band].max(axis=0)
        floor = float(np.median(power[gap].mean(axis=1)))
        levels = 10.0 * np.log10(np.maximum(curve, 1.0e-30))
        usable = (
            (times >= RATE_WINDOW_SECONDS[0])
            & (times <= RATE_WINDOW_SECONDS[1])
            & (curve > FLOOR_MARGIN * floor)
            & (levels > levels.max() - PEAK_RANGE_DB)
        )
        if int(np.count_nonzero(usable)) < 10:
            continue
        result.append((index, centre, times[usable], levels[usable]))
    return result


def partial_rates(
    signal: np.ndarray, rate: int, fundamental: float, maximum_partial: int = 40,
    window: int | None = None,
) -> list[tuple[float, float]]:
    """Return (partial frequency, decay rate in dB/s) for usable partials."""
    result: list[tuple[float, float]] = []
    for _, centre, times, levels in measurable_partials(
            signal, rate, fundamental, maximum_partial, window):
        slope = float(np.polyfit(times, levels, 1)[0])
        if slope < -1.0:
            result.append((centre, -slope))
    return result


def masked_by_artefact(frequency: float) -> bool:
    return any(abs(1200.0 * np.log2(frequency / artefact)) < ARTEFACT_MASK_CENTS
               for artefact in ARTEFACT_HZ)


def _baseband(signal: np.ndarray, rate: int, centre: float,
              half_width_hz: float,
              spectrum: np.ndarray | None = None
              ) -> tuple[np.ndarray, float, float] | None:
    """The band around `centre`, heterodyned to DC and decimated.

    A contiguous slice of the signal's spectrum, reordered so the partial's own
    bin is at DC and inverse transformed, is the band's complex envelope over
    the same span of time at the lowest rate that carries it. That is what a
    damped-exponential fit wants: one partial and its neighbourhood, a
    hundred-odd samples instead of fifty thousand. The slice is tapered rather
    than cut square, because a brick wall puts a component of its own at each
    band edge and the fit then reports that edge as a second line. Filtering is
    linear and time invariant, so it leaves every pole inside the band where it
    was. Returns the samples, the frequency that is now DC, and their rate.
    """
    count = signal.size
    if spectrum is None:
        spectrum = np.fft.fft(signal)
    centre_bin = int(round(centre * count / rate))
    half_bins = int(round(half_width_hz * count / rate))
    if half_bins < 12 or centre_bin - half_bins < 1:
        return None
    if centre_bin + half_bins + 1 > count // 2:
        return None
    band = np.concatenate((
        spectrum[centre_bin : centre_bin + half_bins + 1],
        spectrum[centre_bin - half_bins : centre_bin],
    ))
    offset = np.concatenate((np.arange(half_bins + 1),
                             np.arange(-half_bins, 0)))
    band = band * (0.5 * (1.0 + np.cos(np.pi * offset / (half_bins + 1))))
    samples = np.fft.ifft(band)
    trimmed = samples[BASEBAND_EDGE_SAMPLES : samples.size - BASEBAND_EDGE_SAMPLES]
    return (trimmed, centre_bin * rate / count, band.size * rate / count)


def _damped_fit(samples: np.ndarray, order: int
                ) -> tuple[np.ndarray, np.ndarray, float] | None:
    """Fit `order` damped complex exponentials by the matrix-pencil method.

    Lee, Smith and Valimaki, "Analysis and synthesis of coupled vibrating
    strings using a hybrid modal/waveguide synthesis model", IEEE TASLP 18(4),
    2010, estimate coupled-string doublets this way; the pencil form is Hua and
    Sarkar, IEEE TASSP 38(5), 1990. Returns the poles, their amplitudes and the
    residual energy, or None when the fit is not usable.
    """
    count = samples.size
    pencil = max(order + 1, count // 3)
    if count - pencil < order + 1:
        return None
    hankel = np.lib.stride_tricks.sliding_window_view(samples, pencil + 1)
    _, _, right = np.linalg.svd(hankel, full_matrices=False)
    # Rows of V^H, transposed without conjugating: for y[n] = z^n the Hankel
    # rows are z^r [1, z, ... z^L], so V's own column is the conjugate of that
    # Vandermonde vector and taking it would return every pole conjugated,
    # which reads a partial 1 Hz above the bin as 1 Hz below it.
    basis = right[:order].T
    if basis.shape[0] < 2:
        return None
    poles = np.linalg.eigvals(np.linalg.pinv(basis[:-1]) @ basis[1:])
    # A growing component is not a decay rate, and its Vandermonde column
    # overflows; reject the whole fit rather than report it.
    if np.any(np.abs(poles) >= 1.0) or np.any(np.abs(poles) <= 0.0):
        return None
    vandermonde = poles ** np.arange(count)[:, None]
    amplitudes, *_ = np.linalg.lstsq(vandermonde, samples, rcond=None)
    residual = samples - vandermonde @ amplitudes
    return poles, amplitudes, float(np.vdot(residual, residual).real)


def _components(poles: np.ndarray, amplitudes: np.ndarray, centre: float,
                band_rate: float) -> list[tuple[float, float, float]]:
    """(frequency Hz, decay rate dB/s, amplitude dB) per pole, loudest first.

    Levels are referred back over the samples the baseband trimmed, so they are
    the levels at the start of the rate window rather than a few samples into
    it: two members with different rates separate by a real amount over those
    samples and the reported difference would otherwise depend on the trim.
    """
    rows = [
        (centre + float(np.angle(pole)) * band_rate / (2.0 * np.pi),
         -20.0 * float(np.log10(abs(pole))) * band_rate,
         20.0 * float(np.log10(max(
             abs(amplitude / pole ** BASEBAND_EDGE_SAMPLES), 1.0e-30))))
        for pole, amplitude in zip(poles, amplitudes)
    ]
    return sorted(rows, key=lambda row: -row[2])


def partial_components(
    signal: np.ndarray, rate: int, fundamental: float,
    maximum_partial: int = 40,
) -> list[dict]:
    """Per partial, one damped exponential or two, decided by an F-test.

    A string terminated on a mobile bridge does not decay along one line: two
    polarisations, or a string and a body mode it is coupled to, beat, and a
    least-squares slope through a beating envelope is neither member's rate.
    This fits one damped exponential and then two, and keeps the second only
    when the residual drop is significant at DOUBLET_SIGNIFICANCE. Where it is
    not, the single rate is what is reported, so this is a strict extension of
    the one-line estimator rather than a different one -- including its analysis
    floor: only the partials `measurable_partials` admits are fitted here, so a
    partial the single-line estimator throws out as noise cannot come back as a
    doublet.
    """
    onset = _onset(signal)
    first = onset + round(RATE_WINDOW_SECONDS[0] * rate)
    last = onset + round(RATE_WINDOW_SECONDS[1] * rate)
    window = signal[first:min(last, signal.size)]
    if window.size < 4096:
        return []
    spectrum = np.fft.fft(window)
    result: list[dict] = []
    for index, centre, _, _ in measurable_partials(
            signal, rate, fundamental, maximum_partial):
        half_width = min(0.5 * fundamental, 50.0)
        band = _baseband(window, rate, centre, half_width, spectrum)
        if band is None or band[0].size < 24:
            continue
        samples, band_centre, band_rate = band
        single = _damped_fit(samples, 1)
        if single is None:
            continue
        double = _damped_fit(samples, 2)
        observations = 2 * samples.size
        significant = False
        if double is not None and double[2] > 0.0 and observations > 12:
            statistic = ((single[2] - double[2]) / 4.0) / (
                double[2] / (observations - 8))
            significant = bool(
                statistic > 0.0
                and fisher.sf(statistic, 4, observations - 8)
                < DOUBLET_SIGNIFICANCE)
        if significant:
            pair = _components(double[0], double[1], band_centre, band_rate)
            significant = (
                pair[0][2] - pair[1][2] <= DOUBLET_MAX_LEVEL_DIFFERENCE_DB
                and all(abs(row[0] - band_centre)
                        <= DOUBLET_IN_BAND_FRACTION * half_width
                        for row in pair))
        chosen = double if significant else single
        components = _components(chosen[0], chosen[1], band_centre, band_rate)
        result.append({
            "partial": index,
            "centre_hz": centre,
            "doublet": significant,
            "components": components,
        })
    return result


def _midi_hz(midi: int) -> float:
    return 440.0 * 2.0 ** ((midi - 69) / 12.0)


def collect(directory: Path,
            splits: tuple[str, ...] = ("train", "validation"),
            window: int | None = None,
            ) -> dict[tuple[str, str], list[tuple[float, float]]]:
    pooled: dict[tuple[str, str], list[tuple[float, float]]] = {}
    for split in splits:
        manifest_path = directory / f"{split}.json"
        if not manifest_path.is_file():
            continue
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        for example in manifest["examples"]:
            fundamental = _midi_hz(int(example["midi"]))
            for role, side in (("target", "recording"), ("model", "model")):
                entry = example[role]
                signal = _read(directory / entry["path"], int(entry["channels"]))
                pooled.setdefault((example["material"], side), []).extend(
                    partial_rates(signal, int(entry["sample_rate"]), fundamental,
                                  window=window)
                )
    return pooled


def collect_components(directory: Path,
                       splits: tuple[str, ...] = ("train", "validation"),
                       ) -> dict[tuple[str, str], list[dict]]:
    pooled: dict[tuple[str, str], list[dict]] = {}
    for split in splits:
        manifest_path = directory / f"{split}.json"
        if not manifest_path.is_file():
            continue
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        for example in manifest["examples"]:
            fundamental = _midi_hz(int(example["midi"]))
            for role, side in (("target", "recording"), ("model", "model")):
                entry = example[role]
                signal = _read(directory / entry["path"], int(entry["channels"]))
                pooled.setdefault((example["material"], side), []).extend(
                    partial_components(signal, int(entry["sample_rate"]),
                                       fundamental))
    return pooled


def doublet_report(pooled: dict[tuple[str, str], list[dict]]) -> dict:
    """Per band, how many partials decay along two lines and how far apart."""
    materials = sorted({material for material, _ in pooled})
    out: dict = {"bands_hz": BAND_EDGES.tolist(),
                 "artefact_hz": list(ARTEFACT_HZ), "materials": {}}
    for material in materials:
        rows = []
        for low, high in zip(BAND_EDGES[:-1], BAND_EDGES[1:]):
            entry: dict = {"low_hz": low, "high_hz": high}
            for side in ("recording", "model"):
                selected = [row for row in pooled.get((material, side), [])
                            if low <= row["centre_hz"] < high]
                doublets = [row for row in selected if row["doublet"]]
                entry[side] = {
                    "partials": len(selected),
                    "doublets": len(doublets),
                    "dominant_rate": (
                        float(np.median([row["components"][0][1]
                                         for row in selected]))
                        if len(selected) >= MINIMUM_BAND_COUNT else None),
                    "split_cents": (
                        float(np.median([
                            abs(1200.0 * np.log2(row["components"][1][0]
                                                 / row["components"][0][0]))
                            for row in doublets]))
                        if len(doublets) >= MINIMUM_BAND_COUNT else None),
                }
            rows.append(entry)
        out["materials"][material] = rows
    return out


def _print_doublets(result: dict) -> None:
    for material, rows in result["materials"].items():
        print(f"== {material}   doublet-aware partials "
              f"(artefact frequencies {', '.join(f'{x:.0f}' for x in result['artefact_hz'])} Hz masked)")
        print(f"{'band Hz':>14}{'rec n':>7}{'rec 2x':>8}{'rec dB/s':>10}"
              f"{'rec cents':>11}{'mod n':>7}{'mod 2x':>8}{'mod dB/s':>10}"
              f"{'mod cents':>11}")
        for row in rows:
            cells = []
            for side in ("recording", "model"):
                node = row[side]
                cells.append(f'{node["partials"]:7d}{node["doublets"]:8d}')
                cells.append("       n/a" if node["dominant_rate"] is None
                             else f'{node["dominant_rate"]:10.1f}')
                cells.append("        n/a" if node["split_cents"] is None
                             else f'{node["split_cents"]:11.1f}')
            print(f'{row["low_hz"]:6.0f}-{row["high_hz"]:6.0f}' + "".join(cells))


def _band_medians(pooled, material, side, low, high):
    values = [rate for centre, rate in pooled.get((material, side), [])
              if low <= centre < high]
    return (float(np.median(values)) if len(values) >= MINIMUM_BAND_COUNT
            else None, len(values))


def report(pooled: dict[tuple[str, str], list[tuple[float, float]]],
           check: dict | None = None) -> dict:
    materials = sorted({material for material, _ in pooled})
    out: dict = {"bands_hz": BAND_EDGES.tolist(), "materials": {}}
    for material in materials:
        rows = []
        for low, high in zip(BAND_EDGES[:-1], BAND_EDGES[1:]):
            entry: dict = {"low_hz": low, "high_hz": high}
            for side in ("recording", "model"):
                entry[side], entry[side + "_count"] = _band_medians(
                    pooled, material, side, low, high)
            if entry["recording"] is not None and entry["model"] is not None:
                difference = entry["model"] - entry["recording"]
                if check is not None:
                    other = [_band_medians(check, material, side, low, high)[0]
                             for side in ("recording", "model")]
                    if other[0] is None or other[1] is None or abs(
                            (other[1] - other[0]) - difference
                    ) > STABILITY_TOLERANCE_DB_PER_SECOND:
                        # The band's answer depends on the analysis window, so
                        # it is the floor being measured, not the instrument.
                        entry["difference"] = None
                        entry["unstable"] = True
                        rows.append(entry)
                        continue
                entry["difference"] = difference
            rows.append(entry)
        out["materials"][material] = rows
    return out


def _print(result: dict) -> None:
    for material, rows in result["materials"].items():
        print(f"== {material}   median early decay, dB/s")
        print(f"{'band Hz':>14}{'recording':>12}{'model':>10}{'model-rec':>11}"
              f"{'n rec':>8}{'n mod':>7}")
        for row in rows:
            if row.get("unstable"):
                print(f'{row["low_hz"]:6.0f}-{row["high_hz"]:6.0f}'
                      f'{"depends on the analysis window":>44}')
                continue
            if row["recording"] is None or row["model"] is None:
                continue
            print(f'{row["low_hz"]:6.0f}-{row["high_hz"]:6.0f}'
                  f'{row["recording"]:12.1f}{row["model"]:10.1f}'
                  f'{row["difference"]:+11.1f}'
                  f'{row["recording_count"]:8d}{row["model_count"]:7d}')


def self_test() -> int:
    """A synthetic tone with a known per-partial decay must be recovered."""
    rate = 48000
    fundamental = 220.0
    seconds = 3.2
    time = np.arange(int(seconds * rate)) / rate
    signal = np.zeros_like(time)
    expected = {}
    for index in range(1, 13):
        decay_db_s = 6.0 * index
        expected[fundamental * index] = decay_db_s
        amplitude = 10.0 ** (-decay_db_s * time / 20.0) / index
        signal += amplitude * np.sin(2.0 * np.pi * fundamental * index * time)
    signal += 1.0e-6 * np.sin(2.0 * np.pi * 11111.0 * time)
    measured = dict(partial_rates(signal, rate, fundamental))
    if len(measured) < 10:
        print(f"self-test failed: only {len(measured)} partials recovered")
        return 1
    worst = 0.0
    for centre, want in expected.items():
        got = measured.get(centre)
        if got is None:
            continue
        worst = max(worst, abs(got - want))
    if worst > 1.0:
        print(f"self-test failed: worst decay-rate error {worst:.3f} dB/s")
        return 1
    print(f"self-test passed: worst decay-rate error {worst:.4f} dB/s "
          f"over {len(measured)} partials")
    return doublet_self_test()


def doublet_self_test() -> int:
    """One line where there is one, two where there are two, and nothing at all
    on the termination artefact's own frequencies."""
    rate = 48000
    fundamental = 220.0
    time = np.arange(int(3.2 * rate)) / rate
    # Broadband noise at -100 dB: with a perfectly noiseless band any extra
    # parameter reduces a residual that is already numerical, and an F-test on
    # it means nothing. A recording never gives that, and neither does this.
    signal = 1.0e-5 * np.random.default_rng(20260903).standard_normal(time.size)
    single_rates = {}
    for index in range(2, 7):
        decay = 6.0 * index
        frequency = fundamental * index
        single_rates[index] = decay
        signal += (10.0 ** (-decay * time / 20.0) / index) * np.sin(
            2.0 * np.pi * frequency * time)
    # The doublet under test: 1.0 Hz apart, 6 dB apart where the rate window
    # opens, and each member with its own rate, which is what a string coupled
    # through a mobile bridge produces (Weinreich, JASA 62(6), 1977).
    split_hz, level_db = 1.0, 6.0
    loud_rate, quiet_rate = 12.0, 30.0
    start = time - RATE_WINDOW_SECONDS[0]
    doublet = ((10.0 ** (-loud_rate * start / 20.0))
               * np.sin(2.0 * np.pi * fundamental * time)
               + 10.0 ** (-level_db / 20.0)
               * (10.0 ** (-quiet_rate * start / 20.0))
               * np.sin(2.0 * np.pi * (fundamental + split_hz) * time))
    measured = {row["partial"]: row
                for row in partial_components(signal + doublet, rate,
                                              fundamental)}

    if any(masked_by_artefact(row["centre_hz"]) for row in measured.values()):
        print("self-test failed: a masked artefact frequency was reported")
        return 1
    # Only partials 1 to 6 were synthesised, and 3 is masked. Everything above
    # is the -100 dB noise floor, and a fit to noise returns a rate and can
    # call it a doublet, so the analysis floor has to reject it before the fit.
    extra = sorted(index for index in measured if index > 6)
    if extra:
        print(f"self-test failed: partials {extra} are noise and were fitted "
              "anyway")
        return 1
    if 3 in measured:
        print("self-test failed: partial 3 at 660 Hz sits 21 cents from the "
              "668 Hz artefact and should have been masked")
        return 1
    for index, want in single_rates.items():
        row = measured.get(index)
        if row is None:
            continue
        if row["doublet"]:
            print(f"self-test failed: single partial {index} was called a "
                  "doublet")
            return 1
        got = row["components"][0][1]
        if abs(got - want) > 0.1 * want:
            print(f"self-test failed: single partial {index} decayed at "
                  f"{got:.2f} dB/s against {want:.2f}")
            return 1

    first = measured.get(1)
    if first is None or not first["doublet"]:
        print("self-test failed: the two-component partial was not detected")
        return 1
    components = sorted(first["components"], key=lambda row: row[0])
    got_split = components[1][0] - components[0][0]
    got_level = components[0][2] - components[1][2]
    errors = [
        ("split", got_split, split_hz),
        ("level", got_level, level_db),
        ("loud rate", components[0][1], loud_rate),
        ("quiet rate", components[1][1], quiet_rate),
    ]
    for name, got, want in errors:
        if abs(got - want) > 0.1 * abs(want):
            print(f"self-test failed: doublet {name} recovered as {got:.3f} "
                  f"against {want:.3f}")
            return 1
    print("doublet self-test passed: single partials kept their one line to "
          f"within 10%, and a {split_hz:.0f} Hz, {level_db:.0f} dB doublet was "
          f"recovered as {got_split:.3f} Hz and {got_level:.2f} dB at "
          f"{components[0][1]:.2f} and {components[1][1]:.2f} dB/s")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("corpus", type=Path, nargs="?")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--json", action="store_true")
    parser.add_argument(
        "--split", choices=("train", "validation", "both"), default="both",
        help="restrict the audit to one split (default: both)",
    )
    parser.add_argument(
        "--doublets", action="store_true",
        help="report each partial as one damped exponential or two",
    )
    arguments = parser.parse_args()
    if arguments.self_test:
        return self_test()
    if arguments.corpus is None:
        parser.error("a rendered fit-corpus directory is required")
    splits = (("train", "validation") if arguments.split == "both"
              else (arguments.split,))
    corpus = arguments.corpus.resolve()
    if arguments.doublets:
        doublets = doublet_report(collect_components(corpus, splits))
        if arguments.json:
            json.dump(doublets, sys.stdout, indent=2)
            sys.stdout.write("\n")
        else:
            _print_doublets(doublets)
        return 0
    result = report(collect(corpus, splits),
                    collect(corpus, splits, window=CHECK_WINDOW))
    if arguments.json:
        json.dump(result, sys.stdout, indent=2)
        sys.stdout.write("\n")
    else:
        _print(result)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
