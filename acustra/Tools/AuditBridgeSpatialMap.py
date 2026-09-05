#!/usr/bin/env python3
"""Test Mores' held-out center impact against the mean of bass/treble impacts.

This is a spatial interpolation diagnostic, not a horizontal-admittance
measurement. A rigid heave/rock reduction predicts Hcenter=(Hbass+Htreble)/2.
No gain, delay, sign or minimum-phase fit is allowed to improve this prediction.
Both accelerometers and all three microphones are checked for g21 and g34.

Full-record complex FRFs are primary. Sixth-octave errors are essential:
broadband energy weighting can conceal individual modal/antiresonance failures.
Common 62.5 ms and 16 ms causal tapers expose window sensitivity without destroying
relative phase; 16 ms cannot resolve narrow low-frequency modes. The selected
archive has one impact per position, so results are deterministic discrepancies,
not confidence intervals or statistical coherence. Passing normal-force spatial
interpolation does not identify lateral translation or a saddle rotation axis.

Uses the existing generators' verified source, SI calibration and H1 estimator.
NumPy/SciPy only; no rendering, fitting, plotting or repository mutation.

    python3 Tools/AuditBridgeSpatialMap.py --self-test
    python3 Tools/AuditBridgeSpatialMap.py --raw-mat /path/qualified_selected_impulses.mat --output /new/audit-directory
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
from pathlib import Path
import sys

import numpy as np
import scipy

import GenerateMeasuredBridge as bridge
import GenerateMeasuredBody as body

BANDS = ((60, 120), (120, 250), (250, 500), (500, 1000), (1000, 2000),
         (2000, 4000), (4000, 6000), (6000, 10000))
CHANNELS = {1: "treble_accel", 2: "bass_accel", 3: "upper_mic",
            4: "treble_mic", 5: "bass_mic"}
WINDOWS = (("full_record", None), ("common_62.5ms", 3000), ("common_16ms", 768))


def sha256(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def windowed(response: np.ndarray, keep: int | None) -> np.ndarray:
    if keep is None:
        return response
    window = np.ones(keep)
    fade = max(1, keep // 10)
    window[-fade:] = 0.5 + 0.5 * np.cos(np.linspace(0, np.pi, fade))
    impulse = np.fft.irfft(response, bridge.FFT_SIZE)
    result = np.zeros(bridge.FFT_SIZE)
    result[:keep] = impulse[:keep] * window
    return np.fft.rfft(result)


def metrics(center: np.ndarray, prediction: np.ndarray) -> dict:
    reference = np.linalg.norm(center)
    if not np.isfinite(center).all() or not np.isfinite(prediction).all() or reference <= 0:
        raise ValueError("nonfinite or zero-energy reference band")
    # Only secondary magnitude/phase summaries mask deep notches. The primary
    # complex error includes every bin, with no fitted gain or phase alignment.
    floor = max(abs(center).max(), abs(prediction).max()) * 0.01
    active = (abs(center) > floor) & (abs(prediction) > floor)
    db = 20 * np.log10(abs(prediction[active]) / abs(center[active]))
    phase = np.abs(np.angle(prediction[active] * np.conj(center[active]), deg=True))
    return {
        "complex_relative_l2": float(np.linalg.norm(prediction - center) / reference),
        "magnitude_bias_median_db": float(np.median(db)) if len(db) else None,
        "magnitude_abs_median_db": float(np.median(abs(db))) if len(db) else None,
        "magnitude_abs_p90_db": float(np.percentile(abs(db), 90)) if len(db) else None,
        "phase_abs_median_deg": float(np.median(phase)) if len(phase) else None,
        "phase_abs_p90_deg": float(np.percentile(phase, 90)) if len(phase) else None,
        "active_fraction": float(active.mean()),
        "bins": len(center),
    }


def extract(values: np.ndarray) -> tuple[dict, dict]:
    frequency = np.fft.rfftfreq(bridge.FFT_SIZE, 1 / bridge.SAMPLE_RATE)
    taper = 0.5 + 0.5 * np.cos(np.arange(1, 48001) * np.pi / 48000)
    responses, force_quality = {}, {}
    for guitar in (21, 34):
        for impact in range(3):
            record = values[guitar - 1, impact * 48000:(impact + 1) * 48000]
            force = record[:, 0] * taper * bridge.HAMMER_NEWTONS_PER_FULL_SCALE
            spectrum = np.fft.rfft(force, bridge.FFT_SIZE)
            denominator = abs(spectrum) ** 2
            denominator += denominator.max() * 1e-12
            band_force = abs(spectrum)[(frequency >= 60) & (frequency < 10000)]
            force_quality[f"g{guitar}_impact{impact}"] = {
                "peak_force_N": float(abs(force).max()),
                "force_peak_sample": int(abs(force).argmax()),
                "band_force_spectrum_min_over_max": float(band_force.min() / band_force.max()),
            }
            for channel in CHANNELS:
                if channel <= 2:
                    response = bridge.transfer(values, guitar, impact, channel)
                else:
                    pressure = record[:, channel] * taper * body.MIC_PASCALS_PER_FULL_SCALE
                    response = (np.fft.rfft(pressure, bridge.FFT_SIZE)
                                * np.conj(spectrum) / denominator)
                responses[guitar, impact, channel] = response
    return responses, force_quality


def audit(frequency: np.ndarray, responses: dict) -> tuple[list, list]:
    broad, narrow = [], []
    for guitar in (21, 34):
        for channel, name in CHANNELS.items():
            for window, keep in WINDOWS:
                bass, center, treble = [windowed(responses[guitar, i, channel], keep)
                                        for i in range(3)]
                prediction = 0.5 * (bass + treble)
                for low, high in BANDS:
                    band = (frequency >= low) & (frequency < high)
                    broad.append(dict(guitar=guitar, channel=name, window=window,
                                      low_hz=low, high_hz=high,
                                      **metrics(center[band], prediction[band])))
            center = responses[guitar, 1, channel]
            prediction = 0.5 * (responses[guitar, 0, channel] + responses[guitar, 2, channel])
            for exponent in range(-12, 31):
                hz = 250 * 2 ** (exponent / 6)
                edge = 2 ** (1 / 12)
                band = (frequency >= hz / edge) & (frequency < hz * edge)
                if hz < 60 or hz > 10000 or band.sum() < 3:
                    continue
                narrow.append(dict(guitar=guitar, channel=name, center_hz=float(hz),
                                   complex_relative_l2=float(np.linalg.norm((prediction - center)[band])
                                                             / np.linalg.norm(center[band]))))
    return broad, narrow


def self_test() -> None:
    frequency = np.fft.rfftfreq(bridge.FFT_SIZE, 1 / bridge.SAMPLE_RATE)
    s = 2j * np.pi * frequency
    # Two modes with distinct linear spatial shapes. The center response is
    # independently evaluated at position zero, never constructed by averaging
    # the endpoint records that the audit is asked to predict from.
    responses = {}
    for guitar in (21, 34):
        for channel in CHANNELS:
            for impact, position in enumerate((-1, 0, 1)):
                responses[guitar, impact, channel] = (
                    (1 + 0.35 * position) / (s * s + 30 * s + (2 * np.pi * 180) ** 2)
                    + (0.3 - 0.6 * position) * channel
                    / (s * s + 60 * s + (2 * np.pi * 620) ** 2))
    broad, narrow = audit(frequency, responses)
    if max(row["complex_relative_l2"] for row in broad + narrow) > 1e-10:
        raise AssertionError("linear spatial map failed its held-out center check")
    band = (frequency >= 120) & (frequency < 1000)
    center = responses[21, 1, 4][band]
    prediction = 0.5 * (responses[21, 0, 4][band] + responses[21, 2, 4][band])
    gain_error = metrics(1.25 * center, prediction)
    phase_error = metrics(center * np.exp(1j * np.pi / 6), prediction)
    if (gain_error["complex_relative_l2"] < 0.19
        or gain_error["magnitude_abs_median_db"] < 1.9
        or phase_error["complex_relative_l2"] < 0.5
        or phase_error["phase_abs_median_deg"] < 29.9):
        raise AssertionError("audit concealed a held-out gain or phase error")
    print("Bridge spatial-map self-test passed")


def run(raw: Path, output: Path) -> None:
    if output.exists() or not output.parent.is_dir():
        raise ValueError("output must be a new directory inside an existing parent")
    values = bridge.load_matrix(raw)
    responses, force_quality = extract(values)
    frequency = np.fft.rfftfreq(bridge.FFT_SIZE, 1 / bridge.SAMPLE_RATE)
    broad, narrow = audit(frequency, responses)
    report = {
        "source": str(raw.resolve()), "raw_md5": bridge.digest(raw), "raw_sha256": sha256(raw),
        "source_url": "https://zenodo.org/records/4604577",
        "tool_sha256": sha256(Path(__file__)),
        "helper_sha256": {name: sha256(Path(__file__).with_name(name)) for name in
                          ("GenerateMeasuredBridge.py", "GenerateMeasuredBody.py")},
        "versions": {"python": sys.version.split()[0], "numpy": np.__version__, "scipy": scipy.__version__},
        "sample_rate": bridge.SAMPLE_RATE, "fft_size": bridge.FFT_SIZE,
        "units": {"accelerometer_FRF": "m/s/N (acceleration divided by differentiated force)",
                  "microphone_FRF": "Pa/N", "complex_relative_l2": "dimensionless",
                  "magnitude_errors": "dB", "phase_errors": "degrees", "frequency": "Hz"},
        "method": [
            "Held-out center prediction is strictly 0.5*(bass+treble), with no gain, sign, delay or minimum-phase adjustment.",
            "Accelerometer mobility uses GenerateMeasuredBridge.transfer unchanged, including SI calibration, differentiated hammer and one-second half-cosine taper.",
            "Microphones use GenerateMeasuredBody's calibrated H1 pressure/force estimator before minimum-phase conversion.",
            "A common 2-sample instrumentation phase advance cancels from every full-record error and is not applied. Each H1 references its own measured hammer; different impact sample indices are not manually shifted.",
            "Full one-second selected records are primary. Common 62.5 ms and 16 ms causal tapers are sensitivity checks;16 ms cannot resolve narrow low-frequency modes.",
            "Sixth-octave results must accompany broad bands: energy weighting can hide local modal or antiresonance errors.",
            "Secondary magnitude/phase statistics mask bins below −40 dB of the strongest response in the band. Primary complex L2 is unmasked; active fractions are reported.",
            "Only one selected impact per position is available. No confidence interval, repeatability estimate or statistical coherence is inferred.",
            "Disagreement can reflect spatial flexibility, noncollocated sensor geometry, drift or noise. Passing this normal-force interpolation does not identify lateral translation, saddle rotation-axis height, or horizontal-force radiation.",
        ],
        "force_quality": force_quality, "bands": broad, "sixth_octaves": narrow,
    }
    # Validate and calculate before creating anything; never reuse an output
    # directory. Write the complete report last so partial failures are visible.
    output.mkdir()
    for name, rows in (("band-errors.csv", broad), ("sixth-octave-errors.csv", narrow)):
        with (output / name).open("x", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
            writer.writeheader()
            writer.writerows(rows)
    with (output / "report.json").open("x") as stream:
        json.dump(report, stream, indent=2, allow_nan=False)
        stream.write("\n")
    print(f"Wrote {len(broad)} broad-band and {len(narrow)} sixth-octave rows to {output}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--raw-mat", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    try:
        if args.self_test:
            if args.raw_mat or args.output:
                parser.error("--self-test does not take input/output paths")
            self_test()
        else:
            if args.raw_mat is None or args.output is None:
                parser.error("--raw-mat and --output are required")
            run(args.raw_mat, args.output)
        return 0
    except (OSError, ValueError, AssertionError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
