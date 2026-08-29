#!/usr/bin/env python3
"""Compare one frozen exact-eight reference fixture with Electry v3 probes.

The result is a successor ``electry-evaluation/v4`` descriptive record.  A
separate frozen plan supplies every semantic mapping that the rights/provenance
receipt deliberately does not: asset, channel, crop, nominal frequency and
an onset-search start after at least 50 ms of pre-roll, plus a model probe.  The
plan also binds this analyzer, the fixture validator, the
receipt, the model manifest and every consumed model WAV by SHA-256.

Analysis stays at each file's native sample rate.  It reports onset-aligned,
within-file-normalized envelope and first-cycle descriptors.  It never
resamples, fits a coefficient, aggregates pairs into a score, or decides that a
model/reference distance passes.  Invalid provenance, hashes, audio, mappings,
onsets and signal-to-noise fail closed.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import platform
import struct
import sys
import tempfile
from pathlib import Path


sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
import ValidateReferenceFixture as reference  # noqa: E402


PLAN_SCHEMA = "electry-reference-comparison-plan/v1"
RESULT_SCHEMA = "electry-evaluation/v4"
MODEL_SCHEMA = "electry-evaluation/v3"
STATUS = "frozen"
PROFILE = "native-rate-isolated-envelope/v1"
SCOPE = reference.SCIENTIFIC_SCOPE
SHA256 = reference.SHA256
ZERO_SHA256 = reference.ZERO_SHA256

ENVELOPE_MS = 2
NOISE_MS = 50
ONSET_PEAK_SEARCH_MS = 1000
POST_ONSET_MS = 2000
MINIMUM_SNR_DB = 20.0
ONSET_FRACTION = 0.25
PEAK_TIME_TOLERANCE_DB = 0.05
RMS_WINDOWS_MS = (
    (0, 50),
    (50, 150),
    (150, 500),
    (500, 1000),
    (1000, 2000),
)
MODEL_SAMPLE_RATE = 44100
MODEL_LEAD_IN_FRAMES = 11025
MODEL_HELD_FRAMES = 88200
MODEL_RELEASE_FRAMES = 44100
MODEL_TOTAL_FRAMES = 143325
MODEL_PROBES = (
    ("e1-open", "sustain", 8, 28, 41.20344461, 0.55, 15),
    ("e1-palm-mute-light", "palm_mute", 8, 28, 41.20344461, 0.0, 16),
    ("e1-palm-mute-medium", "palm_mute", 8, 28, 41.20344461, 0.55, 16),
    ("e1-palm-mute-hard", "palm_mute", 8, 28, 41.20344461, 1.0, 16),
    ("e1-dead", "dead", 8, 28, 41.20344461, 0.55, 21),
    ("e2-open", "sustain", 6, 40, 82.40688923, 0.55, 15),
    ("e2-palm-mute-light", "palm_mute", 6, 40, 82.40688923, 0.0, 16),
    ("e2-palm-mute-medium", "palm_mute", 6, 40, 82.40688923, 0.55, 16),
    ("e2-palm-mute-hard", "palm_mute", 6, 40, 82.40688923, 1.0, 16),
    ("e2-dead", "dead", 6, 40, 82.40688923, 0.55, 21),
)

PLAN_TEMPLATE = {
    "schema": PLAN_SCHEMA,
    "status": STATUS,
    "target_protocol": RESULT_SCHEMA,
    "scientific_scope": SCOPE,
    "analysis_profile": PROFILE,
    "analyzer_sha256": ZERO_SHA256,
    "fixture_validator_sha256": ZERO_SHA256,
    "reference_receipt_sha256": ZERO_SHA256,
    "model_manifest_sha256": ZERO_SHA256,
    "pairs": [{
        "id": "REPLACE",
        "reference_asset_id": "REPLACE",
        "reference_channel_index": 0,
        "reference_crop_start_frame": 0,
        "reference_crop_frames": 0,
        "reference_onset_search_start_frame": 0,
        "reference_nominal_frequency_hz": 0.0,
        "model_probe_id": "REPLACE",
        "model_audio_sha256": ZERO_SHA256,
    }],
}

Invalid = reference.Invalid


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise Invalid(message)


def _digest(path: Path, label: str) -> str:
    _, digest = reference._read_snapshot(path, label)
    return digest


def _checked_hash(value, label: str) -> str:
    _require(isinstance(value, str) and SHA256.fullmatch(value) is not None,
             f"{label} must be a lowercase SHA-256")
    _require(value != ZERO_SHA256, f"{label} is still a zero placeholder")
    return value


def _integer(value, minimum: int, label: str) -> int:
    _require(type(value) is int and value >= minimum,
             f"{label} must be an integer >= {minimum}")
    return value


def _positive_float(value, label: str) -> float:
    _require(type(value) in (int, float) and math.isfinite(value)
             and float(value) > 0.0,
             f"{label} must be a finite positive number")
    return float(value)


def _frames(milliseconds: int, sample_rate: int) -> int:
    # All profile boundaries are integer milliseconds.  Integer half-up
    # rounding keeps the contract independent of Python's tie-to-even round().
    return max(1, (milliseconds * sample_rate + 500) // 1000)


def _db_ratio(numerator: float, denominator: float, label: str) -> float:
    _require(math.isfinite(numerator) and math.isfinite(denominator)
             and numerator > 0.0 and denominator > 0.0,
             f"{label} needs two positive finite values")
    return 20.0 * math.log10(numerator / denominator)


def _load_plan(path: Path) -> tuple[dict, str]:
    plan, digest = reference._load_json_snapshot(path.resolve(), "comparison plan")
    plan = reference._exact_keys(
        plan,
        {"schema", "status", "target_protocol", "scientific_scope",
         "analysis_profile", "analyzer_sha256", "fixture_validator_sha256",
         "reference_receipt_sha256", "model_manifest_sha256", "pairs"},
        "comparison plan",
    )
    reference._scan_filled(plan, "comparison plan")
    _require(plan["schema"] == PLAN_SCHEMA,
             "comparison plan schema is not reference-comparison-plan v1")
    _require(plan["status"] == STATUS, "comparison plan is not frozen")
    _require(plan["target_protocol"] == RESULT_SCHEMA,
             "comparison plan target must be electry-evaluation/v4")
    _require(plan["scientific_scope"] == SCOPE,
             "comparison plan scope must forbid fit or promotion")
    _require(plan["analysis_profile"] == PROFILE,
             "comparison plan analysis profile is unsupported")
    for key in ("analyzer_sha256", "fixture_validator_sha256",
                "reference_receipt_sha256", "model_manifest_sha256"):
        _checked_hash(plan[key], f"comparison plan {key}")

    pairs = plan["pairs"]
    _require(isinstance(pairs, list) and 1 <= len(pairs) <= 64,
             "comparison plan pairs must contain 1..64 rows")
    pair_ids = set()
    for index, raw_pair in enumerate(pairs):
        label = f"comparison plan pairs[{index}]"
        pair = reference._exact_keys(
            raw_pair,
            {"id", "reference_asset_id", "reference_channel_index",
             "reference_crop_start_frame", "reference_crop_frames",
             "reference_onset_search_start_frame",
             "reference_nominal_frequency_hz", "model_probe_id",
             "model_audio_sha256"},
            label,
        )
        pair_id = reference._identifier(pair["id"], f"{label}.id")
        _require(pair_id not in pair_ids, f"{label}.id is duplicated")
        pair_ids.add(pair_id)
        reference._identifier(pair["reference_asset_id"],
                              f"{label}.reference_asset_id")
        _integer(pair["reference_channel_index"], 0,
                 f"{label}.reference_channel_index")
        _integer(pair["reference_crop_start_frame"], 0,
                 f"{label}.reference_crop_start_frame")
        _integer(pair["reference_crop_frames"], 1,
                 f"{label}.reference_crop_frames")
        _integer(pair["reference_onset_search_start_frame"], 1,
                 f"{label}.reference_onset_search_start_frame")
        frequency = _positive_float(pair["reference_nominal_frequency_hz"],
                                    f"{label}.reference_nominal_frequency_hz")
        _require(15.0 <= frequency <= 5000.0,
                 f"{label}.reference_nominal_frequency_hz is outside the profile")
        reference._identifier(pair["model_probe_id"],
                              f"{label}.model_probe_id")
        _checked_hash(pair["model_audio_sha256"],
                      f"{label}.model_audio_sha256")
    return plan, digest


def _decode_channel(data: bytes, info: dict, channel_index: int,
                    start_frame: int, frame_count: int, label: str) -> list[float]:
    channels = info["channels"]
    _require(channel_index < channels,
             f"{label} channel index {channel_index} is outside {channels} channels")
    _require(start_frame <= info["frames"]
             and frame_count <= info["frames"] - start_frame,
             f"{label} crop is outside the audio payload")

    bits = info["bits_per_sample"]
    sample_bytes = bits // 8
    encoding = info["encoding"]
    data_offset = info["data_offset"]
    block_align = info["block_align"]
    samples = []
    samples_append = samples.append
    for frame in range(start_frame, start_frame + frame_count):
        offset = data_offset + frame * block_align + channel_index * sample_bytes
        if encoding == "IEEE_FLOAT":
            sample = struct.unpack_from("<f", data, offset)[0]
        else:
            sample = int.from_bytes(
                data[offset:offset + sample_bytes], "little", signed=True)
            sample /= float(1 << (bits - 1))
        _require(math.isfinite(sample), f"{label} contains non-finite audio")
        samples_append(sample)
    return samples


def read_wave_channel(path: Path, channel_index: int = 0,
                      start_frame: int = 0, frame_count: int | None = None,
                      label: str = "audio") -> tuple[dict, str, list[float]]:
    """Read one declared channel without downmixing or resampling."""
    data, digest = reference._read_snapshot(path, label)
    info = reference._wave_info(data, label)
    if frame_count is None:
        frame_count = info["frames"] - start_frame
    _integer(channel_index, 0, f"{label} channel index")
    _integer(start_frame, 0, f"{label} crop start")
    _integer(frame_count, 1, f"{label} crop frames")
    samples = _decode_channel(
        data, info, channel_index, start_frame, frame_count, label)
    return info, digest, samples


def _energy_prefix(samples: list[float]) -> list[float]:
    prefix = [0.0]
    total = 0.0
    for sample in samples:
        total += sample * sample
        _require(math.isfinite(total), "audio energy overflowed")
        prefix.append(total)
    return prefix


def _rms(prefix: list[float], start: int, end: int, label: str) -> float:
    _require(0 <= start < end < len(prefix), f"{label} window is outside audio")
    energy = max(0.0, prefix[end] - prefix[start])
    value = math.sqrt(energy / float(end - start))
    _require(math.isfinite(value), f"{label} RMS is non-finite")
    return value


def _centered_rms(prefix: list[float], center: int,
                  left: int, right: int) -> float:
    return _rms(prefix, center - left, center + right, "centered envelope")


def _peak_centered_rms(prefix: list[float], start: int, end: int,
                       left: int, right: int,
                       label: str) -> tuple[int, float]:
    sample_count = len(prefix) - 1
    _require(left <= start < end <= sample_count - right + 1,
             f"{label} range is outside audio")
    best_index = start
    best_value = _centered_rms(prefix, start, left, right)
    for index in range(start + 1, end):
        value = _centered_rms(prefix, index, left, right)
        if value > best_value:
            best_index = index
            best_value = value
    _require(math.isfinite(best_value), f"{label} envelope peak is non-finite")
    return best_index, best_value


def _earliest_near_peak_centered_rms(
        prefix: list[float], start: int, end: int,
        left: int, right: int, label: str) -> tuple[int, float]:
    _, peak = _peak_centered_rms(prefix, start, end, left, right, label)
    threshold = peak * 10.0 ** (-PEAK_TIME_TOLERANCE_DB / 20.0)
    for index in range(start, end):
        if _centered_rms(prefix, index, left, right) >= threshold:
            return index, peak
    raise Invalid(f"{label} has no near-peak envelope frame")


def analyze_signal(samples: list[float], sample_rate: int,
                   nominal_frequency_hz: float,
                   onset_search_start_frame: int, label: str) -> dict:
    """Apply the immutable native-rate-isolated-envelope/v1 profile."""
    _require(nominal_frequency_hz < sample_rate / 2.0,
             f"{label} nominal frequency reaches Nyquist")
    absolute_peak = max(abs(sample) for sample in samples)
    full_scale_samples = sum(abs(sample) >= 1.0 for sample in samples)
    noise_frames = _frames(NOISE_MS, sample_rate)
    post_frames = _frames(POST_ONSET_MS, sample_rate)
    search_frames = _frames(ONSET_PEAK_SEARCH_MS, sample_rate)
    envelope_frames = _frames(ENVELOPE_MS, sample_rate)
    left = envelope_frames // 2
    right = envelope_frames - left
    _integer(onset_search_start_frame, 1,
             f"{label} onset search start frame")
    _require(onset_search_start_frame >= noise_frames,
             f"{label} needs {NOISE_MS} ms of pre-onset noise before its "
             "declared search start")
    _require(onset_search_start_frame + post_frames <= len(samples),
             f"{label} has less than {POST_ONSET_MS} ms after its declared "
             "onset search start")

    prefix = _energy_prefix(samples)
    noise_start = onset_search_start_frame - noise_frames
    noise_rms = _rms(prefix, noise_start, onset_search_start_frame,
                     f"{label} pre-onset noise")
    first_center = max(onset_search_start_frame, left)
    search_end = min(len(samples) - right + 1,
                     first_center + search_frames)
    _require(first_center < search_end,
             f"{label} has no valid onset search interval")
    envelope_peak_frame, envelope_peak = _earliest_near_peak_centered_rms(
        prefix, first_center, search_end, left, right,
        f"{label} onset search")
    _require(envelope_peak > 0.0, f"{label} is silent in the onset search")
    threshold = ONSET_FRACTION * envelope_peak
    onset = None
    for center in range(first_center, envelope_peak_frame + 1):
        if _centered_rms(prefix, center, left, right) >= threshold:
            onset = center
            break
    _require(onset is not None, f"{label} has no valid onset")
    _require(onset + post_frames <= len(samples),
             f"{label} has less than {POST_ONSET_MS} ms after onset")

    baseline_end = onset + _frames(RMS_WINDOWS_MS[0][1], sample_rate)
    baseline_rms = _rms(prefix, onset, baseline_end, f"{label} 0-50 ms")
    _require(baseline_rms > 0.0, f"{label} has zero onset-window energy")
    snr_db = None
    if noise_rms > 0.0:
        snr_db = _db_ratio(baseline_rms, noise_rms, f"{label} onset SNR")
        _require(snr_db >= MINIMUM_SNR_DB,
                 f"{label} onset SNR {snr_db:.3f} dB is below "
                 f"{MINIMUM_SNR_DB:.1f} dB")

    relative_rms_db = []
    for start_ms, end_ms in RMS_WINDOWS_MS:
        start = onset + _frames(start_ms, sample_rate) if start_ms else onset
        end = onset + _frames(end_ms, sample_rate)
        value = _rms(prefix, start, end, f"{label} {start_ms}-{end_ms} ms")
        relative_rms_db.append(
            _db_ratio(value, baseline_rms,
                      f"{label} {start_ms}-{end_ms} ms normalization"))

    peak_search_end = min(len(samples) - right + 1, onset + search_frames)
    onset_peak_frame, onset_peak = _earliest_near_peak_centered_rms(
        prefix, onset, peak_search_end, left, right,
        f"{label} onset-to-peak")

    half_period = max(1, int(math.floor(
        0.5 * sample_rate / nominal_frequency_hz + 0.5)))
    one_and_half_periods = max(half_period + 1, int(math.floor(
        1.5 * sample_rate / nominal_frequency_hz + 0.5)))
    _require(onset + one_and_half_periods <= len(samples) - right + 1,
             f"{label} is too short for its first-cycle descriptor")
    first_frame, first_peak = _earliest_near_peak_centered_rms(
        prefix, onset, onset + half_period, left, right,
        f"{label} first crest")
    return_frame, return_peak = _earliest_near_peak_centered_rms(
        prefix, onset + half_period, onset + one_and_half_periods,
        left, right, f"{label} return crest")
    _require(first_peak > 0.0 and return_peak > 0.0,
             f"{label} first-cycle crest is zero")

    return {
        "absolute_peak": absolute_peak,
        "samples_at_or_above_full_scale": full_scale_samples,
        "onset_search_start_frame": onset_search_start_frame,
        "noise_window_start_frame": noise_start,
        "onset_frame": onset,
        "onset_time_seconds": onset / float(sample_rate),
        "noise_rms": noise_rms,
        "baseline_0_50_ms_rms": baseline_rms,
        "onset_snr_db": snr_db,
        "pre_onset_noise_is_exactly_zero": noise_rms == 0.0,
        "envelope_peak_frame": envelope_peak_frame,
        "envelope_threshold_rms": threshold,
        "onset_to_peak_ms": 1000.0 * (onset_peak_frame - onset) / sample_rate,
        "onset_envelope_peak_rms": onset_peak,
        "rms_relative_db": relative_rms_db,
        "first_crest_ms": 1000.0 * (first_frame - onset) / sample_rate,
        "return_crest_ms": 1000.0 * (return_frame - onset) / sample_rate,
        "return_over_first_db": _db_ratio(
            return_peak, first_peak, f"{label} return/first crest"),
    }


def _frozen_number(value, expected: float, label: str) -> None:
    _require(type(value) in (int, float) and math.isfinite(value)
             and float(value) == expected,
             f"{label} differs from the frozen v3 contract")


def _load_model_manifest(path: Path, expected_digest: str) -> tuple[dict, str]:
    manifest, digest = reference._load_json_snapshot(path.resolve(), "model manifest")
    _require(digest == expected_digest, "model manifest SHA-256 mismatch")
    manifest = reference._exact_keys(
        manifest,
        {"schema", "generator", "audio_format", "signal_chain", "protocol",
         "engine_parameters", "performance_controls", "probes"},
        "model manifest",
    )
    reference._scan_filled(manifest, "model manifest")
    _require(manifest["schema"] == MODEL_SCHEMA,
             "model manifest must be electry-evaluation/v3")

    generator = reference._exact_keys(
        manifest["generator"],
        {"name", "project_version", "build_mode", "determinism_scope"},
        "model manifest generator",
    )
    _require(generator["name"] == "ElectryRenderEvaluation"
             and isinstance(generator["project_version"], str)
             and generator["project_version"]
             and generator["build_mode"] in {"debug", "release"}
             and generator["determinism_scope"]
                 == "same executable and CPU architecture",
             "model manifest generator differs from the frozen v3 contract")

    audio_format = reference._exact_keys(
        manifest["audio_format"],
        {"container", "encoding", "bits_per_sample", "channels",
         "sample_rate_hz", "normalization_applied", "post_render_gain"},
        "model manifest audio_format",
    )
    _require(audio_format["container"] == "WAVE"
             and audio_format["encoding"] == "IEEE_FLOAT"
             and audio_format["bits_per_sample"] == 32
             and audio_format["channels"] == 1
             and audio_format["sample_rate_hz"] == MODEL_SAMPLE_RATE
             and audio_format["normalization_applied"] is False,
             "model manifest is not the raw mono 44.1 kHz v3 contract")
    _frozen_number(audio_format["post_render_gain"], 1.0,
                   "model manifest post-render gain")

    signal_chain = reference._exact_keys(
        manifest["signal_chain"], {"path", "amplitude_reference"},
        "model manifest signal_chain")
    _require(signal_chain["path"]
             == "ElectryEngine dry DI; ElectryFx not instantiated"
             and signal_chain["amplitude_reference"]
             == "arbitrary digital model full scale; not volts and not level matched",
             "model manifest signal chain differs from the frozen dry-DI contract")

    protocol = reference._exact_keys(
        manifest["protocol"],
        {"instrument", "tuning_low_to_high", "targets", "velocity",
         "lead_in_frames", "held_frames", "release_frames", "block_size"},
        "model manifest protocol",
    )
    _require(protocol["instrument"] == "eight-string guitar"
             and protocol["tuning_low_to_high"]
                 == ["E1", "B1", "E2", "A2", "D3", "G3", "B3", "E4"]
             and protocol["lead_in_frames"] == MODEL_LEAD_IN_FRAMES
             and protocol["held_frames"] == MODEL_HELD_FRAMES
             and protocol["release_frames"] == MODEL_RELEASE_FRAMES
             and protocol["block_size"] == 256,
             "model manifest protocol differs from the frozen v3 contract")
    _frozen_number(protocol["velocity"], 0.94999999,
                   "model manifest protocol velocity")
    expected_targets = (
        ("e1", 8, 28, 41.20344461),
        ("e2", 6, 40, 82.40688923),
    )
    targets = protocol["targets"]
    _require(isinstance(targets, list) and len(targets) == len(expected_targets),
             "model manifest targets differ from the frozen v3 contract")
    for index, (target, expected) in enumerate(zip(targets, expected_targets)):
        target = reference._exact_keys(
            target, {"id", "string", "fret", "midi_note",
                     "equal_temperament_frequency_hz"},
            f"model manifest target {index}")
        target_id, string_number, midi_note, frequency = expected
        _require(target["id"] == target_id
                 and target["string"] == string_number
                 and target["fret"] == 0
                 and target["midi_note"] == midi_note,
                 f"model manifest target {index} differs from frozen v3")
        _frozen_number(target["equal_temperament_frequency_hz"], frequency,
                       f"model manifest target {index} frequency")

    engine_parameter_numbers = {
        "guitar_build", "body_wood", "body_size", "body_shape",
        "construction", "scale_length", "pickup_type", "tone_knob",
        "body_resonance", "string_gauge", "string_age", "pick_position",
        "pick_hardness", "pick_noise", "finger_noise", "release_noise",
        "mute_damping", "bend_time_seconds", "velocity_amount", "output_gain",
        "artifact_amount", "sympathetic_amount", "palm_mute",
        "strum_spread_seconds", "resonance_depth", "vibrato_depth",
    }
    engine_parameters = reference._exact_keys(
        manifest["engine_parameters"],
        engine_parameter_numbers | {"pickup_selector", "output_mode"},
        "model manifest engine_parameters",
    )
    _require(engine_parameters["pickup_selector"]
             in {"neck", "both", "bridge"}
             and engine_parameters["output_mode"] == "mono",
             "model manifest engine routing differs from the v3 contract")
    for key in engine_parameter_numbers:
        value = engine_parameters[key]
        _require(type(value) in (int, float) and math.isfinite(value),
                 f"model manifest engine parameter {key} must be finite")
    for key in ("sympathetic_amount", "palm_mute", "strum_spread_seconds"):
        _frozen_number(engine_parameters[key], 0.0,
                       f"model manifest engine parameter {key}")
    controls = reference._exact_keys(
        manifest["performance_controls"],
        {"pitch_bend", "mod_wheel_resonance", "palm_mute_pressure",
         "channel_pressure_vibrato", "sustain_pedal", "acoustic_return_level"},
        "model manifest performance_controls",
    )
    _require(controls["sustain_pedal"] is False,
             "model manifest sustain pedal must be off")
    for key in ("pitch_bend", "mod_wheel_resonance", "palm_mute_pressure",
                "channel_pressure_vibrato", "acoustic_return_level"):
        _frozen_number(controls[key], 0.0,
                       f"model manifest performance control {key}")

    probes = manifest["probes"]
    _require(isinstance(probes, list) and len(probes) == len(MODEL_PROBES),
             "model manifest must contain the ten frozen v3 probes")
    for index, (probe, expected) in enumerate(zip(probes, MODEL_PROBES)):
        label = f"model manifest probe {index}"
        probe = reference._exact_keys(
            probe,
            {"id", "file", "play_style", "target_string", "target_fret",
             "midi_note", "equal_temperament_frequency_hz", "mute_damping",
             "frames", "raw_peak", "events"},
            label,
        )
        (probe_id, play_style, string_number, midi_note, frequency,
         damping, style_note) = expected
        _require(probe["id"] == probe_id
                 and probe["file"] == f"{probe_id}.wav"
                 and probe["play_style"] == play_style
                 and probe["target_string"] == string_number
                 and probe["target_fret"] == 0
                 and probe["midi_note"] == midi_note
                 and probe["frames"] == MODEL_TOTAL_FRAMES,
                 f"{label} metadata differs from frozen v3")
        _frozen_number(probe["equal_temperament_frequency_hz"], frequency,
                       f"{label} frequency")
        _frozen_number(probe["mute_damping"], damping, f"{label} damping")
        _positive_float(probe["raw_peak"], f"{label} raw peak")

        events = probe["events"]
        _require(isinstance(events, list) and len(events) == 4,
                 f"{label} must contain the four frozen events")
        pick = reference._exact_keys(
            events[0], {"type", "bank", "value", "midi_note", "sample",
                        "time_seconds"}, f"{label} pick event")
        style = reference._exact_keys(
            events[1], {"type", "bank", "value", "midi_note", "sample",
                        "time_seconds"}, f"{label} style event")
        note_on = reference._exact_keys(
            events[2], {"type", "midi_note", "velocity", "sample",
                        "time_seconds"}, f"{label} note-on event")
        note_off = reference._exact_keys(
            events[3], {"type", "midi_note", "sample", "time_seconds"},
            f"{label} note-off event")
        _require(pick["type"] == "keyswitch" and pick["bank"] == "pick_style"
                 and pick["value"] == "down" and pick["midi_note"] == 12
                 and pick["sample"] == 0,
                 f"{label} pick event differs from frozen v3")
        _frozen_number(pick["time_seconds"], 0.0,
                       f"{label} pick event time")
        _require(style["type"] == "keyswitch"
                 and style["bank"] == "play_style"
                 and style["value"] == play_style
                 and style["midi_note"] == style_note
                 and style["sample"] == 0,
                 f"{label} style event differs from frozen v3")
        _frozen_number(style["time_seconds"], 0.0,
                       f"{label} style event time")
        _require(note_on["type"] == "note_on"
                 and note_on["midi_note"] == midi_note
                 and note_on["sample"] == MODEL_LEAD_IN_FRAMES,
                 f"{label} note-on event differs from frozen v3")
        _frozen_number(note_on["velocity"], 0.94999999,
                       f"{label} note-on velocity")
        _frozen_number(note_on["time_seconds"], 0.25,
                       f"{label} note-on time")
        _require(note_off["type"] == "note_off"
                 and note_off["midi_note"] == midi_note
                 and note_off["sample"]
                     == MODEL_LEAD_IN_FRAMES + MODEL_HELD_FRAMES,
                 f"{label} note-off event differs from frozen v3")
        _frozen_number(note_off["time_seconds"], 2.25,
                       f"{label} note-off time")
    return manifest, digest


def _find_unique(items: list, key: str, value: str, label: str) -> dict:
    matches = [item for item in items
               if isinstance(item, dict) and item.get(key) == value]
    _require(len(matches) == 1, f"{label} must resolve exactly once")
    return matches[0]


def _runtime_record() -> dict:
    return {
        "python_implementation": platform.python_implementation(),
        "python_version": platform.python_version(),
        "python_build": list(platform.python_build()),
        "python_compiler": platform.python_compiler(),
        "operating_system": platform.system(),
        "operating_system_release": platform.release(),
        "machine": platform.machine(),
        "libc": list(platform.libc_ver()),
        "float_mantissa_bits": sys.float_info.mant_dig,
        "determinism_scope": (
            "same input and source hashes on this recorded Python/runtime host"),
    }


def compare(plan_path: Path, receipt_path: Path,
            model_manifest_path: Path, output_path: Path | None = None) -> dict:
    plan_path = plan_path.resolve()
    receipt_path = receipt_path.resolve()
    model_manifest_path = model_manifest_path.resolve()
    plan, plan_digest = _load_plan(plan_path)

    analyzer_path = Path(__file__).resolve()
    validator_path = Path(reference.__file__).resolve()
    analyzer_digest = _digest(analyzer_path, "analyzer")
    validator_digest = _digest(validator_path, "fixture validator")
    _require(analyzer_digest == plan["analyzer_sha256"],
             "analyzer SHA-256 mismatch")
    _require(validator_digest == plan["fixture_validator_sha256"],
             "fixture validator SHA-256 mismatch")

    receipt_digest = reference.validate(receipt_path)
    _require(receipt_digest == plan["reference_receipt_sha256"],
             "reference receipt SHA-256 mismatch")
    receipt, receipt_reread_digest = reference._load_json_snapshot(
        receipt_path, "reference receipt")
    _require(receipt_reread_digest == receipt_digest,
             "reference receipt changed after validation")
    model_manifest, model_manifest_digest = _load_model_manifest(
        model_manifest_path, plan["model_manifest_sha256"])

    reference_assets = receipt["assets"]
    model_probes = model_manifest["probes"]
    analyzed_model = {}
    result_pairs = []
    input_paths = {plan_path, receipt_path, model_manifest_path,
                   analyzer_path, validator_path}
    for index, asset in enumerate(reference_assets):
        input_paths.add(reference._resolve_file(
            receipt_path, asset["file"], f"reference asset {index} audio"))
        input_paths.add(reference._resolve_file(
            receipt_path, asset["evidence_record_file"],
            f"reference asset {index} evidence"))
    model_paths = {}
    for index, probe in enumerate(model_probes):
        model_path = reference._resolve_file(
            model_manifest_path, probe["file"],
            f"model manifest probe {index} audio")
        model_paths[probe["id"]] = model_path
        input_paths.add(model_path)

    for row in plan["pairs"]:
        pair_id = row["id"]
        asset = _find_unique(reference_assets, "upstream_asset_id",
                             row["reference_asset_id"],
                             f"pair {pair_id} reference asset")
        reference_path = reference._resolve_file(
            receipt_path, asset["file"], f"pair {pair_id} reference audio")
        input_paths.add(reference_path)
        reference_info, reference_digest, reference_samples = read_wave_channel(
            reference_path,
            row["reference_channel_index"],
            row["reference_crop_start_frame"],
            row["reference_crop_frames"],
            f"pair {pair_id} reference audio",
        )
        _require(reference_digest == asset["sha256"],
                 f"pair {pair_id} reference audio SHA-256 mismatch")
        probe = _find_unique(model_probes, "id", row["model_probe_id"],
                             f"pair {pair_id} model probe")
        model_frequency = _positive_float(
            probe["equal_temperament_frequency_hz"],
            f"pair {pair_id} model nominal frequency")
        reference_frequency = float(row["reference_nominal_frequency_hz"])
        _require(reference_frequency == model_frequency,
                 f"pair {pair_id} reference and model nominal notes differ")
        reference_analysis = analyze_signal(
            reference_samples,
            reference_info["sample_rate_hz"],
            reference_frequency,
            row["reference_onset_search_start_frame"],
            f"pair {pair_id} reference",
        )

        model_path = model_paths[probe["id"]]
        _require(not reference._same_file(
            reference_path, model_path, f"pair {pair_id} audio identity"),
            f"pair {pair_id} reference and model audio are the same file")
        cache_key = (row["model_probe_id"], str(model_path),
                     row["model_audio_sha256"])
        if cache_key not in analyzed_model:
            model_info, model_digest, model_samples = read_wave_channel(
                model_path, label=f"pair {pair_id} model audio")
            _require(model_digest == row["model_audio_sha256"],
                     f"pair {pair_id} model audio SHA-256 mismatch")
            _require(model_info["encoding"] == "IEEE_FLOAT"
                     and model_info["bits_per_sample"] == 32
                     and model_info["channels"] == 1
                     and model_info["sample_rate_hz"]
                         == model_manifest["audio_format"]["sample_rate_hz"],
                     f"pair {pair_id} model WAV differs from its manifest format")
            _require(model_info["frames"] == probe.get("frames"),
                     f"pair {pair_id} model WAV frame count differs from its probe")
            model_analysis = analyze_signal(
                model_samples, model_info["sample_rate_hz"], model_frequency,
                probe["events"][2]["sample"],
                f"pair {pair_id} model")
            _require(abs(model_analysis["absolute_peak"]
                         - float(probe["raw_peak"])) <= 5.1e-9,
                     f"pair {pair_id} model peak differs from its probe")
            analyzed_model[cache_key] = (
                model_info, model_digest, model_frequency, model_analysis)
        model_info, model_digest, model_frequency, model_analysis = \
            analyzed_model[cache_key]
        _require(model_digest != reference_digest,
                 f"pair {pair_id} reference and model audio are byte-identical")

        reference_rms = reference_analysis["rms_relative_db"]
        model_rms = model_analysis["rms_relative_db"]
        result_pairs.append({
            "id": pair_id,
            "reference": {
                "fixture_id": receipt["fixture_id"],
                "upstream_asset_id": row["reference_asset_id"],
                "audio_sha256": reference_digest,
                "channel_index": row["reference_channel_index"],
                "crop_start_frame": row["reference_crop_start_frame"],
                "crop_frames": row["reference_crop_frames"],
                "onset_search_start_frame": (
                    row["reference_onset_search_start_frame"]),
                "sample_rate_hz": reference_info["sample_rate_hz"],
                "nominal_frequency_hz": row["reference_nominal_frequency_hz"],
                "analysis": reference_analysis,
            },
            "model": {
                "probe_id": row["model_probe_id"],
                "audio_sha256": model_digest,
                "frames": model_info["frames"],
                "sample_rate_hz": model_info["sample_rate_hz"],
                "nominal_frequency_hz": model_frequency,
                "analysis": model_analysis,
            },
            "descriptive_difference_model_minus_reference": {
                "onset_to_peak_ms": (
                    model_analysis["onset_to_peak_ms"]
                    - reference_analysis["onset_to_peak_ms"]),
                "rms_relative_db": [
                    model - actual
                    for model, actual in zip(model_rms, reference_rms)
                ],
                "return_over_first_db": (
                    model_analysis["return_over_first_db"]
                    - reference_analysis["return_over_first_db"]),
            },
        })

    if output_path is not None:
        resolved_output = output_path.resolve()
        _require(resolved_output not in input_paths,
                 "output path aliases a comparison input")
        if output_path.exists():
            for input_path in input_paths:
                _require(not reference._same_file(
                    output_path, input_path, "comparison output"),
                    "output path aliases a comparison input")

    return {
        "schema": RESULT_SCHEMA,
        "status": "descriptive_complete",
        "scientific_scope": SCOPE,
        "decision_rule": "none_descriptive_only",
        "generator": {
            "name": "CompareReferenceFixture.py",
            "sha256": analyzer_digest,
            "fixture_validator_sha256": validator_digest,
            "runtime": _runtime_record(),
        },
        "inputs": {
            "comparison_plan_sha256": plan_digest,
            "reference_receipt_sha256": receipt_digest,
            "fixture_id": receipt["fixture_id"],
            "model_manifest_schema": model_manifest["schema"],
            "model_manifest_sha256": model_manifest_digest,
            "model_build_mode": model_manifest["generator"]["build_mode"],
            "model_engine_parameters": model_manifest["engine_parameters"],
            "model_performance_controls": (
                model_manifest["performance_controls"]),
        },
        "analysis_profile": {
            "id": PROFILE,
            "native_sample_rates": True,
            "resampling": False,
            "channel_policy": "declared_zero_based_channel_no_downmix",
            "envelope_ms": ENVELOPE_MS,
            "envelope_indexing": "centered_left=floor(N/2)_right=N-left",
            "onset_rule": (
                "first_>=_25_percent_of_first_second_envelope_peak_at_or_after_"
                "declared_search_start"),
            "peak_time_rule": (
                "earliest_centered_envelope_frame_within_0.05_dB_of_maximum"),
            "peak_time_tolerance_db": PEAK_TIME_TOLERANCE_DB,
            "noise_window_ms_immediately_before_search_start": NOISE_MS,
            "minimum_onset_snr_db": MINIMUM_SNR_DB,
            "exactly_zero_noise_policy": "passes_with_null_snr_db_and_zero_flag",
            "rms_normalization_window_ms": list(RMS_WINDOWS_MS[0]),
            "rms_windows_ms": [list(window) for window in RMS_WINDOWS_MS],
            "first_cycle_rule": (
                "centered_envelope_peak_0_to_half_period_and_"
                "half_to_1.5_periods"),
            "fit": False,
            "aggregate_score": False,
        },
        "pairs": result_pairs,
    }


def _write_atomic(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path = None
    try:
        with tempfile.NamedTemporaryFile(
                mode="wb", dir=path.parent,
                prefix=f".{path.name}.", suffix=".tmp",
                delete=False) as stream:
            temporary_path = Path(stream.name)
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary_path, path)
        temporary_path = None
    finally:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--print-template", action="store_true",
                        help="print a fillable frozen comparison plan and exit")
    parser.add_argument("--plan", type=Path)
    parser.add_argument("--receipt", type=Path)
    parser.add_argument("--model-manifest", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.print_template:
        if any((args.plan, args.receipt, args.model_manifest, args.output)):
            parser.error("--print-template does not accept comparison paths")
        print(json.dumps(PLAN_TEMPLATE, indent=2, ensure_ascii=False))
        return 0
    if not all((args.plan, args.receipt, args.model_manifest, args.output)):
        parser.error("--plan, --receipt, --model-manifest and --output are required")

    try:
        inputs = {args.plan.resolve(), args.receipt.resolve(),
                  args.model_manifest.resolve(), Path(__file__).resolve(),
                  Path(reference.__file__).resolve()}
        output = args.output.resolve()
        _require(output not in inputs, "output path aliases a comparison input")
        result = compare(args.plan, args.receipt, args.model_manifest, output)
        serialized = (json.dumps(result, indent=2, ensure_ascii=False,
                                 allow_nan=False) + "\n").encode("utf-8")
        _write_atomic(output, serialized)
        print(f"PASS {output} SHA-256 {hashlib.sha256(serialized).hexdigest()}")
    except (Invalid, OSError, OverflowError, ValueError) as exc:
        print(f"invalid: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
