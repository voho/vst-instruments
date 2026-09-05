#!/usr/bin/env python3
"""Measure the April 3, 2026 KR-106 calibration MIDI and matching hardware WAV.

Requires NumPy and SciPy. Convert AIFF without resampling first, for example:
  ffmpeg -i 106_calibration_bip.aif -c:a pcm_s24le hardware.wav
Export the supplied MIDI as seconds/hexbytes for YouKnowRenderCalibrationEvents:
  python3 Tools/AnalyzeHardwareCalibration.py --midi 106_calibration.mid --events events.txt
Compare recordings with the same MIDI timeline (offsets are audio minus MIDI):
  python3 Tools/AnalyzeHardwareCalibration.py --reference hardware.wav --model model.wav

This is a descriptive comparison of one serviced Juno-106 with replacement
Borish voice cards. It neither fits DSP coefficients nor establishes fidelity
to original 80017A cards. The WAV files and downloaded MIDI stay outside git.
"""

import argparse
import hashlib
import json
import math
import platform
import struct
from pathlib import Path

import numpy as np
import scipy
from scipy.io import wavfile
from scipy.signal import periodogram, welch


MIDI_SHA256 = "c9727669f08ff27b3c1ccdd4faf4cf1d586537c156483d55d735d0b75dc855e3"
SOURCE = "https://github.com/kayrockscreenprinting/ultramaster_kr106/issues/16#issuecomment-4184997000"
MIDI_SOURCE = "https://kayrock.org/kr106/106_calibration.zip"
REFERENCE_PCM_SHA256 = "3575d2e9dc6e42c6ca7cef0c1b017ed9e6b85a0f0db227ee646ad944146f4bb0"
REFERENCE_AIFF_SHA256 = "a9282c4a287e7adf1a8cf46879d037633225db6f0e56f16014c08373ffb683aa"
COMMON_BAND_HZ = (20.0, 20000.0)


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def export_events(midi, output, expected_sha256=MIDI_SHA256, end_seconds=math.inf):
    # Only frozen public sequences use this parser. Their hashes fix the
    # supported container, tempo, bytes and event ordering together.
    data = Path(midi).read_bytes()
    if hashlib.sha256(data).hexdigest() != expected_sha256:
        raise ValueError("MIDI differs from the expected calibration sequence")
    ppq = struct.unpack_from(">H", data, 12)[0]
    pos, tempo, seconds = 22, 500000, 0.0
    rows = []

    def vlq():
        nonlocal pos
        value = 0
        while True:
            byte = data[pos]
            pos += 1
            value = (value << 7) | (byte & 127)
            if byte < 128:
                return value

    while pos < len(data):
        delta = vlq()
        seconds += delta * tempo / 1e6 / ppq
        status = data[pos]
        pos += 1
        if status == 0xff:
            kind = data[pos]
            pos += 1
            length = vlq()
            payload = data[pos:pos + length]
            pos += length
            if kind == 0x51:
                tempo = int.from_bytes(payload, "big")
        else:
            length = vlq() if status == 0xf0 else 2
            payload = bytes([status]) + data[pos:pos + length]
            pos += length
            if seconds <= end_seconds:
                rows.append(f"{seconds:.9f}\t{payload.hex()}\n")
    Path(output).write_text("".join(rows))


def load_audio(path):
    rate, samples = wavfile.read(path)
    if samples.dtype.kind == "u":
        samples = (samples.astype(float) - 128) / 128
    elif samples.dtype.kind == "i":
        samples = samples.astype(float) / (1 << (8 * samples.dtype.itemsize - 1))
    else:
        samples = samples.astype(float)
    if samples.ndim == 1:
        samples = samples[:, None]
    if samples.ndim != 2 or not np.all(np.isfinite(samples)):
        raise ValueError("audio must contain finite mono or multichannel samples")
    if rate < 44100:
        raise ValueError("use the original recording sample rate, at least 44.1 kHz")
    return rate, samples


def db(value):
    return 20 * math.log10(max(float(value), 1e-15))


def common_band_rms(samples, rate):
    # Integrate a rectangular-window periodogram: Parseval preserves the same
    # time weighting as std(), while excluding DC, subsonics and ultrasound.
    # No resampling or fitted recording gain enters this measurement.
    frequencies, power = periodogram(samples, rate, window="boxcar")
    band = (frequencies >= COMMON_BAND_HZ[0]) & (frequencies <= COMMON_BAND_HZ[1])
    return math.sqrt(float(np.sum(power[band]) * (frequencies[1] - frequencies[0])))


def peak_measurement(samples, rate, low=2.0, high=None):
    # A Hann window and log-parabolic interpolation permit sub-bin estimates.
    nfft = 1 << (4 * len(samples) - 1).bit_length()
    frequencies, power = periodogram(samples, rate, window="hann", nfft=nfft)
    upper = rate * .49 if high is None else min(high, rate * .49)
    choices = np.flatnonzero((frequencies >= low) & (frequencies <= upper))
    index = choices[np.argmax(power[choices])]
    a, b, c = np.log(np.maximum(power[index - 1:index + 2], 1e-30))
    fraction = .5 * (a - c) / (a - 2 * b + c) if a - 2 * b + c else 0
    peak = float((index + np.clip(fraction, -.5, .5)) * rate / nfft)
    narrow = (frequencies > .95 * peak) & (frequencies < 1.05 * peak)
    local = (frequencies > .5 * peak) & (frequencies < 2 * peak)
    concentration = float(np.sum(power[narrow]) / max(np.sum(power[local]), 1e-30))
    return {"peak_hz": peak, "peak_band_power_fraction": concentration}


def analyze(path, offset):
    rate, audio = load_audio(path)

    def segment(start):
        first, end = round((start + offset) * rate), round((start + offset + 1.3) * rate)
        if first < 0 or end > len(audio):
            raise ValueError(f"recording does not cover the measurement at {start}s")
        return audio[first:end]

    result = {"path": str(Path(path).resolve()), "sha256": sha256(path),
              "sample_rate": rate, "channels": audio.shape[1],
              "pcm_float64le_sha256": hashlib.sha256(
                  np.ascontiguousarray(audio, dtype="<f8")).hexdigest(),
              "duration_seconds": len(audio) / rate, "offset_seconds": offset}
    channels = []
    for channel in range(audio.shape[1]):
        # The first noise note is absent in this hardware capture. The identical
        # open-filter/flat-HPF noise setup at 10s supplies the noise measurement.
        starts = {"noise": 10.5, "saw": 2.5, "pulse_pwm64": 4.5, "sub": 6.5}
        levels = {name: db(np.std(segment(start)[:, channel]))
                  for name, start in starts.items()}
        band_levels = {name: db(common_band_rms(segment(start)[:, channel], rate))
                       for name, start in starts.items()}
        level_ranges = {}
        for name, start in starts.items():
            parts = np.array_split(segment(start)[:, channel], 3)
            values = [db(np.std(part)) for part in parts]
            level_ranges[name] = [min(values), max(values)]
        spectra = []
        for start in [8.5, 10.5, 12.5, 14.5]:
            frequencies, power = welch(segment(start)[:, channel], rate,
                                        nperseg=round(rate * (32768 / 96000)))
            spectra.append(power)
        bands = [(31.25, 62.5), (62.5, 125), (125, 250), (250, 500),
                 (500, 1000), (1000, 2000), (2000, 4000), (4000, 8000)]
        hpf = []
        for low, high in bands:
            mask = (frequencies >= low) & (frequencies < high)
            reference = np.sum(spectra[1][mask])
            hpf.append({"band_hz": [low, high], "positions_relative_flat_db": [
                db(math.sqrt(np.sum(power[mask]) / reference)) for power in spectra]})
        # These are NOISE-DRIVEN resonant peaks, not isolated self-oscillation.
        # Report independent-window variation instead of false cents precision.
        # Codes112/120 are ultrasonic on hardware and alias in a 48k render.
        peaks = []
        for code in range(32, 105, 8):
            samples = segment(80.5 + code / 4)[:, channel]
            measurement = peak_measurement(samples, rate)
            values = [peak_measurement(part, rate)["peak_hz"]
                      for part in np.array_split(samples, 3)]
            measurement.update({"cutoff_byte": code,
                "rms_dbfs": db(np.std(samples)),
                "noise_relative_resonant_output_db": levels["noise"] - db(np.std(samples)),
                "window_peak_range_hz": [min(values), max(values)],
                "window_span_cents": 1200 * math.log2(max(values) / min(values)),
                "near_nyquist": measurement["peak_hz"] >= .4 * rate,
                "broad_or_unstable": measurement["peak_band_power_fraction"] < .75
                                     or max(values) / min(values) > 2 ** (100 / 1200)})
            peaks.append(measurement)
        channels.append({"source_rms_dbfs": levels,
                         "source_common_band_rms_dbfs": band_levels,
                         "source_rms_window_range_dbfs": level_ranges,
                         "source_relative_saw_db": {key: value - levels["saw"]
                                                    for key, value in levels.items()},
                         "source_common_band_relative_saw_db": {
                             key: value - band_levels["saw"] for key, value in band_levels.items()},
                         "hpf": hpf, "resonance127_peaks": peaks})
    result["measurements"] = channels
    return result


def self_test():
    rate = 48000
    tone = .25 * np.sin(2 * np.pi * 271.234 * np.arange(62400) / rate)
    measurement = peak_measurement(tone, rate)
    assert abs(measurement["peak_hz"] - 271.234) < .02
    assert measurement["peak_band_power_fraction"] > .999
    assert abs(db(np.std(tone)) - db(.25 / math.sqrt(2))) < .01
    assert abs(db(2) - 6.020599913279624) < 1e-10
    # Same audible signal at two recording rates; hardware additionally carries
    # ultrasound and DC. Full-band RMS differs, common-band RMS must agree.
    values = []
    for sample_rate in [48000, 96000]:
        time = np.arange(sample_rate) / sample_rate
        samples = .25 * np.sin(2 * np.pi * 1000 * time) + .1
        if sample_rate == 96000:
            samples += .5 * np.sin(2 * np.pi * 28000 * time)
        values.append(common_band_rms(samples, sample_rate))
    assert abs(db(values[0] / values[1])) < 1e-10
    assert abs(values[0] - .25 / math.sqrt(2)) < 1e-12
    print("hardware calibration analyzer self-check passed")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--midi", type=Path)
    parser.add_argument("--events", type=Path)
    parser.add_argument("--reference", type=Path)
    parser.add_argument("--model", type=Path)
    parser.add_argument("--reference-offset", type=float, default=0)
    parser.add_argument("--model-offset", type=float, default=0)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
    if bool(args.midi) != bool(args.events):
        parser.error("--midi and --events must be supplied together")
    if args.midi:
        export_events(args.midi, args.events)
    if args.model and not args.reference:
        parser.error("--model requires --reference")
    if not args.reference:
        if not (args.self_test or args.midi):
            parser.error("supply --reference, --midi/--events or --self-test")
        return
    result = {"expected_hardware_source": SOURCE, "midi_source": MIDI_SOURCE,
              "midi_sha256": MIDI_SHA256, "analyzer_sha256": sha256(__file__),
              "expected_original_aiff_sha256": REFERENCE_AIFF_SHA256,
              "common_band_hz": COMMON_BAND_HZ,
              "common_band_method": "DC-detrended rectangular-window periodogram; sum PSD bins within band times bin width, without resampling. Band-edge resolution is 1 / window duration.",
              "versions": {"python": platform.python_version(), "numpy": np.__version__,
                           "scipy": scipy.__version__},
              "scope": "Descriptive comparison against the expected April 3 calibration recording; identity is checked below.",
              "notes": ["MIDI uses 16-foot range, note60, VCA gate/level64, chorus off.",
                        "Pulse PWM byte64 is manual; the website's 50% prose does not match these bytes.",
                        "Noise uses the repeated flat-HPF segment; initial hardware noise note is absent.",
                        "Sub sounds one octave below saw; its level ratio includes the frequency response.",
                        "Independent noise windows and different voice allocations add sampling/unit variation.",
                        "Full-band RMS includes each file's available bandwidth; use common-band ratios for sample-rate-independent source comparison.",
                        "Resonance127 measures noise-driven spectral peaks, not an isolated self-oscillation fundamental.",
                        "Broad/unstable flags mean <75% local power within +/-5% of peak or >100c window variation; descriptive, not a fidelity pass criterion.",
                        "No recording-chain calibration or original voice-card equivalence is established."],
              "reference": analyze(args.reference, args.reference_offset)}
    reference = result["reference"]
    identified = (reference["pcm_float64le_sha256"] == REFERENCE_PCM_SHA256
                  and reference["sample_rate"] == 96000 and reference["channels"] == 2)
    result["reference_identity"] = (
        "Canonical PCM verified: Juno-106 #439522, Borish replacement voice cards, serviced 2022."
        if identified else "Recording identity unverified; the expected source/unit attribution does not apply.")
    if args.model:
        result["model"] = analyze(args.model, args.model_offset)
        if result["reference"]["channels"] != result["model"]["channels"]:
            raise ValueError("reference and model must have the same channel count")
        comparison_high_hz = .4 * min(result["reference"]["sample_rate"],
                                      result["model"]["sample_rate"])
        differences = []
        for reference, model in zip(result["reference"]["measurements"], result["model"]["measurements"]):
            differences.append({"source_ratio_error_db": {
                name: model["source_relative_saw_db"][name] - value
                for name, value in reference["source_relative_saw_db"].items()},
                "source_ratio_common_band_error_db": {
                    name: model["source_common_band_relative_saw_db"][name] - value
                    for name, value in reference["source_common_band_relative_saw_db"].items()},
                "resonance127_peak_error_cents": [{"cutoff_byte": r["cutoff_byte"],
                    "cents": (None if max(r["peak_hz"], m["peak_hz"]) >= comparison_high_hz else
                              1200 * math.log2(m["peak_hz"] / r["peak_hz"])),
                    "broad_or_unstable": r["broad_or_unstable"] or m["broad_or_unstable"]}
                    for r, m in zip(reference["resonance127_peaks"], model["resonance127_peaks"])]})
        result["model_minus_hardware"] = differences
    text = json.dumps(result, indent=2, allow_nan=False) + "\n"
    if args.output:
        args.output.write_text(text)
    else:
        print(text, end="")


if __name__ == "__main__":
    main()
