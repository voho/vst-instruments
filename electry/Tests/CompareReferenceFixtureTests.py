#!/usr/bin/env python3

import hashlib
import importlib.util
import json
import math
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TOOL_PATH = ROOT / "Tools" / "CompareReferenceFixture.py"
sys.dont_write_bytecode = True
SPEC = importlib.util.spec_from_file_location("compare_reference_fixture", TOOL_PATH)
TOOL = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(TOOL)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
    path.write_text(
        json.dumps(value, ensure_ascii=True, separators=(",", ":"), sort_keys=True),
        encoding="utf-8")


def write_wave(path, encoding, sample_rate, channel_samples):
    bits = {"pcm16": 16, "pcm24": 24, "pcm32": 32, "float32": 32}[encoding]
    tag = 3 if encoding == "float32" else 1
    channels = len(channel_samples)
    frames = len(channel_samples[0])
    assert channels and all(len(samples) == frames for samples in channel_samples)
    sample_bytes = bits // 8
    payload = bytearray()
    for frame in range(frames):
        for samples in channel_samples:
            value = samples[frame]
            if tag == 3:
                payload.extend(struct.pack("<f", value))
            else:
                payload.extend(int(value).to_bytes(sample_bytes, "little", signed=True))
    block_align = channels * sample_bytes
    format_chunk = struct.pack(
        "<HHIIHH", tag, channels, sample_rate, sample_rate * block_align,
        block_align, bits)
    padding = b"\0" if len(payload) & 1 else b""
    body = (b"WAVE" + b"fmt " + struct.pack("<I", len(format_chunk))
            + format_chunk + b"data" + struct.pack("<I", len(payload))
            + payload + padding)
    path.write_bytes(b"RIFF" + struct.pack("<I", len(body)) + body)


def profile_signal(sample_rate, onset, frames):
    envelope_frames = (2 * sample_rate + 500) // 1000
    right = envelope_frames - envelope_frames // 2
    threshold_samples = (envelope_frames + 15) // 16
    signal_start = onset + right - threshold_samples
    boundaries = (
        (50, 0.5),
        (150, 0.25),
        (500, 0.125),
        (1000, 0.0625),
        (2000, 0.03125),
    )
    samples = [0.0] * frames
    for frame in range(signal_start, frames):
        elapsed = frame - onset
        amplitude = boundaries[-1][1]
        for end_ms, value in boundaries:
            if elapsed < (end_ms * sample_rate + 500) // 1000:
                amplitude = value
                break
        samples[frame] = amplitude if frame % 2 == 0 else -amplitude
    return samples


def build_fixture(root):
    external = root / "external"
    evidence_dir = root / "evidence"
    model_dir = root / "model"
    external.mkdir()
    evidence_dir.mkdir()
    model_dir.mkdir()

    reference_rate = 8000
    reference_crop_start = 37
    reference_crop_frames = 16800
    reference_samples = profile_signal(reference_rate, 600, reference_crop_frames)
    reference_full = [0.0] * reference_crop_start + reference_samples
    reference_audio = external / "exact-eight-reference.wav"
    write_wave(
        reference_audio, "pcm24", reference_rate,
        [[0] * len(reference_full),
         [round(sample * (1 << 23)) for sample in reference_full]])

    dataset_id = "synthetic-exact-eight"
    snapshot = "Synthetic test snapshot 2026-08-29"
    asset_id = "synthetic-e1"
    source_url = "https://example.com/exact-eight/synthetic-e1.wav"
    audio_hash = digest(reference_audio)
    evidence = {
        "schema": "electry-reference-asset-evidence/v1",
        "dataset_id": dataset_id,
        "upstream_snapshot": snapshot,
        "instrument_string_count": 8,
        "upstream_asset_id": asset_id,
        "source_url": source_url,
        "download_representation": "upstream_original",
        "audio_sha256": audio_hash,
        "rights_evidence_kind": "per_asset_cc0_record",
        "rights_basis_id": "CC0-1.0",
        "commercial_model_calibration": True,
        "private_competitive_evaluation": True,
        "evidence_text": "Synthetic per-asset CC0 evidence used only by this test.",
    }
    evidence_path = evidence_dir / "synthetic-e1.json"
    write_json(evidence_path, evidence)
    receipt = {
        "schema": "electry-reference-fixture/v1",
        "status": "frozen",
        "fixture_id": "synthetic-exact-eight-originals",
        "target_protocol": "electry-evaluation/v4",
        "scientific_scope": "descriptive_comparison_only_no_fit_or_promotion",
        "source": {
            "dataset_id": dataset_id,
            "canonical_url": "https://example.com/exact-eight/",
            "upstream_snapshot": snapshot,
            "retrieved_utc": "2026-08-29T12:00:00Z",
            "instrument_string_count": 8,
        },
        "rights_review": {
            "reviewer_id": "test-reviewer",
            "reviewed_utc": "2026-08-29T12:05:00Z",
            "covers_all_listed_audio_assets": True,
            "commercial_model_calibration": True,
            "private_competitive_evaluation": True,
        },
        "assets": [{
            "upstream_asset_id": asset_id,
            "source_url": source_url,
            "download_representation": "upstream_original",
            "file": "external/exact-eight-reference.wav",
            "sha256": audio_hash,
            "evidence_record_file": "evidence/synthetic-e1.json",
            "evidence_record_sha256": digest(evidence_path),
            "rights_evidence_kind": "per_asset_cc0_record",
            "rights_basis_id": "CC0-1.0",
        }],
    }
    receipt_path = root / "receipt.json"
    write_json(receipt_path, receipt)

    model_rate = TOOL.MODEL_SAMPLE_RATE
    model_onset = TOOL.MODEL_LEAD_IN_FRAMES + 882
    model_samples = profile_signal(
        model_rate, model_onset, TOOL.MODEL_TOTAL_FRAMES)
    unselected_model = [0.0] * TOOL.MODEL_TOTAL_FRAMES
    unselected_model[TOOL.MODEL_LEAD_IN_FRAMES + 1] = 0.25
    model_paths = {}
    probes = []
    for (probe_id, play_style, string_number, midi_note, frequency,
         damping, style_note) in TOOL.MODEL_PROBES:
        model_path = model_dir / f"{probe_id}.wav"
        probe_samples = (model_samples if probe_id == "e1-open"
                         else unselected_model)
        write_wave(
            model_path, "float32", model_rate, [probe_samples])
        model_paths[probe_id] = model_path
        probes.append({
            "id": probe_id,
            "file": f"{probe_id}.wav",
            "play_style": play_style,
            "target_string": string_number,
            "target_fret": 0,
            "midi_note": midi_note,
            "equal_temperament_frequency_hz": frequency,
            "mute_damping": damping,
            "frames": TOOL.MODEL_TOTAL_FRAMES,
            "raw_peak": round(max(abs(sample) for sample in probe_samples), 8),
            "events": [
                {"type": "keyswitch", "bank": "pick_style",
                 "value": "down", "midi_note": 12, "sample": 0,
                 "time_seconds": 0.0},
                {"type": "keyswitch", "bank": "play_style",
                 "value": play_style, "midi_note": style_note, "sample": 0,
                 "time_seconds": 0.0},
                {"type": "note_on", "midi_note": midi_note,
                 "velocity": 0.94999999,
                 "sample": TOOL.MODEL_LEAD_IN_FRAMES,
                 "time_seconds": 0.25},
                {"type": "note_off", "midi_note": midi_note,
                 "sample": TOOL.MODEL_LEAD_IN_FRAMES + TOOL.MODEL_HELD_FRAMES,
                 "time_seconds": 2.25},
            ],
        })
    model_audio = model_paths["e1-open"]
    model_manifest = {
        "schema": "electry-evaluation/v3",
        "generator": {
            "name": "ElectryRenderEvaluation",
            "project_version": "1.2.0-test",
            "build_mode": "release",
            "determinism_scope": "same executable and CPU architecture",
        },
        "audio_format": {
            "container": "WAVE",
            "encoding": "IEEE_FLOAT",
            "bits_per_sample": 32,
            "channels": 1,
            "sample_rate_hz": model_rate,
            "normalization_applied": False,
            "post_render_gain": 1,
        },
        "signal_chain": {
            "path": "ElectryEngine dry DI; ElectryFx not instantiated",
            "amplitude_reference": (
                "arbitrary digital model full scale; not volts and not level matched"),
        },
        "protocol": {
            "instrument": "eight-string guitar",
            "tuning_low_to_high": [
                "E1", "B1", "E2", "A2", "D3", "G3", "B3", "E4"],
            "targets": [
                {"id": "e1", "string": 8, "fret": 0, "midi_note": 28,
                 "equal_temperament_frequency_hz": 41.20344461},
                {"id": "e2", "string": 6, "fret": 0, "midi_note": 40,
                 "equal_temperament_frequency_hz": 82.40688923},
            ],
            "velocity": 0.94999999,
            "lead_in_frames": TOOL.MODEL_LEAD_IN_FRAMES,
            "held_frames": TOOL.MODEL_HELD_FRAMES,
            "release_frames": TOOL.MODEL_RELEASE_FRAMES,
            "block_size": 256,
        },
        "engine_parameters": {
            "guitar_build": 0.80000001,
            "pickup_selector": "bridge",
            "body_wood": 0.80000001,
            "body_size": 0.5,
            "body_shape": 0.5,
            "construction": 0.5,
            "scale_length": 0.85000002,
            "pickup_type": 0.31999999,
            "tone_knob": 0.69999999,
            "body_resonance": 0.34999999,
            "string_gauge": 1.0,
            "string_age": 0.30000001,
            "pick_position": 0.18000001,
            "pick_hardness": 0.57999998,
            "pick_noise": 0.5,
            "finger_noise": 0.40000001,
            "release_noise": 0.40000001,
            "mute_damping": 0.55000001,
            "bend_time_seconds": 0.28,
            "velocity_amount": 0.85000002,
            "output_gain": 1.0,
            "artifact_amount": 0.18000001,
            "output_mode": "mono",
            "sympathetic_amount": 0.0,
            "palm_mute": 0.0,
            "strum_spread_seconds": 0.0,
            "resonance_depth": 0.34999999,
            "vibrato_depth": 0.30000001,
        },
        "performance_controls": {
            "pitch_bend": 0.0,
            "mod_wheel_resonance": 0.0,
            "palm_mute_pressure": 0.0,
            "channel_pressure_vibrato": 0.0,
            "sustain_pedal": False,
            "acoustic_return_level": 0.0,
        },
        "probes": probes,
    }
    model_manifest_path = model_dir / "manifest.json"
    write_json(model_manifest_path, model_manifest)

    plan = {
        "schema": "electry-reference-comparison-plan/v1",
        "status": "frozen",
        "target_protocol": "electry-evaluation/v4",
        "scientific_scope": "descriptive_comparison_only_no_fit_or_promotion",
        "analysis_profile": "native-rate-isolated-envelope/v1",
        "analyzer_sha256": digest(TOOL_PATH),
        "fixture_validator_sha256": digest(Path(TOOL.reference.__file__)),
        "reference_receipt_sha256": digest(receipt_path),
        "model_manifest_sha256": digest(model_manifest_path),
        "pairs": [{
            "id": "synthetic-e1-pair",
            "reference_asset_id": asset_id,
            "reference_channel_index": 1,
            "reference_crop_start_frame": reference_crop_start,
            "reference_crop_frames": reference_crop_frames,
            "reference_onset_search_start_frame": 400,
            "reference_nominal_frequency_hz": 41.20344461,
            "model_probe_id": "e1-open",
            "model_audio_sha256": digest(model_audio),
        }],
    }
    plan_path = root / "comparison-plan.json"
    write_json(plan_path, plan)
    return {
        "plan": plan,
        "plan_path": plan_path,
        "receipt_path": receipt_path,
        "model_manifest_path": model_manifest_path,
        "model_manifest": model_manifest,
        "model_audio": model_audio,
        "model_paths": model_paths,
        "model_samples": model_samples,
        "model_onset": model_onset,
        "reference_audio": reference_audio,
        "reference_crop_start": reference_crop_start,
        "reference_crop_frames": reference_crop_frames,
    }


def run_cli(fixture, output):
    return subprocess.run(
        [sys.executable, str(TOOL_PATH),
         "--plan", str(fixture["plan_path"]),
         "--receipt", str(fixture["receipt_path"]),
         "--model-manifest", str(fixture["model_manifest_path"]),
         "--output", str(output)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)


class CompareReferenceFixtureTests(unittest.TestCase):
    def test_decode_pcm16_pcm24_pcm32_and_float32(self):
        cases = (
            ("pcm16", [-32768, -16384, -1, 0, 1, 16384, 32767], 16),
            ("pcm24", [-8388608, -4194304, -1, 0, 1, 4194304, 8388607], 24),
            ("pcm32", [-2147483648, -1073741824, -1, 0, 1,
                       1073741824, 2147483647], 32),
            ("float32", [-1.0, -0.5, -2.0 ** -20, 0.0,
                         2.0 ** -20, 0.5, 0.75], 32),
        )
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for encoding, values, bits in cases:
                with self.subTest(encoding=encoding):
                    path = root / f"{encoding}.wav"
                    write_wave(path, encoding, 8000, [values])
                    info, audio_hash, samples = TOOL.read_wave_channel(path)
                    expected = (values if encoding == "float32" else
                                [value / float(1 << (bits - 1)) for value in values])
                    self.assertEqual(samples, expected)
                    self.assertEqual(info["bits_per_sample"], bits)
                    self.assertEqual(audio_hash, digest(path))

            nonfinite = root / "nonfinite.wav"
            write_wave(nonfinite, "float32", 8000, [[0.0, math.nan]])
            with self.assertRaisesRegex(TOOL.Invalid, "contains non-finite audio"):
                TOOL.read_wave_channel(nonfinite)

    def test_first_near_peak_is_stable_across_native_sample_rates(self):
        frequency = 41.20344461
        onset_to_peak_ms = []
        for sample_rate in (8000, 44100, 48000, 96000):
            with self.subTest(sample_rate=sample_rate):
                search_start = sample_rate // 20
                onset = sample_rate // 10
                frames = 22 * sample_rate // 10
                samples = [0.0] * onset + [
                    0.5 * math.sin(2.0 * math.pi * frequency * frame
                                   / sample_rate)
                    for frame in range(frames - onset)
                ]
                analysis = TOOL.analyze_signal(
                    samples, sample_rate, frequency, search_start,
                    f"{sample_rate} Hz sine")
                onset_to_peak_ms.append(analysis["onset_to_peak_ms"])
                self.assertLess(
                    abs(analysis["return_over_first_db"]), 0.001)
        self.assertLessEqual(
            max(onset_to_peak_ms) - min(onset_to_peak_ms), 0.05)

    def test_frozen_comparison_is_byte_deterministic_and_native_rate(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixture = build_fixture(root)
            outputs = (root / "result-a.json", root / "result-b.json")
            runs = [run_cli(fixture, output) for output in outputs]
            for run, output in zip(runs, outputs):
                self.assertEqual(run.returncode, 0, run.stderr.decode())
                self.assertEqual(run.stderr, b"")
                self.assertIn(digest(output).encode("ascii"), run.stdout)
            self.assertEqual(outputs[0].read_bytes(), outputs[1].read_bytes())

            result = json.loads(outputs[0].read_text(encoding="utf-8"))
            self.assertEqual(result["generator"]["sha256"], digest(TOOL_PATH))
            self.assertEqual(
                result["generator"]["fixture_validator_sha256"],
                digest(Path(TOOL.reference.__file__)))
            self.assertEqual(
                result["inputs"]["comparison_plan_sha256"],
                digest(fixture["plan_path"]))
            self.assertEqual(
                result["inputs"]["reference_receipt_sha256"],
                digest(fixture["receipt_path"]))
            self.assertEqual(
                result["inputs"]["model_manifest_sha256"],
                digest(fixture["model_manifest_path"]))
            self.assertTrue(result["analysis_profile"]["native_sample_rates"])
            self.assertFalse(result["analysis_profile"]["resampling"])

            pair = result["pairs"][0]
            self.assertEqual(pair["reference"]["audio_sha256"],
                             digest(fixture["reference_audio"]))
            self.assertEqual(pair["model"]["audio_sha256"],
                             digest(fixture["model_audio"]))
            self.assertEqual(pair["reference"]["sample_rate_hz"], 8000)
            self.assertEqual(pair["model"]["sample_rate_hz"], 44100)
            self.assertEqual(pair["reference"]["channel_index"], 1)
            self.assertEqual(pair["reference"]["crop_start_frame"],
                             fixture["reference_crop_start"])
            self.assertEqual(pair["reference"]["crop_frames"],
                             fixture["reference_crop_frames"])
            self.assertEqual(
                pair["reference"]["analysis"]["onset_search_start_frame"], 400)
            self.assertEqual(pair["reference"]["analysis"]["onset_frame"], 600)
            self.assertEqual(
                pair["model"]["analysis"]["onset_search_start_frame"],
                TOOL.MODEL_LEAD_IN_FRAMES)
            self.assertEqual(pair["model"]["analysis"]["onset_frame"],
                             fixture["model_onset"])
            self.assertGreater(pair["model"]["analysis"]["onset_frame"],
                               TOOL.MODEL_LEAD_IN_FRAMES)
            selected_probe = fixture["model_manifest"]["probes"][0]
            self.assertEqual(pair["model"]["analysis"]["absolute_peak"],
                             selected_probe["raw_peak"])

            reference_baseline = 0.5 * math.sqrt(393.0 / 400.0)
            model_baseline = 0.5 * math.sqrt(2167.0 / 2205.0)
            expected_rms = {
                "reference": [0.0] + [
                    20.0 * math.log10(amplitude / reference_baseline)
                    for amplitude in (0.25, 0.125, 0.0625, 0.03125)],
                "model": [0.0] + [
                    20.0 * math.log10(amplitude / model_baseline)
                    for amplitude in (0.25, 0.125, 0.0625, 0.03125)],
            }
            for side, expected_values in expected_rms.items():
                actual = pair[side]["analysis"]["rms_relative_db"]
                for measured, expected in zip(actual, expected_values):
                    self.assertAlmostEqual(measured, expected, places=12)
            differences = pair[
                    "descriptive_difference_model_minus_reference"][
                        "rms_relative_db"]
            for difference, model, reference in zip(
                    differences, expected_rms["model"], expected_rms["reference"]):
                self.assertAlmostEqual(difference, model - reference, places=12)

    def test_cli_failures_are_closed_and_quiet(self):
        def stale_hash(fixture):
            fixture["plan"]["pairs"][0]["model_audio_sha256"] = "1" * 64

        def invalid_channel(fixture):
            fixture["plan"]["pairs"][0]["reference_channel_index"] = 2

        def invalid_probe(fixture):
            fixture["plan"]["pairs"][0]["model_probe_id"] = "missing-probe"

        def silence(fixture):
            fixture["plan"]["pairs"][0]["reference_channel_index"] = 0

        def nonfinite(fixture):
            samples = list(fixture["model_samples"])
            samples[10] = math.nan
            write_wave(
                fixture["model_audio"], "float32", TOOL.MODEL_SAMPLE_RATE,
                [samples])
            fixture["plan"]["pairs"][0]["model_audio_sha256"] = digest(
                fixture["model_audio"])

        def too_short(fixture):
            fixture["plan"]["pairs"][0]["reference_crop_frames"] = 1000

        def insufficient_pre_roll(fixture):
            fixture["plan"]["pairs"][0][
                "reference_onset_search_start_frame"] = 399

        def pseudo_v3_protocol(fixture):
            fixture["model_manifest"]["protocol"]["lead_in_frames"] -= 1
            write_json(fixture["model_manifest_path"], fixture["model_manifest"])
            fixture["plan"]["model_manifest_sha256"] = digest(
                fixture["model_manifest_path"])

        def nominal_note_mismatch(fixture):
            fixture["plan"]["pairs"][0][
                "reference_nominal_frequency_hz"] = 82.40688923

        cases = (
            ("stale hash", stale_hash, "model audio SHA-256 mismatch"),
            ("invalid channel", invalid_channel, "outside 2 channels"),
            ("invalid probe", invalid_probe, "must resolve exactly once"),
            ("silence", silence, "silent in the onset search"),
            ("nonfinite", nonfinite, "contains non-finite audio"),
            ("too short", too_short, "less than 2000 ms"),
            ("insufficient pre-roll", insufficient_pre_roll,
             "needs 50 ms of pre-onset noise"),
            ("pseudo-v3 protocol", pseudo_v3_protocol,
             "protocol differs from the frozen v3 contract"),
            ("nominal note mismatch", nominal_note_mismatch,
             "reference and model nominal notes differ"),
        )
        for name, mutate, message in cases:
            with self.subTest(case=name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                fixture = build_fixture(root)
                mutate(fixture)
                write_json(fixture["plan_path"], fixture["plan"])
                output = root / "should-not-exist.json"
                run = run_cli(fixture, output)
                self.assertEqual(run.returncode, 1)
                self.assertFalse(output.exists())
                self.assertIn(b"invalid:", run.stderr)
                self.assertIn(message.encode("utf-8"), run.stderr)
                self.assertNotIn(b"Traceback", run.stderr)
                self.assertNotIn(b"Traceback", run.stdout)

        with self.assertRaisesRegex(TOOL.Invalid, "nominal frequency reaches Nyquist"):
            TOOL.analyze_signal(
                [0.0] * 16800, 8000, 4000.0, 400, "Nyquist fixture")

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixture = build_fixture(root)
            unconsumed = fixture["model_paths"]["e2-dead"]
            original_bytes = unconsumed.read_bytes()
            output = root / "hard-linked-output.json"
            output.hardlink_to(unconsumed)
            run = run_cli(fixture, output)
            self.assertEqual(run.returncode, 1)
            self.assertIn(b"output path aliases a comparison input", run.stderr)
            self.assertNotIn(b"Traceback", run.stderr)
            self.assertNotIn(b"Traceback", run.stdout)
            self.assertEqual(unconsumed.read_bytes(), original_bytes)
            self.assertEqual(output.read_bytes(), original_bytes)


if __name__ == "__main__":
    unittest.main()
