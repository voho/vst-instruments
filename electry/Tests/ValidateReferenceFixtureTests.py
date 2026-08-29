#!/usr/bin/env python3

import copy
import hashlib
import importlib.util
import json
import os
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TOOL_PATH = ROOT / "Tools" / "ValidateReferenceFixture.py"
sys.dont_write_bytecode = True
SPEC = importlib.util.spec_from_file_location("validate_reference_fixture", TOOL_PATH)
TOOL = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(TOOL)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_pcm24(path, sample_rate=96000, channels=1, frames=64,
                extensible=False, malformed_extra=False):
    sample = (1234).to_bytes(3, "little", signed=True)
    payload = sample * channels * frames
    format_chunk = struct.pack(
        "<HHIIHH", 1, channels, sample_rate, sample_rate * channels * 3,
        channels * 3, 24)
    if extensible:
        format_chunk = struct.pack(
            "<HHIIHHH", 0xfffe, channels, sample_rate,
            sample_rate * channels * 3, channels * 3, 24, 22)
        format_chunk += struct.pack("<HI", 24, 0)
        format_chunk += bytes.fromhex("0100000000001000800000aa00389b71")
    if malformed_extra:
        format_chunk += b"\0"
    format_padding = b"\0" if len(format_chunk) & 1 else b""
    body = (b"WAVE" + b"fmt " + struct.pack("<I", len(format_chunk))
            + format_chunk + format_padding
            + b"data" + struct.pack("<I", len(payload)) + payload)
    path.write_bytes(b"RIFF" + struct.pack("<I", len(body)) + body)


class ValidateReferenceFixtureTests(unittest.TestCase):
    def make_fixture(self, root, source="cabled", rights_kind=None):
        audio_dir = root / "external"
        evidence_dir = root / "evidence"
        audio_dir.mkdir()
        evidence_dir.mkdir()

        if source == "cabled":
            dataset_id = "freesound-pack-29585"
            canonical_url = "https://freesound.org/people/cabled_mess/packs/29585/"
            snapshot = "Freesound API response archived 2026-08-29"
            asset_id = "freesound-525010"
            source_url = "https://freesound.org/people/cabled_mess/sounds/525010/"
            audio_name = "525010-original.wav"
            rights_kind = rights_kind or "per_asset_cc0_record"
            rights_basis = "CC0-1.0"
            evidence_text = "Archived per-sound API record declares CC0-1.0."
            sample_rate, channels = 96000, 1
        else:
            dataset_id = "8ridgelite"
            canonical_url = "https://github.com/JamesStubbsEng/8ridgelite"
            snapshot = "e69ebe0eb2752243de4678fe84df298555730c94"
            asset_id = "8ridgelite-Natural-0_e1"
            source_url = (
                "https://github.com/JamesStubbsEng/8ridgelite/blob/"
                "e69ebe0eb2752243de4678fe84df298555730c94/Natural/0_e1.wav")
            audio_name = "Natural-0_e1.wav"
            rights_kind = rights_kind or "written_rightsholder_confirmation"
            rights_basis = "author-grant-2026-08-29"
            evidence_text = (
                "The rightsholder confirms WAV-scope commercial calibration and "
                "private competitive evaluation rights for Natural/0_e1.wav.")
            sample_rate, channels = 48000, 2

        audio = audio_dir / audio_name
        write_pcm24(audio, sample_rate, channels)
        audio_hash = digest(audio)
        evidence = {
            "schema": "electry-reference-asset-evidence/v1",
            "dataset_id": dataset_id,
            "upstream_snapshot": snapshot,
            "instrument_string_count": 8,
            "upstream_asset_id": asset_id,
            "source_url": source_url,
            "download_representation": "upstream_original",
            "audio_sha256": audio_hash,
            "rights_evidence_kind": rights_kind,
            "rights_basis_id": rights_basis,
            "commercial_model_calibration": True,
            "private_competitive_evaluation": True,
            "evidence_text": evidence_text,
        }
        evidence_path = evidence_dir / f"{asset_id}.json"
        self.write(evidence_path, evidence)
        receipt = {
            "schema": "electry-reference-fixture/v1",
            "status": "frozen",
            "fixture_id": f"{dataset_id}-originals-2026",
            "target_protocol": "electry-evaluation/v4",
            "scientific_scope": "descriptive_comparison_only_no_fit_or_promotion",
            "source": {
                "dataset_id": dataset_id,
                "canonical_url": canonical_url,
                "upstream_snapshot": snapshot,
                "retrieved_utc": "2026-08-29T12:00:00Z",
                "instrument_string_count": 8,
            },
            "rights_review": {
                "reviewer_id": "electry-maintainer",
                "reviewed_utc": "2026-08-29T12:05:00Z",
                "covers_all_listed_audio_assets": True,
                "commercial_model_calibration": True,
                "private_competitive_evaluation": True,
            },
            "assets": [{
                "upstream_asset_id": asset_id,
                "source_url": source_url,
                "download_representation": "upstream_original",
                "file": f"external/{audio_name}",
                "sha256": audio_hash,
                "evidence_record_file": f"evidence/{asset_id}.json",
                "evidence_record_sha256": digest(evidence_path),
                "rights_evidence_kind": rights_kind,
                "rights_basis_id": rights_basis,
            }],
        }
        receipt_path = root / "receipt.json"
        self.write(receipt_path, receipt)
        return receipt_path, receipt, audio, evidence_path, evidence

    @staticmethod
    def write(path, value):
        path.write_text(json.dumps(value), encoding="utf-8")

    def test_original_cc0_and_author_confirmed_fixtures_pass(self):
        for source in ("cabled", "8ridgelite"):
            with self.subTest(source=source), tempfile.TemporaryDirectory() as directory:
                path, _, _, _, _ = self.make_fixture(Path(directory), source)
                self.assertEqual(TOOL.validate(path), digest(path))

    def test_audio_and_structured_evidence_are_hash_bound(self):
        for target_index in (0, 1):
            with self.subTest(target=target_index), tempfile.TemporaryDirectory() as directory:
                path, _, audio, evidence, _ = self.make_fixture(Path(directory))
                (audio, evidence)[target_index].write_bytes(b"changed")
                with self.assertRaisesRegex(TOOL.Invalid, "SHA-256 mismatch"):
                    TOOL.validate(path)

    def test_evidence_is_cross_bound_to_source_asset_and_audio(self):
        fields = (
            ("dataset_id", "another-dataset"),
            ("upstream_snapshot", "another-snapshot"),
            ("instrument_string_count", 7),
            ("upstream_asset_id", "another-asset"),
            ("source_url", "https://example.com/another.wav"),
            ("download_representation", "lossy_preview"),
            ("audio_sha256", "1" * 64),
        )
        for field, value in fields:
            with self.subTest(field=field), tempfile.TemporaryDirectory() as directory:
                path, receipt, _, evidence_path, evidence = self.make_fixture(
                    Path(directory))
                evidence[field] = value
                self.write(evidence_path, evidence)
                receipt["assets"][0]["evidence_record_sha256"] = digest(evidence_path)
                self.write(path, receipt)
                with self.assertRaisesRegex(TOOL.Invalid, "not cross-bound"):
                    TOOL.validate(path)

    def test_root_license_frozen_targets_preview_scope_and_rights_are_rejected(self):
        mutations = (
            (lambda value: value["assets"][0].update(
                rights_evidence_kind="repository_root_license"), "repository-root"),
            (lambda value: value["assets"][0].update(
                download_representation="lossy_preview"), "upstream original"),
            (lambda value: value.update(
                scientific_scope="calibration_fit_and_promotion"), "forbid fit"),
            (lambda value: value["rights_review"].update(
                commercial_model_calibration=False), "both permitted uses"),
            (lambda value: value["source"].update(
                instrument_string_count=7), "exact eight-string"),
        )
        for mutate, message in mutations:
            with self.subTest(message=message), tempfile.TemporaryDirectory() as directory:
                path, receipt, *_ = self.make_fixture(Path(directory))
                mutate(receipt)
                self.write(path, receipt)
                with self.assertRaisesRegex(TOOL.Invalid, message):
                    TOOL.validate(path)

        for target in ("electry-mute-capture/v1",
                       "electry-repick-phase-capture/v1",
                       "electry-evaluation/v3"):
            with self.subTest(target=target), tempfile.TemporaryDirectory() as directory:
                path, receipt, *_ = self.make_fixture(Path(directory))
                receipt["target_protocol"] = target
                self.write(path, receipt)
                with self.assertRaisesRegex(TOOL.Invalid, "successor"):
                    TOOL.validate(path)

    def test_audio_cannot_alias_its_evidence(self):
        for hardlink in (False, True):
            with self.subTest(hardlink=hardlink), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                path, receipt, audio, _, _ = self.make_fixture(root)
                if hardlink:
                    alias = root / "evidence" / "audio-hardlink.json"
                    os.link(audio, alias)
                    receipt["assets"][0]["evidence_record_file"] = (
                        "evidence/audio-hardlink.json")
                else:
                    receipt["assets"][0]["evidence_record_file"] = (
                        f"external/./{audio.name}")
                receipt["assets"][0]["evidence_record_sha256"] = digest(audio)
                self.write(path, receipt)
                with self.assertRaisesRegex(TOOL.Invalid, "own evidence"):
                    TOOL.validate(path)

    def test_non_audio_payload_is_rejected_after_hash_validation(self):
        with tempfile.TemporaryDirectory() as directory:
            path, receipt, audio, evidence_path, evidence = self.make_fixture(
                Path(directory))
            audio.write_bytes(b"RIFF not really audio")
            audio_hash = digest(audio)
            receipt["assets"][0]["sha256"] = audio_hash
            evidence["audio_sha256"] = audio_hash
            self.write(evidence_path, evidence)
            receipt["assets"][0]["evidence_record_sha256"] = digest(evidence_path)
            self.write(path, receipt)
            with self.assertRaisesRegex(TOOL.Invalid, "RIFF/WAVE"):
                TOOL.validate(path)

    def test_extensible_pcm24_passes_and_malformed_fmt_size_fails(self):
        for extensible, malformed, passes in ((True, False, True),
                                              (False, True, False)):
            with self.subTest(extensible=extensible), tempfile.TemporaryDirectory() as directory:
                path, receipt, audio, evidence_path, evidence = self.make_fixture(
                    Path(directory))
                write_pcm24(audio, extensible=extensible, malformed_extra=malformed)
                audio_hash = digest(audio)
                receipt["assets"][0]["sha256"] = audio_hash
                evidence["audio_sha256"] = audio_hash
                self.write(evidence_path, evidence)
                receipt["assets"][0]["evidence_record_sha256"] = digest(evidence_path)
                self.write(path, receipt)
                if passes:
                    self.assertEqual(TOOL.validate(path), digest(path))
                else:
                    with self.assertRaisesRegex(TOOL.Invalid, "format chunk size"):
                        TOOL.validate(path)

    def test_placeholders_duplicate_keys_and_malformed_input_fail_closed(self):
        mutations = (
            (lambda value: value.update(fixture_id="REPLACE"), "REPLACE"),
            (lambda value: value["source"].update(upstream_snapshot=None), "null"),
            (lambda value: value["assets"][0].update(sha256="0" * 64),
             "zero hash"),
            (lambda value: value["assets"][0].update(sha256=[]),
             "audio SHA-256"),
            (lambda value: value["source"].update(canonical_url="https://["),
             "valid HTTPS"),
            (lambda value: value["source"].update(canonical_url="https://user@"),
             "without credentials"),
            (lambda value: value["source"].update(
                canonical_url="https://example.com:bad"), "valid HTTPS"),
            (lambda value: value["source"].update(
                canonical_url="https://example.com/white space"), "without credentials"),
        )
        for mutate, message in mutations:
            with self.subTest(message=message), tempfile.TemporaryDirectory() as directory:
                path, receipt, *_ = self.make_fixture(Path(directory))
                mutate(receipt)
                self.write(path, receipt)
                with self.assertRaisesRegex(TOOL.Invalid, message):
                    TOOL.validate(path)

        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "receipt.json"
            path.write_text('{"schema":"one","schema":"two"}', encoding="utf-8")
            with self.assertRaisesRegex(TOOL.Invalid, "duplicate JSON key"):
                TOOL.validate(path)

            path.write_bytes(b"\xff")
            result = subprocess.run(
                [sys.executable, str(TOOL_PATH), str(path)],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
            self.assertEqual(result.returncode, 1)
            self.assertIn(b"invalid:", result.stderr)
            self.assertNotIn(b"Traceback", result.stderr)

    def test_duplicate_asset_identity_path_and_audio_are_rejected(self):
        fields = (
            ("upstream_asset_id", "upstream asset ID"),
            ("file", "local audio path"),
            ("sha256", "audio hash"),
        )
        for field, message in fields:
            with self.subTest(field=field), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                path, receipt, _, _, evidence = self.make_fixture(root)
                second_audio = root / "external" / "second.wav"
                write_pcm24(second_audio, frames=96)
                second_hash = digest(second_audio)
                second_id = "freesound-525011"
                second_url = "https://freesound.org/people/cabled_mess/sounds/525011/"
                second_evidence = copy.deepcopy(evidence)
                second_evidence.update({
                    "upstream_asset_id": second_id,
                    "source_url": second_url,
                    "audio_sha256": second_hash,
                    "evidence_text": "Archived second per-sound API CC0 record.",
                })
                second_evidence_path = root / "evidence" / f"{second_id}.json"
                self.write(second_evidence_path, second_evidence)
                second = copy.deepcopy(receipt["assets"][0])
                second.update({
                    "upstream_asset_id": second_id,
                    "source_url": second_url,
                    "file": "external/second.wav",
                    "sha256": second_hash,
                    "evidence_record_file": f"evidence/{second_id}.json",
                    "evidence_record_sha256": digest(second_evidence_path),
                })
                second[field] = receipt["assets"][0][field]
                receipt["assets"].append(second)
                self.write(path, receipt)
                with self.assertRaisesRegex(TOOL.Invalid, message):
                    TOOL.validate(path)

    def test_generated_templates_match_the_accepted_shapes(self):
        with tempfile.TemporaryDirectory() as directory:
            _, receipt, _, _, evidence = self.make_fixture(Path(directory))
        templates = (
            (TOOL.RECEIPT_TEMPLATE, receipt),
            (TOOL.EVIDENCE_TEMPLATE, evidence),
        )

        def compare(template, filled):
            if isinstance(template, dict):
                self.assertEqual(set(template), set(filled))
                for key in template:
                    compare(template[key], filled[key])
            elif isinstance(template, list):
                self.assertEqual(len(template), len(filled))
                for expected, actual in zip(template, filled):
                    compare(expected, actual)
            elif not (isinstance(template, str)
                      and ("REPLACE" in template or set(template) == {"0"})):
                self.assertEqual(template, filled)

        for template, filled in templates:
            compare(template, filled)

        for name, template in (("receipt", TOOL.RECEIPT_TEMPLATE),
                               ("evidence", TOOL.EVIDENCE_TEMPLATE)):
            result = subprocess.run(
                [sys.executable, str(TOOL_PATH), "--print-template", name],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
            self.assertEqual(result.returncode, 0)
            self.assertEqual(json.loads(result.stdout), template)


if __name__ == "__main__":
    unittest.main()
