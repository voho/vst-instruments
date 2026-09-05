#!/usr/bin/env python3
"""Compare the first five isolators of Lewis Francis's May 7, 2026 hardware take.

Export its frozen MIDI prefix, then render using YouKnowRenderCalibrationEvents:
  python3 Tools/AnalyzeHardwareIsolators.py --midi osc_calibrate_new.mid --events events.txt
  ./build/YouKnowRenderCalibrationEvents events.txt model.wav 0
  python3 Tools/AnalyzeHardwareIsolators.py --reference hardware.wav --model model.wav

Hardware WAV must preserve the first 11.5 seconds at the original 192 kHz.
This serviced Juno-106 has Borish replacement VCF/VCA cards and original DCOs.
Ratios within each file cancel fixed recording gain, not card/trim variation.
"""

import argparse
import hashlib
import json
import platform
from pathlib import Path

import numpy as np
import scipy

from AnalyzeHardwareCalibration import (COMMON_BAND_HZ, common_band_rms, db,
                                        export_events, load_audio, peak_measurement, sha256)


MIDI_SHA256 = "48311413d82d4289b3c961575666739f4581b73baadaa8cbee65156dad29afad"
REFERENCE_PCM_SHA256 = "05d3817d991ba5178a41d9c063b6eefb0bb87cf15aff0a081835aa46a656ccee"
PREFIX_AIFF_SHA256 = "3500df3f0b3c0cd64ab44a3a0eb57635233ab3378e8b94766c5765dabb16bfdf"
SOURCE = "https://github.com/kayrockscreenprinting/ultramaster_kr106/issues/44#issuecomment-4400136262"
AUDIO_SOURCE = "https://lewisfrancis.com/nwio/osc_calibrate_new_bip.aif"
MIDI_SOURCE = "https://kayrock.org/kr106/osc_calibrate_new.mid"
WINDOWS = {"saw": (1.0, 2.3), "pulse50": (3.0, 4.3), "sub": (5.0, 6.3),
           "noise": (7.0, 8.3), "selfosc": (9.0, 10.3)}
FUNDAMENTAL_BANDS_HZ = {"saw": (100, 170), "pulse50": (100, 170),
                        "sub": (50, 85), "selfosc": (200, 300)}


def harmonic_ratios(samples, rate, fundamental):
    # Coherent Hann projections at each file's own estimated pitch: no shared
    # phase, clock, gain, or integer number of periods is required. Nearby
    # projections expose contamination; they are not a calibrated noise floor.
    time = np.arange(len(samples)) / rate
    windowed = (samples - np.mean(samples)) * np.hanning(len(samples))

    def amplitude(frequency):
        return float(abs(np.sum(windowed * np.exp(-2j * np.pi * frequency * time))))

    first = amplitude(fundamental)
    spacing = 5 * rate / len(samples)
    rows = []
    for harmonic in range(2, 9):
        frequency = harmonic * fundamental
        line = amplitude(frequency)
        adjacent = max(amplitude(frequency - spacing), amplitude(frequency + spacing))
        rows.append({"harmonic": harmonic, "relative_fundamental_db": db(line / first),
                     "adjacent_level_relative_fundamental_db": db(adjacent / first),
                     "resolved_above_adjacent_bins": line > 10 ** (10 / 20) * adjacent})
    return {"fundamental_hz": fundamental, "harmonics": rows}


def measure_channels(audio, rate, offset):
    channels = []
    for channel in range(audio.shape[1]):
        sources = {}
        for name, (start, end) in WINDOWS.items():
            first, last = round((start + offset) * rate), round((end + offset) * rate)
            if first < 0 or last > len(audio):
                raise ValueError(f"recording does not cover {name} at {start + offset}s")
            samples = audio[first:last, channel]
            parts = np.array_split(samples, 3)
            values = [db(np.std(part)) for part in parts]
            band_values = [db(common_band_rms(part, rate)) for part in parts]
            measurement = {"rms_dbfs": db(np.std(samples)),
                           "common_band_rms_dbfs": db(common_band_rms(samples, rate)),
                           "window_rms_range_dbfs": [min(values), max(values)],
                           "window_common_band_rms_range_dbfs": [min(band_values), max(band_values)]}
            if name != "noise":
                measurement.update(peak_measurement(samples, rate, low=30, high=1000))
                low, high = FUNDAMENTAL_BANDS_HZ[name]
                frequency = measurement["peak_hz"]
                identified = (low <= frequency <= high
                              and measurement["peak_band_power_fraction"] >= .75)
                measurement["harmonic_analysis"] = (
                    {"fundamental_identified": True, **harmonic_ratios(samples, rate, frequency)}
                    if identified else {"fundamental_identified": False,
                        "reason": "Dominant peak outside the fixed patch's expected band or insufficiently concentrated; harmonic ratios omitted."})
            sources[name] = measurement
        channels.append({"sources": sources, **{
            label: {name: source[field] - sources[anchor][field]
                    for name, source in sources.items()}
            for label, anchor, field in [
                ("source_relative_saw_db", "saw", "rms_dbfs"),
                ("source_relative_selfosc_db", "selfosc", "rms_dbfs"),
                ("source_common_band_relative_saw_db", "saw", "common_band_rms_dbfs"),
                ("source_common_band_relative_selfosc_db", "selfosc", "common_band_rms_dbfs")]}})
    return channels


def analyze(path, offset):
    rate, audio = load_audio(path)
    return {"path": str(path.resolve()), "sha256": sha256(path),
            "pcm_float64le_sha256": hashlib.sha256(
                np.ascontiguousarray(audio, dtype="<f8")).hexdigest(),
            "sample_rate": rate, "channels": audio.shape[1],
            "duration_seconds": len(audio) / rate, "offset_seconds": offset,
            "measurements": measure_channels(audio, rate, offset)}


def self_test():
    rate = 48000
    audio = np.zeros((11 * rate, 1))
    for i, (start, end) in enumerate(WINDOWS.values()):
        first, last = round(start * rate), round(end * rate)
        audio[first:last, 0] = .01 * (i + 1) * np.sin(2 * np.pi * 250 * np.arange(last - first) / rate)
    result = measure_channels(audio, rate, 0)[0]
    json.dumps(result, allow_nan=False)
    assert abs(result["source_relative_saw_db"]["pulse50"] - db(2)) < 1e-9
    assert abs(result["source_common_band_relative_selfosc_db"]["noise"] - db(4 / 5)) < 1e-9
    assert abs(result["sources"]["selfosc"]["peak_hz"] - 250) < .02
    assert not result["sources"]["saw"]["harmonic_analysis"]["fundamental_identified"]
    for sample_rate, frequency, gain, phase in [(48000, 130.123, .1, .4),
                                                (192000, 131.987, .7, 1.8)]:
        time = np.arange(round(1.3 * sample_rate)) / sample_rate
        signal = gain * (np.sin(2 * np.pi * frequency * time + phase)
                         + .3 * np.sin(4 * np.pi * frequency * time - .9)
                         + .07 * np.sin(6 * np.pi * frequency * time + 2.1)) + .2
        estimated = peak_measurement(signal, sample_rate, low=100, high=170)["peak_hz"]
        harmonics = harmonic_ratios(signal, sample_rate, estimated)["harmonics"]
        for row, ratio in zip(harmonics, [.3, .07]):
            assert abs(row["relative_fundamental_db"] - db(ratio)) < .002
            assert row["resolved_above_adjacent_bins"]
    try:
        measure_channels(audio[:10 * rate], rate, 0)
    except ValueError:
        pass
    else:
        raise AssertionError("truncated capture accepted")
    print("hardware isolator analyzer self-check passed")


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
        export_events(args.midi, args.events, MIDI_SHA256, end_seconds=10.5)
    if args.model and not args.reference:
        parser.error("--model requires --reference")
    if not args.reference:
        if not (args.self_test or args.midi):
            parser.error("supply --reference, --midi/--events or --self-test")
        return
    reference = analyze(args.reference, args.reference_offset)
    identified = (reference["pcm_float64le_sha256"] == REFERENCE_PCM_SHA256
                  and reference["sample_rate"] == 192000 and reference["channels"] == 2)
    result = {"expected_hardware_source": SOURCE, "audio_source": AUDIO_SOURCE,
              "midi_source": MIDI_SOURCE, "midi_sha256": MIDI_SHA256,
              "expected_prefix_aiff_sha256": PREFIX_AIFF_SHA256,
              "reference_artifact_scope": "HTTP bytes 0-13999999 only; WAV contains the first 11.5 seconds decoded without resampling. Hashes identify this prefix, not the complete remote AIFF.",
              "reference_identity": (
                  "Canonical prefix PCM verified: Juno-106 #439522, original DCO chips, Borish replacement VCF/VCA cards."
                  if identified else "Recording identity unverified; expected source/unit attribution does not apply."),
              "analyzer_sha256": sha256(__file__),
              "shared_analyzer_sha256": sha256(Path(__file__).with_name("AnalyzeHardwareCalibration.py")),
              "versions": {"python": platform.python_version(), "numpy": np.__version__, "scipy": scipy.__version__},
              "measurement_windows_seconds": WINDOWS, "common_band_hz": COMMON_BAND_HZ,
              "harmonic_method": "Hann-windowed coherent projections H2..H8 at each recording's estimated fundamental. Dominant peak must fall in the fixed patch band and contain >=75% of local power within +/-5%. Adjacent projections +/-5/window-duration Hz mark lines resolved by >10 dB; unresolved ratios include leakage/noise and are not distortion estimates. No waveform-null or missing-fundamental identification is claimed.",
              "common_band_method": "DC-detrended rectangular-window periodogram; sum in-band PSD bins times bin width, without resampling.",
              "conditions": "VCA gate64, HPF1, chorus Off, range16foot, note60; pulse PWM0/manual; selfosc cutoff49/res127/all sound sources off.",
              "limitations": ["One serviced unit; replacement-card gain and noise trim are not original-card calibration evidence.",
                              "Ratios cancel only constant gain within each file; no cross-session absolute gain comparison.",
                              "Three subwindow ranges describe local variation, not confidence intervals or independent hardware units.",
                              "Steady measurement windows do not qualify onset, MIDI serialization or switching timing.",
                              "Peak estimates assume an in-band dominant tone; unknown recordings are unverified."],
              "reference": reference}
    if args.model:
        model = analyze(args.model, args.model_offset)
        if reference["channels"] != model["channels"]:
            raise ValueError("reference and model must have the same channel count")
        result["model"] = model
        result["model_minus_hardware"] = [{
            key.replace("relative", "ratio_error"): {name: m[key][name] - value
                                                     for name, value in r[key].items()}
            for key in r if key != "sources"}
            for r, m in zip(reference["measurements"], model["measurements"])]
    output = json.dumps(result, indent=2, allow_nan=False) + "\n"
    if args.output:
        args.output.write_text(output)
    else:
        print(output, end="")


if __name__ == "__main__":
    main()
