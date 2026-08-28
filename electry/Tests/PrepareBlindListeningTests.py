#!/usr/bin/env python3

import hashlib
import importlib.util
import io
import json
import stat
import struct
import sys
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
from contextlib import redirect_stderr
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
SPEC = importlib.util.spec_from_file_location(
    "prepare_blind_listening", ROOT / "Tools" / "PrepareBlindListening.py")
TOOL = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(TOOL)
SERVER_SPEC = importlib.util.spec_from_file_location(
    "serve_blind_listening", ROOT / "Tools" / "ServeBlindListening.py")
SERVER = importlib.util.module_from_spec(SERVER_SPEC)
assert SERVER_SPEC.loader is not None
SERVER_SPEC.loader.exec_module(SERVER)
sys.path.insert(0, str(ROOT / "Tests"))
from BlindListeningTestSupport import (
    build_valid_study, canonical_sha256, digest, write_pcm24)


class PrepareBlindListeningTests(unittest.TestCase):
    def test_deterministic_balanced_opaque_pack(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixture = build_valid_study(root, TOOL)
            comparison_value = fixture["comparison"]
            comparison = fixture["comparison_path"]
            manifest = fixture["manifest"]
            manifest_path = fixture["manifest_path"]
            presentation_seed = manifest["presentation_seed"]

            generated_receipt = root / "generated-selection-receipt.json"
            TOOL.generate_selection_receipt(
                fixture["selection_input_path"], generated_receipt)
            self.assertEqual(
                json.loads(generated_receipt.read_text(encoding="utf-8")),
                json.loads(fixture["selection_receipt_path"].read_text(
                    encoding="utf-8")))

            wrong_practice_input = json.loads(
                fixture["selection_input_path"].read_text(encoding="utf-8"))
            wrong_practice_input["practice"][0]["selection_kind"] = "run"
            wrong_practice_input_path = root / "wrong-practice-selection-input.json"
            wrong_practice_input_path.write_text(
                json.dumps(wrong_practice_input), encoding="utf-8")
            with self.assertRaisesRegex(
                    TOOL.StudyError, r"selection_kind must be 'slot'"):
                TOOL.generate_selection_receipt(
                    wrong_practice_input_path, root / "wrong-practice-receipt.json")

            wrong_rapid_input = json.loads(
                fixture["selection_input_path"].read_text(encoding="utf-8"))
            rapid_take = next(
                take for take in wrong_rapid_input["sessions"][0]["takes"]
                if take["file"] == "e1-palm-middle-rapid.wav")
            rapid_take["run_bpms_in_order"] = [120, 120, 240]
            wrong_rapid_input_path = root / "wrong-rapid-selection-input.json"
            wrong_rapid_input_path.write_text(
                json.dumps(wrong_rapid_input), encoding="utf-8")
            with self.assertRaisesRegex(
                    TOOL.StudyError, "120/180/240 permutation"):
                TOOL.generate_selection_receipt(
                    wrong_rapid_input_path, root / "wrong-rapid-receipt.json")

            wrong_practice_comparison_value = json.loads(
                json.dumps(comparison_value))
            wrong_practice_comparison_value["practice"][0]["provenance"][
                "selection_unit"] = {
                    "kind": "run", "index": 1, "stroke": "down_first"}
            wrong_practice_comparison = root / "wrong-practice-comparison.json"
            wrong_practice_comparison.write_text(
                json.dumps(wrong_practice_comparison_value), encoding="utf-8")
            wrong_practice_manifest = json.loads(json.dumps(manifest))
            wrong_practice_manifest["comparison_manifest"]["path"] = (
                wrong_practice_comparison.name)
            wrong_practice_manifest["comparison_manifest"]["sha256"] = (
                hashlib.sha256(wrong_practice_comparison.read_bytes()).hexdigest())
            wrong_practice_study = root / "wrong-practice-study.json"
            wrong_practice_study.write_text(
                json.dumps(wrong_practice_manifest), encoding="utf-8")
            with self.assertRaisesRegex(
                    TOOL.StudyError,
                    r"practice\[0\].provenance.selection_unit.kind must be slot"):
                TOOL._validate_study(wrong_practice_study)

            weak_seed = json.loads(json.dumps(manifest))
            weak_seed["presentation_seed"] = "11" * 32
            weak_seed_path = root / "weak-seed-study.json"
            weak_seed_path.write_text(json.dumps(weak_seed), encoding="utf-8")
            with self.assertRaisesRegex(TOOL.StudyError, "secrets.token_hex"):
                TOOL._validate_study(weak_seed_path)

            placeholder_id = json.loads(json.dumps(manifest))
            placeholder_id["study_id"] = "REPLACE-STUDY-ID"
            placeholder_id_path = root / "placeholder-id-study.json"
            placeholder_id_path.write_text(
                json.dumps(placeholder_id), encoding="utf-8")
            with self.assertRaisesRegex(TOOL.StudyError, "study_id"):
                TOOL._validate_study(placeholder_id_path)

            boolean_cell_id = json.loads(json.dumps(manifest))
            boolean_cell_id["cells"][0]["id"] = True
            boolean_cell_id_path = root / "boolean-cell-id-study.json"
            boolean_cell_id_path.write_text(
                json.dumps(boolean_cell_id), encoding="utf-8")
            with self.assertRaisesRegex(TOOL.StudyError, r"cells\[0\]\.id"):
                TOOL._validate_study(boolean_cell_id_path)

            output_a = root / "pack-a"
            output_b = root / "pack-b"
            fingerprint_a = TOOL.prepare(manifest_path, output_a)
            fingerprint_b = TOOL.prepare(manifest_path, output_b)
            self.assertEqual(fingerprint_a, fingerprint_b)

            short_manifest = json.loads(json.dumps(manifest))
            for source, value in (("physical", 2500), ("electry", 2550)):
                short = root / f"short-{source}.wav"
                short_manifest["cells"][0][source] = {
                    "path": short.name,
                    "sha256": write_pcm24(short, value, 32),
                }
            short_path = root / "short-study.json"
            short_path.write_text(json.dumps(short_manifest), encoding="utf-8")
            with self.assertRaisesRegex(TOOL.StudyError, "exactly 30870 frames"):
                TOOL._validate_study(short_path)

            bad_pad = root / "bad-pad.wav"
            write_pcm24(bad_pad, 100, 1)
            bad_pad.write_bytes(bad_pad.read_bytes()[:-1] + b"\1")
            with self.assertRaisesRegex(TOOL.StudyError, "zero RIFF pad byte"):
                TOOL._inspect_pcm24_wav(bad_pad)

            extended = root / "extended-fmt.wav"
            format_data = struct.pack(
                "<HHIIHHH", 1, 1, 44100, 132300, 3, 24, 0)
            payload = (100).to_bytes(3, "little", signed=True)
            body = (b"WAVE" + b"fmt " + struct.pack("<I", len(format_data))
                    + format_data + b"data" + struct.pack("<I", len(payload))
                    + payload + b"\0")
            extended.write_bytes(b"RIFF" + struct.pack("<I", len(body)) + body)
            with self.assertRaisesRegex(TOOL.StudyError, "canonical 16-byte"):
                TOOL._inspect_pcm24_wav(extended)

            stub_comparison = root / "stub-comparison.json"
            stub_comparison.write_text(
                '{"schema":"electry-blind-comparison/v1","status":"frozen"}\n',
                encoding="utf-8")
            stub_manifest = json.loads(json.dumps(manifest))
            stub_manifest["comparison_manifest"]["path"] = stub_comparison.name
            stub_manifest["comparison_manifest"]["sha256"] = hashlib.sha256(
                stub_comparison.read_bytes()).hexdigest()
            stub_path = root / "stub-study.json"
            stub_path.write_text(json.dumps(stub_manifest), encoding="utf-8")
            with self.assertRaisesRegex(TOOL.StudyError,
                                        "comparison manifest keys differ"):
                TOOL._validate_study(stub_path)

            draft_comparison = root / "draft-comparison.json"
            draft_value = json.loads(json.dumps(comparison_value))
            draft_value["status"] = "draft"
            draft_comparison.write_text(json.dumps(draft_value), encoding="utf-8")
            draft_manifest = json.loads(json.dumps(manifest))
            draft_manifest["comparison_manifest"]["path"] = draft_comparison.name
            draft_manifest["comparison_manifest"]["sha256"] = hashlib.sha256(
                draft_comparison.read_bytes()).hexdigest()
            draft_path = root / "draft-study.json"
            draft_path.write_text(json.dumps(draft_manifest), encoding="utf-8")
            with self.assertRaisesRegex(TOOL.StudyError,
                                        "must declare electry-blind-comparison/v1 and frozen"):
                TOOL._validate_study(draft_path)

            mismatched_comparison = root / "mismatched-comparison.json"
            mismatched_value = json.loads(json.dumps(comparison_value))
            mismatched_value["cells"][0]["physical_sha256"] = digest("wrong-cell")
            mismatched_comparison.write_text(
                json.dumps(mismatched_value), encoding="utf-8")
            mismatched_manifest = json.loads(json.dumps(manifest))
            mismatched_manifest["comparison_manifest"]["path"] = (
                mismatched_comparison.name)
            mismatched_manifest["comparison_manifest"]["sha256"] = hashlib.sha256(
                mismatched_comparison.read_bytes()).hexdigest()
            mismatched_path = root / "mismatched-study.json"
            mismatched_path.write_text(json.dumps(mismatched_manifest), encoding="utf-8")
            with self.assertRaisesRegex(TOOL.StudyError,
                                        "cell 1 physical SHA-256 does not match"):
                TOOL._validate_study(mismatched_path)

            unhashed_settings = root / "unhashed-settings-comparison.json"
            unhashed_value = json.loads(json.dumps(comparison_value))
            unhashed_value["freezes"]["render"]["settings"]["sample_rate_hz"] = 48000
            unhashed_settings.write_text(json.dumps(unhashed_value), encoding="utf-8")
            unhashed_manifest = json.loads(json.dumps(manifest))
            unhashed_manifest["comparison_manifest"]["path"] = unhashed_settings.name
            unhashed_manifest["comparison_manifest"]["sha256"] = hashlib.sha256(
                unhashed_settings.read_bytes()).hexdigest()
            unhashed_path = root / "unhashed-settings-study.json"
            unhashed_path.write_text(json.dumps(unhashed_manifest), encoding="utf-8")
            with self.assertRaisesRegex(TOOL.StudyError,
                                        "settings_sha256 does not match"):
                TOOL._validate_study(unhashed_path)

            contradictory_settings = root / "contradictory-settings-comparison.json"
            contradictory_value = json.loads(json.dumps(comparison_value))
            contradictory_render = contradictory_value["freezes"]["render"]
            contradictory_render["settings"]["sample_rate_hz"] = 48000
            contradictory_render["settings_sha256"] = canonical_sha256(
                contradictory_render["settings"])
            contradictory_settings.write_text(
                json.dumps(contradictory_value), encoding="utf-8")
            contradictory_manifest = json.loads(json.dumps(manifest))
            contradictory_manifest["comparison_manifest"]["path"] = (
                contradictory_settings.name)
            contradictory_manifest["comparison_manifest"]["sha256"] = hashlib.sha256(
                contradictory_settings.read_bytes()).hexdigest()
            contradictory_path = root / "contradictory-settings-study.json"
            contradictory_path.write_text(
                json.dumps(contradictory_manifest), encoding="utf-8")
            with self.assertRaisesRegex(TOOL.StudyError,
                                        "sample_rate_hz must be 44100"):
                TOOL._validate_study(contradictory_path)

            wrong_oversampling = root / "wrong-oversampling-comparison.json"
            wrong_oversampling_value = json.loads(json.dumps(comparison_value))
            wrong_chain = wrong_oversampling_value["freezes"]["chain"]
            wrong_chain["settings"]["oversampling"] = 2
            wrong_chain["settings_sha256"] = canonical_sha256(wrong_chain["settings"])
            wrong_oversampling.write_text(
                json.dumps(wrong_oversampling_value), encoding="utf-8")
            wrong_oversampling_manifest = json.loads(json.dumps(manifest))
            wrong_oversampling_manifest["comparison_manifest"]["path"] = (
                wrong_oversampling.name)
            wrong_oversampling_manifest["comparison_manifest"]["sha256"] = (
                hashlib.sha256(wrong_oversampling.read_bytes()).hexdigest())
            wrong_oversampling_path = root / "wrong-oversampling-study.json"
            wrong_oversampling_path.write_text(
                json.dumps(wrong_oversampling_manifest), encoding="utf-8")
            with self.assertRaisesRegex(TOOL.StudyError,
                                        "oversampling must be 8"):
                TOOL._validate_study(wrong_oversampling_path)

            wrong_gate = root / "wrong-gate-comparison.json"
            wrong_gate_value = json.loads(json.dumps(comparison_value))
            wrong_analysis = wrong_gate_value["freezes"]["analysis"]
            wrong_analysis["settings"]["gate_thresholds"][
                "repeat_same_source_agreement_minimum"] = 0.5
            wrong_analysis["settings_sha256"] = canonical_sha256(
                wrong_analysis["settings"])
            wrong_gate.write_text(json.dumps(wrong_gate_value), encoding="utf-8")
            wrong_gate_manifest = json.loads(json.dumps(manifest))
            wrong_gate_manifest["comparison_manifest"]["path"] = wrong_gate.name
            wrong_gate_manifest["comparison_manifest"]["sha256"] = hashlib.sha256(
                wrong_gate.read_bytes()).hexdigest()
            wrong_gate_path = root / "wrong-gate-study.json"
            wrong_gate_path.write_text(
                json.dumps(wrong_gate_manifest), encoding="utf-8")
            with self.assertRaisesRegex(
                    TOOL.StudyError,
                    "repeat_same_source_agreement_minimum must be 0.7"):
                TOOL._validate_study(wrong_gate_path)

            rapid_repeatability = root / "rapid-repeatability-comparison.json"
            rapid_repeatability_value = json.loads(json.dumps(comparison_value))
            rapid_margin = next(
                endpoint["no_regression_margin"]
                for endpoint in rapid_repeatability_value["engineering_freeze"][
                    "endpoints"]
                if endpoint["id"] == "rapid_envelope_shape_error")
            rapid_margin["train_repeatability_p90"] = 0.02
            rapid_repeatability.write_text(
                json.dumps(rapid_repeatability_value), encoding="utf-8")
            rapid_repeatability_manifest = json.loads(json.dumps(manifest))
            rapid_repeatability_manifest["comparison_manifest"]["path"] = (
                rapid_repeatability.name)
            rapid_repeatability_manifest["comparison_manifest"]["sha256"] = (
                hashlib.sha256(rapid_repeatability.read_bytes()).hexdigest())
            rapid_repeatability_path = root / "rapid-repeatability-study.json"
            rapid_repeatability_path.write_text(
                json.dumps(rapid_repeatability_manifest), encoding="utf-8")
            with self.assertRaisesRegex(
                    TOOL.StudyError, "train_repeatability_p90 must be 0.0"):
                TOOL._validate_study(rapid_repeatability_path)

            wrong_margin_method = root / "wrong-margin-method-comparison.json"
            wrong_margin_method_value = json.loads(json.dumps(comparison_value))
            groove_margin = next(
                endpoint["no_regression_margin"]
                for endpoint in wrong_margin_method_value["engineering_freeze"][
                    "endpoints"]
                if endpoint["id"] == "dead_groove_inter_hit_residual_error")
            groove_margin["method"] = (
                "balanced_three_versus_three_isolated_repetitions")
            wrong_margin_method.write_text(
                json.dumps(wrong_margin_method_value), encoding="utf-8")
            wrong_margin_method_manifest = json.loads(json.dumps(manifest))
            wrong_margin_method_manifest["comparison_manifest"]["path"] = (
                wrong_margin_method.name)
            wrong_margin_method_manifest["comparison_manifest"]["sha256"] = (
                hashlib.sha256(wrong_margin_method.read_bytes()).hexdigest())
            wrong_margin_method_path = root / "wrong-margin-method-study.json"
            wrong_margin_method_path.write_text(
                json.dumps(wrong_margin_method_manifest), encoding="utf-8")
            with self.assertRaisesRegex(
                    TOOL.StudyError,
                    "method must be 'complete_groove_run_resampling'"):
                TOOL._validate_study(wrong_margin_method_path)

            coupled_seed = root / "coupled-seed-comparison.json"
            coupled_value = json.loads(json.dumps(comparison_value))
            coupled_value["freezes"]["selection"]["seed"] = presentation_seed
            coupled_seed.write_text(json.dumps(coupled_value), encoding="utf-8")
            coupled_manifest = json.loads(json.dumps(manifest))
            coupled_manifest["comparison_manifest"]["path"] = coupled_seed.name
            coupled_manifest["comparison_manifest"]["sha256"] = hashlib.sha256(
                coupled_seed.read_bytes()).hexdigest()
            coupled_path = root / "coupled-seed-study.json"
            coupled_path.write_text(json.dumps(coupled_manifest), encoding="utf-8")
            with self.assertRaisesRegex(TOOL.StudyError,
                                        "independently generated"):
                TOOL._validate_study(coupled_path)

            wrong_tempo = root / "wrong-tempo-comparison.json"
            wrong_tempo_value = json.loads(json.dumps(comparison_value))
            wrong_tempo_value["cells"][4]["provenance"]["selection_unit"] = {
                "kind": "run", "index": 1, "stroke": "down_first"}
            wrong_tempo.write_text(json.dumps(wrong_tempo_value), encoding="utf-8")
            wrong_tempo_manifest = json.loads(json.dumps(manifest))
            wrong_tempo_manifest["comparison_manifest"]["path"] = wrong_tempo.name
            wrong_tempo_manifest["comparison_manifest"]["sha256"] = hashlib.sha256(
                wrong_tempo.read_bytes()).hexdigest()
            wrong_tempo_path = root / "wrong-tempo-study.json"
            wrong_tempo_path.write_text(
                json.dumps(wrong_tempo_manifest), encoding="utf-8")
            with self.assertRaisesRegex(TOOL.StudyError, "required 180-BPM run"):
                TOOL._validate_study(wrong_tempo_path)

            wrong_source = root / "wrong-source-comparison.json"
            wrong_source_value = json.loads(json.dumps(comparison_value))
            wrong_source_value["cells"][0]["source_session_id"] = "train-a"
            wrong_source.write_text(json.dumps(wrong_source_value), encoding="utf-8")
            wrong_source_manifest = json.loads(json.dumps(manifest))
            wrong_source_manifest["comparison_manifest"]["path"] = wrong_source.name
            wrong_source_manifest["comparison_manifest"]["sha256"] = hashlib.sha256(
                wrong_source.read_bytes()).hexdigest()
            wrong_source_path = root / "wrong-source-study.json"
            wrong_source_path.write_text(
                json.dumps(wrong_source_manifest), encoding="utf-8")
            with self.assertRaisesRegex(TOOL.StudyError,
                                        "source session/cluster is not in the holdout cohort"):
                TOOL._validate_study(wrong_source_path)

            too_few_train = root / "too-few-train-comparison.json"
            too_few_train_value = json.loads(json.dumps(comparison_value))
            too_few_train_value["source_cohort"]["engineering_train_clusters"].pop()
            too_few_train.write_text(
                json.dumps(too_few_train_value), encoding="utf-8")
            too_few_train_manifest = json.loads(json.dumps(manifest))
            too_few_train_manifest["comparison_manifest"]["path"] = too_few_train.name
            too_few_train_manifest["comparison_manifest"]["sha256"] = hashlib.sha256(
                too_few_train.read_bytes()).hexdigest()
            too_few_train_path = root / "too-few-train-study.json"
            too_few_train_path.write_text(
                json.dumps(too_few_train_manifest), encoding="utf-8")
            with self.assertRaisesRegex(
                    TOOL.StudyError, "at least three engineering train clusters"):
                TOOL._validate_study(too_few_train_path)

            unseeded_choice = root / "unseeded-choice-comparison.json"
            unseeded_choice_value = json.loads(json.dumps(comparison_value))
            unit = unseeded_choice_value["cells"][0]["provenance"]["selection_unit"]
            unit["index"] = ((unit["index"] - 1 + 2) % 12) + 1
            unseeded_choice.write_text(
                json.dumps(unseeded_choice_value), encoding="utf-8")
            unseeded_choice_manifest = json.loads(json.dumps(manifest))
            unseeded_choice_manifest["comparison_manifest"]["path"] = (
                unseeded_choice.name)
            unseeded_choice_manifest["comparison_manifest"]["sha256"] = hashlib.sha256(
                unseeded_choice.read_bytes()).hexdigest()
            unseeded_choice_path = root / "unseeded-choice-study.json"
            unseeded_choice_path.write_text(
                json.dumps(unseeded_choice_manifest), encoding="utf-8")
            with self.assertRaisesRegex(TOOL.StudyError, "not the first seeded rank"):
                TOOL._validate_study(unseeded_choice_path)

            bad_partition_value = json.loads(json.dumps(comparison_value))
            receipt_path = root / bad_partition_value["engineering_freeze"][
                "derivation_receipt"]["path"]
            receipt_value = json.loads(receipt_path.read_text(encoding="utf-8"))
            result_path = root / receipt_value["derivation_result"]["path"]
            result_value = json.loads(result_path.read_text(encoding="utf-8"))
            raw_descriptor = result_value["train_clusters"][0]["analysis_result"]
            raw_path = root / raw_descriptor["path"]
            raw_value = json.loads(raw_path.read_text(encoding="utf-8"))
            samples = raw_value["endpoints"][0]["repeatability_samples"]
            samples[1]["half_a_repetition_ids"] = samples[0][
                "half_a_repetition_ids"]
            samples[1]["half_b_repetition_ids"] = samples[0][
                "half_b_repetition_ids"]
            bad_raw_path = root / "bad-partition-analysis.json"
            bad_raw_path.write_text(json.dumps(raw_value), encoding="utf-8")
            raw_descriptor["path"] = bad_raw_path.name
            raw_descriptor["sha256"] = hashlib.sha256(
                bad_raw_path.read_bytes()).hexdigest()
            bad_result_path = root / "bad-partition-result.json"
            bad_result_path.write_text(json.dumps(result_value), encoding="utf-8")
            receipt_value["derivation_result"]["path"] = bad_result_path.name
            receipt_value["derivation_result"]["sha256"] = hashlib.sha256(
                bad_result_path.read_bytes()).hexdigest()
            bad_receipt_path = root / "bad-partition-receipt.json"
            bad_receipt_path.write_text(json.dumps(receipt_value), encoding="utf-8")
            bad_partition_value["engineering_freeze"]["derivation_receipt"][
                "path"] = bad_receipt_path.name
            bad_partition_value["engineering_freeze"]["derivation_receipt"][
                "sha256"] = hashlib.sha256(bad_receipt_path.read_bytes()).hexdigest()
            bad_partition = root / "bad-partition-comparison.json"
            bad_partition.write_text(json.dumps(bad_partition_value), encoding="utf-8")
            bad_partition_manifest = json.loads(json.dumps(manifest))
            bad_partition_manifest["comparison_manifest"]["path"] = bad_partition.name
            bad_partition_manifest["comparison_manifest"]["sha256"] = hashlib.sha256(
                bad_partition.read_bytes()).hexdigest()
            bad_partition_path = root / "bad-partition-study.json"
            bad_partition_path.write_text(
                json.dumps(bad_partition_manifest), encoding="utf-8")
            with self.assertRaisesRegex(
                    TOOL.StudyError, "half_a_repetition_ids.*must be"):
                TOOL._validate_study(bad_partition_path)

            omitted_input_value = json.loads(json.dumps(comparison_value))
            omitted_receipt_path = root / omitted_input_value[
                "engineering_freeze"]["derivation_receipt"]["path"]
            omitted_receipt_value = json.loads(
                omitted_receipt_path.read_text(encoding="utf-8"))
            omitted_result_path = root / omitted_receipt_value[
                "derivation_result"]["path"]
            omitted_result_value = json.loads(
                omitted_result_path.read_text(encoding="utf-8"))
            omitted_raw_descriptor = omitted_result_value[
                "train_clusters"][0]["analysis_result"]
            omitted_raw_path = root / omitted_raw_descriptor["path"]
            omitted_raw_value = json.loads(
                omitted_raw_path.read_text(encoding="utf-8"))
            omitted_raw_value["endpoints"][0]["eligible_input_units"][0][
                "input_units"].pop()
            bad_coverage_raw_path = root / "omitted-input-analysis.json"
            bad_coverage_raw_path.write_text(
                json.dumps(omitted_raw_value), encoding="utf-8")
            omitted_raw_descriptor["path"] = bad_coverage_raw_path.name
            omitted_raw_descriptor["sha256"] = hashlib.sha256(
                bad_coverage_raw_path.read_bytes()).hexdigest()
            bad_coverage_result_path = root / "omitted-input-result.json"
            bad_coverage_result_path.write_text(
                json.dumps(omitted_result_value), encoding="utf-8")
            omitted_receipt_value["derivation_result"]["path"] = (
                bad_coverage_result_path.name)
            omitted_receipt_value["derivation_result"]["sha256"] = hashlib.sha256(
                bad_coverage_result_path.read_bytes()).hexdigest()
            bad_coverage_receipt_path = root / "omitted-input-receipt.json"
            bad_coverage_receipt_path.write_text(
                json.dumps(omitted_receipt_value), encoding="utf-8")
            omitted_input_value["engineering_freeze"]["derivation_receipt"][
                "path"] = bad_coverage_receipt_path.name
            omitted_input_value["engineering_freeze"]["derivation_receipt"][
                "sha256"] = hashlib.sha256(
                    bad_coverage_receipt_path.read_bytes()).hexdigest()
            bad_coverage_comparison = root / "omitted-input-comparison.json"
            bad_coverage_comparison.write_text(
                json.dumps(omitted_input_value), encoding="utf-8")
            bad_coverage_manifest = json.loads(json.dumps(manifest))
            bad_coverage_manifest["comparison_manifest"]["path"] = (
                bad_coverage_comparison.name)
            bad_coverage_manifest["comparison_manifest"]["sha256"] = hashlib.sha256(
                bad_coverage_comparison.read_bytes()).hexdigest()
            bad_coverage_study = root / "omitted-input-study.json"
            bad_coverage_study.write_text(
                json.dumps(bad_coverage_manifest), encoding="utf-8")
            with self.assertRaisesRegex(
                    TOOL.StudyError, "eligible_input_units.*must be"):
                TOOL._validate_study(bad_coverage_study)

            wrong_scorer = root / "wrong-scorer-comparison.json"
            wrong_scorer_value = json.loads(json.dumps(comparison_value))
            original_scorer = wrong_scorer_value["freezes"]["analysis"][
                "listener_scorer_sha256"]
            wrong_scorer_value["freezes"]["analysis"]["listener_scorer_sha256"] = (
                digest("wrong-scorer"))
            registry_path = root / comparison_value["artifact_registry"]["path"]
            wrong_registry_value = json.loads(registry_path.read_text(encoding="utf-8"))
            for artifact in wrong_registry_value["artifacts"]:
                if artifact["sha256"] == original_scorer:
                    artifact["sha256"] = digest("wrong-scorer")
                    break
            wrong_registry = root / "wrong-scorer-registry.json"
            wrong_registry.write_text(json.dumps(wrong_registry_value), encoding="utf-8")
            wrong_scorer_value["artifact_registry"]["path"] = wrong_registry.name
            wrong_scorer_value["artifact_registry"]["sha256"] = hashlib.sha256(
                wrong_registry.read_bytes()).hexdigest()
            wrong_scorer.write_text(json.dumps(wrong_scorer_value), encoding="utf-8")
            wrong_scorer_manifest = json.loads(json.dumps(manifest))
            wrong_scorer_manifest["comparison_manifest"]["path"] = wrong_scorer.name
            wrong_scorer_manifest["comparison_manifest"]["sha256"] = hashlib.sha256(
                wrong_scorer.read_bytes()).hexdigest()
            wrong_scorer_path = root / "wrong-scorer-study.json"
            wrong_scorer_path.write_text(
                json.dumps(wrong_scorer_manifest), encoding="utf-8")
            with self.assertRaisesRegex(TOOL.StudyError,
                                        "does not match ScoreBlindListening.py"):
                TOOL.prepare(wrong_scorer_path, root / "wrong-scorer-pack")

            key_a = output_a / "private" / "answer-key.json"
            key_b = output_b / "private" / "answer-key.json"
            self.assertEqual(key_a.read_bytes(), key_b.read_bytes())
            key = json.loads(key_a.read_text(encoding="utf-8"))
            self.assertEqual(len(key["participants"]), 30)
            comparison_archive = output_a / "private" / "comparison-manifest.json"
            self.assertEqual(comparison_archive.read_bytes(), comparison.read_bytes())
            self.assertEqual(stat.S_IMODE(comparison_archive.stat().st_mode), 0o600)
            self.assertEqual(key["comparison_manifest"], {
                "path": "private/comparison-manifest.json",
                "schema": "electry-blind-comparison/v1",
                "sha256": hashlib.sha256(comparison.read_bytes()).hexdigest(),
                "status": "frozen",
            })
            self.assertEqual(key["source_manifest"]["path"],
                             "private/study-manifest.json")
            self.assertEqual((output_a / "private" / "study-manifest.json").read_bytes(),
                             manifest_path.read_bytes())
            self.assertEqual((output_a / "private" / "prepare.py").read_bytes(),
                             (ROOT / "Tools" / "PrepareBlindListening.py").read_bytes())
            self.assertEqual(stat.S_IMODE(
                (output_a / "private" / "prepare.py").stat().st_mode), 0o600)
            self.assertEqual(len(key["archives"]["capture_manifests"]), 5)
            self.assertEqual(len(key["archives"]["event_records"]), 9)
            self.assertEqual(len(key["archives"]["engineering_train_analysis"]), 3)
            for section in ("capture_manifests", "event_records"):
                for archived in key["archives"][section]:
                    archived_path = output_a / archived["path"]
                    self.assertEqual(hashlib.sha256(archived_path.read_bytes()).hexdigest(),
                                     archived["sha256"])
                    self.assertEqual(stat.S_IMODE(archived_path.stat().st_mode), 0o600)
            for section in ("engineering_derivation_receipt",
                            "engineering_derivation_result"):
                archived = key["archives"][section]
                archived_path = output_a / archived["path"]
                self.assertEqual(hashlib.sha256(archived_path.read_bytes()).hexdigest(),
                                 archived["sha256"])
                self.assertEqual(stat.S_IMODE(archived_path.stat().st_mode), 0o600)
            for archived in key["archives"]["engineering_train_analysis"]:
                archived_path = output_a / archived["path"]
                self.assertEqual(hashlib.sha256(archived_path.read_bytes()).hexdigest(),
                                 archived["sha256"])
                self.assertEqual(stat.S_IMODE(archived_path.stat().st_mode), 0o600)
            links_path = output_a / "private" / "participant-links.tsv"
            self.assertEqual(key["participant_links"], {
                "path": "private/participant-links.tsv",
                "sha256": hashlib.sha256(links_path.read_bytes()).hexdigest(),
            })
            self.assertEqual(key["study_fingerprint"], fingerprint_a)

            balance = {
                pair_id: {"extended_range_guitarist": 0, "metal_producer": 0}
                for pair_id in ["practice-1", "practice-2", *range(1, 11)]
            }
            public_audio_paths = set()
            for participant in key["participants"]:
                trials = participant["trials"]
                self.assertEqual(len(trials), 13)
                originals = {trial["cell"]: trial for trial in trials
                             if trial["repeat_of"] is None}
                repeats = {trial["repeat_of"]: trial for trial in trials
                           if trial["repeat_of"] is not None}
                for cell in (5, 7, 9):
                    self.assertGreaterEqual(
                        repeats[cell]["position"] - originals[cell]["position"], 4)
                    self.assertNotEqual(repeats[cell]["a_source"],
                                        originals[cell]["a_source"])
                for practice in participant["practice"]:
                    balance[practice["pair"]][participant["listener_stratum"]] += (
                        practice["a_source"] == "physical")
                for cell, trial in originals.items():
                    balance[cell][participant["listener_stratum"]] += (
                        trial["a_source"] == "physical")

                public_path = (output_a / "public" / "sessions"
                               / f"{participant['session_token']}.json")
                second_public_path = (output_b / "public" / "sessions"
                                      / f"{participant['session_token']}.json")
                self.assertEqual(public_path.read_bytes(),
                                 second_public_path.read_bytes())
                public_value = json.loads(public_path.read_text(encoding="utf-8"))
                self.assertEqual(public_value["study_fingerprint"],
                                 key["study_fingerprint"])
                self.assertEqual(public_value["mapping_commitment"],
                                 participant["mapping_commitment"])
                public_text = json.dumps(public_value)
                self.assertNotIn("participant_id", public_text)
                self.assertNotIn(participant["participant_id"], public_text)
                self.assertNotIn("a_source", public_text)
                self.assertNotIn("physical", public_text)
                for record in public_value["practice"] + public_value["trials"]:
                    for side in ("a", "b"):
                        self.assertNotIn(record[side], public_audio_paths)
                        public_audio_paths.add(record[side])
                        self.assertTrue((output_a / "public" / record[side]).is_file())

            for pair_id, counts in balance.items():
                ordinal = (int(pair_id[-1]) if isinstance(pair_id, str) else pair_id)
                expected_guitarists = 8 if ordinal % 2 else 7
                self.assertEqual(counts["extended_range_guitarist"],
                                 expected_guitarists)
                self.assertEqual(counts["metal_producer"],
                                 15 - expected_guitarists)
            self.assertEqual(len(public_audio_paths), 30 * 15 * 2)
            self.assertTrue((output_a / "public" / "index.html").is_file())
            runner = (output_a / "public" / "index.html").read_text(
                encoding="utf-8")
            self.assertIn("playPending", runner)
            self.assertIn("client-authored", runner)
            self.assertTrue((output_a / "score.py").is_file())
            self.assertTrue((output_a / "serve.py").is_file())
            self.assertTrue((output_a / "private" / "participant-links.tsv").is_file())
            self.assertEqual(
                (output_a / "public" / "README.txt").read_text(encoding="utf-8"),
                "Run `python3 serve.py --expected-fingerprint "
                "<externally-recorded-64-hex-fingerprint>` from the pack root; "
                "do not use a directory-listing server.\n"
                "Open only the opaque URL assigned by the coordinator.\n"
                "Never expose the private directory or participant-links.tsv.\n")

            server = SERVER.make_server(
                output_a, "127.0.0.1", 0, fingerprint_a)
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            try:
                base = f"http://127.0.0.1:{server.server_port}"
                with redirect_stderr(io.StringIO()):
                    with urllib.request.urlopen(base + "/", timeout=2) as reply:
                        self.assertEqual(reply.status, 200)
                    with self.assertRaises(urllib.error.HTTPError) as error:
                        urllib.request.urlopen(base + "/sessions/", timeout=2)
                self.assertEqual(error.exception.code, 404)
            finally:
                server.shutdown()
                server.server_close()
                thread.join(timeout=2)

    def test_stimulus_snapshot_survives_source_replacement(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixture = build_valid_study(root, TOOL, study_id="snapshot-test")
            expected_sha256 = fixture["cells"][0]["physical"]["sha256"]
            original_validate = TOOL._validate_study

            def validate_then_replace(path):
                study = original_validate(path)
                study["cells"][1]["physical"]["path"].write_bytes(b"replaced")
                return study

            TOOL._validate_study = validate_then_replace
            try:
                output = root / "snapshot-pack"
                TOOL.prepare(fixture["manifest_path"], output)
            finally:
                TOOL._validate_study = original_validate

            key = json.loads(
                (output / "private" / "answer-key.json").read_text(encoding="utf-8"))
            participant = key["participants"][0]
            private_trial = next(
                trial for trial in participant["trials"]
                if trial["cell"] == 1 and trial["repeat_of"] is None)
            side = "a" if private_trial["a_sha256"] == expected_sha256 else "b"
            public = json.loads((
                output / "public" / "sessions"
                / f"{participant['session_token']}.json").read_text(encoding="utf-8"))
            public_trial = next(
                trial for trial in public["trials"]
                if trial["trial_id"] == private_trial["trial_id"])
            packed = output / "public" / public_trial[side]
            self.assertEqual(hashlib.sha256(packed.read_bytes()).hexdigest(),
                             expected_sha256)

    def test_repository_template_cannot_claim_a_completed_study(self):
        template_directory = (ROOT / "Docs" / "capture"
                              / "electry-mute-capture-v1")
        for template_file in template_directory.glob("*.template.json"):
            TOOL._parse_json_bytes(
                template_file.read_bytes(), f"repository template {template_file.name}")
        template = template_directory / "blind-study.template.json"
        with self.assertRaisesRegex(TOOL.StudyError,
                                    "licensed finalized stimuli are still missing"):
            TOOL._validate_study(template)


if __name__ == "__main__":
    unittest.main()
