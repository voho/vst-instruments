#!/usr/bin/env python3

import hashlib
import importlib.util
import io
import json
import sys
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
from contextlib import redirect_stderr
from pathlib import Path

sys.dont_write_bytecode = True
from BlindListeningTestSupport import build_valid_study


ROOT = Path(__file__).resolve().parents[1]


def load_tool(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / "Tools" / f"{name}.py")
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


PREPARE = load_tool("PrepareBlindListening")
SCORE = load_tool("ScoreBlindListening")
SERVER = load_tool("ServeBlindListening")


class ScoreBlindListeningTests(unittest.TestCase):
    def test_archive_json_hash_and_parse_share_one_snapshot(self):
        first = b'{"status":"frozen"}'
        second = b'{"status":"swapped"}'

        class SwappingArchive:
            def __init__(self):
                self.read_count = 0

            def is_symlink(self):
                return False

            def read_bytes(self):
                self.read_count += 1
                return first if self.read_count == 1 else second

            def resolve(self):
                return Path("/virtual/archive.json")

        archive = SwappingArchive()
        value, record = SCORE._checked_json(
            archive, hashlib.sha256(first).hexdigest(), "swapping archive")
        self.assertEqual(archive.read_count, 1)
        self.assertEqual(value, {"status": "frozen"})
        self.assertEqual(record["sha256"], hashlib.sha256(first).hexdigest())

        with tempfile.TemporaryDirectory() as directory:
            invalid = Path(directory) / "invalid.json"
            for raw, message in (
                    (b'{"value":1,"value":2}', "duplicate JSON key"),
                    (b'{"value":NaN}', "non-finite JSON number")):
                invalid.write_bytes(raw)
                with self.subTest(message=message), self.assertRaisesRegex(
                        SCORE.ScoreError, message):
                    SCORE._checked_json(
                        invalid, hashlib.sha256(raw).hexdigest(), "invalid archive")

    def test_passing_cohort_and_malformed_response_rejection(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            presentation_seed = (
                "3f8e6649790cd57185ee2d4f6c9301d6"
                "f8fbbb80ce7a511b3226b40b8ba2cee7")
            fixture = build_valid_study(
                root, PREPARE, study_id="score-self-test",
                presentation_seed=presentation_seed)
            manifest_path = fixture["manifest_path"]
            pack = root / "pack"
            PREPARE.prepare(manifest_path, pack)
            key_path = pack / "private" / "answer-key.json"
            key = json.loads(key_path.read_text(encoding="utf-8"))
            anchor = key["study_fingerprint"]
            self.assertEqual(
                {item["listener_stratum"] for item in key["participants"][:15]},
                {"extended_range_guitarist"})
            self.assertEqual(
                {item["listener_stratum"] for item in key["participants"][15:]},
                {"metal_producer"})

            responses = root / "responses"
            responses.mkdir()
            correct_by_cell = {cell: set() for cell in range(1, 11)}
            for pair_index in range(15):
                guitarist = f"p{pair_index + 1:03d}"
                producer = f"p{pair_index + 16:03d}"
                six_cells = {
                    (pair_index + offset) % 10 + 1 for offset in range(6)
                }
                high, low = ((guitarist, producer) if pair_index % 2 == 0
                             else (producer, guitarist))
                for cell in range(1, 11):
                    correct_by_cell[cell].add(
                        high if cell in six_cells else low)
            self.assertTrue(all(len(value) == 15
                                for value in correct_by_cell.values()))
            correct_counts = {
                participant["participant_id"]: sum(
                    participant["participant_id"] in correct_by_cell[cell]
                    for cell in range(1, 11))
                for participant in key["participants"]
            }
            for stratum in (range(1, 16), range(16, 31)):
                self.assertEqual(
                    {correct_counts[f"p{number:03d}"] for number in stratum},
                    {4, 6})

            for participant_number, participant in enumerate(key["participants"], 1):
                records = []
                ordered = [("practice", item) for item in participant["practice"]]
                ordered += [("scored", item) for item in participant["trials"]]
                for sequence, (section, trial) in enumerate(ordered, 1):
                    if section == "practice":
                        physical_choice = "a"
                        preference = "tie"
                    else:
                        selected = ("physical"
                                    if participant["participant_id"] in
                                    correct_by_cell[trial["cell"]]
                                    else "electry")
                        physical_choice = ("a" if trial["a_source"] == selected
                                           else "b")
                        if trial["repeat_of"] is None:
                            preferred = ("electry"
                                         if (participant_number + trial["cell"]) % 5 < 3
                                         else "physical")
                            preference = ("a" if trial["a_source"] == preferred
                                          else "b")
                        else:
                            preference = "tie"
                    records.append({
                        "confidence": 3,
                        "defect": None,
                        "elapsed_ms": 1000,
                        "physical_choice": physical_choice,
                        "plays_a": 1,
                        "plays_b": 1,
                        "preference": preference,
                        "replay_count": 0,
                        "section": section,
                        "sequence": sequence,
                        "trial_id": trial["trial_id"],
                    })
                response = {
                    "completed_utc": "2026-08-25T12:10:00.000Z",
                    "mapping_commitment": participant["mapping_commitment"],
                    "playback_screen_confirmed": True,
                    "responses": records,
                    "schema": "electry-blind-results/v1",
                    "session_token": participant["session_token"],
                    "started_utc": "2026-08-25T12:00:00.000Z",
                    "study_fingerprint": key["study_fingerprint"],
                    "study_id": key["study_id"],
                }
                (responses / f"{participant['session_token']}.json").write_text(
                    json.dumps(response), encoding="utf-8")

            result = SCORE.score(key_path, responses, anchor)
            self.assertTrue(result["gates"]["all"])
            self.assertEqual(
                result["endpoints"]["physical_source_identification"][
                    "bootstrap_90_percentile_interval"],
                [0.47333333333333333, 0.5266666666666666])
            self.assertAlmostEqual(
                result["endpoints"]["electry_preference"][
                    "bootstrap_one_sided_95_lower_bound"], 0.6)
            self.assertEqual(
                result["endpoints"]["hidden_repeat_identification"][
                    "same_source_agreement"], 1.0)
            self.assertEqual(result["endpoints"]["a_side_choice"]["n"], 390)
            self.assertAlmostEqual(
                result["endpoints"]["a_side_choice"]["a_choice_rate"],
                202 / 390)
            self.assertEqual(len(result["inputs"]["responses"]), 30)
            self.assertEqual(
                result["inputs"]["external_expected_fingerprint"], anchor)
            self.assertEqual(result["inputs"]["scorer"]["sha256"],
                             key["implementation"]["scorer_sha256"])
            frozen_pack = result["inputs"]["frozen_pack"]
            self.assertEqual(frozen_pack["public"]["sessions"]["count"], 30)
            self.assertEqual(frozen_pack["public"]["stimuli"]["count"], 900)
            self.assertEqual(
                len(frozen_pack["archives"]["evidence"]["capture_manifests"]), 5)
            self.assertGreater(
                len(frozen_pack["archives"]["evidence"]["event_records"]), 0)
            self.assertEqual(
                len(frozen_pack["archives"]["evidence"][
                    "engineering_train_analysis"]), 3)
            self.assertEqual(
                frozen_pack["archives"]["participant_links"]["sha256"],
                key["participant_links"]["sha256"])
            self.assertEqual(len(frozen_pack["public"]["files"]), 932)

            server = SERVER.make_server(pack, "127.0.0.1", 0, anchor.upper())
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            try:
                base = f"http://127.0.0.1:{server.server_port}/"
                first_session = json.loads(
                    (pack / "public" / "sessions"
                     / f"{key['participants'][0]['session_token']}.json").read_text(
                         encoding="utf-8"))
                relative_audio = first_session["practice"][0]["a"]
                served_audio = pack / "public" / relative_audio
                audio_snapshot = served_audio.read_bytes()
                with redirect_stderr(io.StringIO()):
                    request = urllib.request.Request(
                        base + relative_audio, headers={"Range": "bytes=0-9"})
                    with urllib.request.urlopen(request, timeout=2) as reply:
                        self.assertEqual(reply.status, 206)
                        self.assertEqual(reply.headers["Content-Range"],
                                         f"bytes 0-9/{len(audio_snapshot)}")
                        self.assertEqual(reply.read(), audio_snapshot[:10])
                    head = urllib.request.Request(
                        base + relative_audio, method="HEAD")
                    with urllib.request.urlopen(head, timeout=2) as reply:
                        self.assertEqual(reply.status, 200)
                        self.assertEqual(reply.read(), b"")
                    invalid = urllib.request.Request(
                        base + relative_audio, headers={"Range": "bytes=9-3"})
                    with self.assertRaises(urllib.error.HTTPError) as error:
                        urllib.request.urlopen(invalid, timeout=2)
                    self.assertEqual(error.exception.code, 416)
                    with self.assertRaises(urllib.error.HTTPError) as error:
                        urllib.request.urlopen(base + "%69ndex.html", timeout=2)
                    self.assertEqual(error.exception.code, 404)
                    served_audio.write_bytes(audio_snapshot + b"\0")
                    with self.assertRaises(urllib.error.HTTPError) as error:
                        urllib.request.urlopen(base + relative_audio, timeout=2)
                    self.assertEqual(error.exception.code, 409)
                served_audio.write_bytes(audio_snapshot)
            finally:
                server.shutdown()
                thread.join(timeout=2)
                server.server_close()
            with self.assertRaisesRegex(
                    ValueError, "external expected fingerprint"):
                SERVER.make_server(pack, "127.0.0.1", 0, "00" * 32)

            remapped = json.loads(key_path.read_text(encoding="utf-8"))
            trial = next(item for item in remapped["participants"][0]["trials"]
                         if item["cell"] == 1 and item["repeat_of"] is None)
            trial["a_source"], trial["b_source"] = trial["b_source"], trial["a_source"]
            trial["a_sha256"], trial["b_sha256"] = trial["b_sha256"], trial["a_sha256"]
            participant = remapped["participants"][0]
            mapping = {name: participant[name] for name in (
                "listener_stratum", "participant_id", "practice",
                "session_token", "trials")}
            participant["mapping_commitment"] = SCORE._mapping_commitment(
                remapped["presentation_seed"], mapping)
            remapped_path = pack / "private" / "remapped-key.json"
            remapped_path.write_text(json.dumps(remapped), encoding="utf-8")
            with self.assertRaisesRegex(
                    SCORE.ScoreError,
                    "scored presentation differs from the frozen seed"):
                SCORE.score(remapped_path, responses, anchor)

            malformed_key = json.loads(key_path.read_text(encoding="utf-8"))
            malformed_participant = malformed_key["participants"][0]
            malformed_participant["trials"][0]["a_source"] = []
            malformed_mapping = {name: malformed_participant[name] for name in (
                "listener_stratum", "participant_id", "practice",
                "session_token", "trials")}
            malformed_participant["mapping_commitment"] = SCORE._mapping_commitment(
                malformed_key["presentation_seed"], malformed_mapping)
            malformed_key_path = pack / "private" / "malformed-key.json"
            malformed_key_path.write_text(
                json.dumps(malformed_key), encoding="utf-8")
            with self.assertRaisesRegex(
                    SCORE.ScoreError, "map A/B to both sources"):
                SCORE.score(malformed_key_path, responses, anchor)

            malformed_path = responses / f"{key['participants'][0]['session_token']}.json"
            malformed = json.loads(malformed_path.read_text(encoding="utf-8"))
            malformed["responses"][0]["replay_count"] = 1
            malformed_path.write_text(json.dumps(malformed), encoding="utf-8")
            with self.assertRaisesRegex(SCORE.ScoreError, "invalid play counts"):
                SCORE.score(key_path, responses, anchor)

            malformed["responses"][0]["replay_count"] = 0
            for field, value, message in (
                    ("physical_choice", [], "forced A/B choice"),
                    ("preference", {}, "invalid preference"),
                    ("defect", [], "invalid defect")):
                original = malformed["responses"][0][field]
                malformed["responses"][0][field] = value
                malformed_path.write_text(json.dumps(malformed), encoding="utf-8")
                with self.subTest(field=field), self.assertRaisesRegex(
                        SCORE.ScoreError, message):
                    SCORE.score(key_path, responses, anchor)
                malformed["responses"][0][field] = original
            malformed_path.write_text(json.dumps(malformed), encoding="utf-8")

            comparison_archive = pack / "private" / "comparison-manifest.json"
            archived_bytes = comparison_archive.read_bytes()
            comparison_archive.write_bytes(archived_bytes + b" ")
            with self.assertRaisesRegex(
                    SCORE.ScoreError, "archived comparison manifest SHA-256"):
                SCORE.score(key_path, responses, anchor)
            comparison_archive.write_bytes(archived_bytes)

            study_archive = pack / "private" / "study-manifest.json"
            study_bytes = study_archive.read_bytes()
            study_archive.write_bytes(study_bytes + b" ")
            with self.assertRaisesRegex(
                    SCORE.ScoreError, "archived study manifest SHA-256"):
                SCORE.score(key_path, responses, anchor)
            study_archive.write_bytes(study_bytes)

            preparer_archive = pack / "private" / "prepare.py"
            preparer_bytes = preparer_archive.read_bytes()
            preparer_archive.write_bytes(preparer_bytes + b"\n")
            with self.assertRaisesRegex(
                    SCORE.ScoreError, "archived preparer SHA-256"):
                SCORE.score(key_path, responses, anchor)
            preparer_archive.write_bytes(preparer_bytes)

            registry_archive = pack / "private" / "artifact-registry.json"
            registry_bytes = registry_archive.read_bytes()
            registry_archive.write_bytes(registry_bytes + b" ")
            with self.assertRaisesRegex(
                    SCORE.ScoreError, "archived artifact registry SHA-256"):
                SCORE.score(key_path, responses, anchor)
            registry_archive.write_bytes(registry_bytes)

            derivation_receipt_archive = (
                pack / "private" / "engineering-derivation-receipt.json")
            derivation_receipt_bytes = derivation_receipt_archive.read_bytes()
            derivation_receipt_archive.write_bytes(derivation_receipt_bytes + b" ")
            with self.assertRaisesRegex(
                    SCORE.ScoreError,
                    "archived engineering derivation receipt SHA-256"):
                SCORE.score(key_path, responses, anchor)
            derivation_receipt_archive.write_bytes(derivation_receipt_bytes)

            derivation_result_archive = (
                pack / "private" / "engineering-derivation-result.json")
            derivation_result_bytes = derivation_result_archive.read_bytes()
            derivation_result_archive.write_bytes(derivation_result_bytes + b" ")
            with self.assertRaisesRegex(
                    SCORE.ScoreError,
                    "archived engineering derivation result SHA-256"):
                SCORE.score(key_path, responses, anchor)
            derivation_result_archive.write_bytes(derivation_result_bytes)

            train_analysis_archive = next(
                (pack / "private" / "engineering-train-analysis").glob("*.json"))
            train_analysis_bytes = train_analysis_archive.read_bytes()
            train_analysis_archive.write_bytes(train_analysis_bytes + b" ")
            with self.assertRaisesRegex(
                    SCORE.ScoreError,
                    "archived engineering train analysis.*SHA-256"):
                SCORE.score(key_path, responses, anchor)
            train_analysis_archive.write_bytes(train_analysis_bytes)

            participant_links = pack / "private" / "participant-links.tsv"
            participant_links_bytes = participant_links.read_bytes()
            participant_links.write_bytes(participant_links_bytes + b"tampered\n")
            with self.assertRaisesRegex(
                    SCORE.ScoreError, "archived participant links SHA-256"):
                SCORE.score(key_path, responses, anchor)
            with self.assertRaisesRegex(
                    ValueError, "archived participant links SHA-256"):
                SERVER.make_server(pack, "127.0.0.1", 0, anchor)
            participant_links.write_bytes(participant_links_bytes)

            key_bytes = key_path.read_bytes()
            changed_links = participant_links_bytes.replace(
                b"p001\t", b"x001\t", 1)
            participant_links.write_bytes(changed_links)
            changed_key = json.loads(key_bytes)
            changed_key["participant_links"]["sha256"] = hashlib.sha256(
                changed_links).hexdigest()
            key_path.write_text(json.dumps(changed_key), encoding="utf-8")
            with self.assertRaisesRegex(
                    SCORE.ScoreError,
                    "participant links do not match the answer key"):
                SCORE.score(key_path, responses, anchor)
            with self.assertRaisesRegex(
                    ValueError, "participant links do not match the answer key"):
                SERVER.make_server(pack, "127.0.0.1", 0, anchor)
            key_path.write_bytes(key_bytes)
            participant_links.write_bytes(participant_links_bytes)

            first_participant = key["participants"][0]
            session_path = (pack / "public" / "sessions"
                            / f"{first_participant['session_token']}.json")
            session_bytes = session_path.read_bytes()
            session = json.loads(session_bytes)
            session["study_id"] = "tampered"
            session_path.write_text(json.dumps(session), encoding="utf-8")
            with self.assertRaisesRegex(
                    SCORE.ScoreError, "public session.*does not match"):
                SCORE.score(key_path, responses, anchor)
            session_path.write_bytes(session_bytes)

            session = json.loads(session_bytes)
            audio_path = pack / "public" / session["practice"][0]["a"]
            audio_bytes = audio_path.read_bytes()
            audio_path.write_bytes(audio_bytes + b"\0")
            with self.assertRaisesRegex(
                    SCORE.ScoreError, "public practice audio.*SHA-256"):
                SCORE.score(key_path, responses, anchor)
            audio_path.write_bytes(audio_bytes)

            leaked_key = pack / "public" / "answer-key.json"
            leaked_key.write_text("{}\n", encoding="utf-8")
            with self.assertRaisesRegex(
                    SCORE.ScoreError, "missing or unexpected entries"):
                SCORE.score(key_path, responses, anchor)
            leaked_key.unlink()

            replacement_comparison = json.loads(archived_bytes)
            replacement_comparison["engineering_freeze"][
                "candidate_contour_rmse_reduction_min"] = 0.3
            comparison_archive.write_text(
                json.dumps(replacement_comparison), encoding="utf-8")
            replacement_comparison_sha256 = hashlib.sha256(
                comparison_archive.read_bytes()).hexdigest()
            replacement_study = json.loads(study_bytes)
            replacement_study["comparison_manifest"][
                "sha256"] = replacement_comparison_sha256
            study_archive.write_text(
                json.dumps(replacement_study), encoding="utf-8")
            replacement_study_sha256 = hashlib.sha256(
                study_archive.read_bytes()).hexdigest()
            replacement_key = json.loads(key_path.read_text(encoding="utf-8"))
            replacement_key["comparison_manifest"][
                "sha256"] = replacement_comparison_sha256
            replacement_key["source_manifest"]["sha256"] = replacement_study_sha256
            implementation = replacement_key["implementation"]
            replacement_key["study_fingerprint"] = hashlib.sha256(
                (f"{replacement_study_sha256}:"
                 f"{implementation['preparer_sha256']}:"
                 f"{implementation['runner_sha256']}:"
                 f"{implementation['scorer_sha256']}:"
                 f"{implementation['server_sha256']}").encode("ascii")
            ).hexdigest()
            replacement_key_path = pack / "private" / "replacement-key.json"
            replacement_key_path.write_text(
                json.dumps(replacement_key), encoding="utf-8")
            replacement_session_bytes = {}
            for participant in key["participants"]:
                path = (pack / "public" / "sessions"
                        / f"{participant['session_token']}.json")
                replacement_session_bytes[path] = path.read_bytes()
                value = json.loads(replacement_session_bytes[path])
                value["study_fingerprint"] = replacement_key["study_fingerprint"]
                path.write_text(json.dumps(value), encoding="utf-8")
            with self.assertRaisesRegex(
                    SCORE.ScoreError, "external expected fingerprint does not match"):
                SCORE.score(replacement_key_path, responses, anchor)
            for path, value in replacement_session_bytes.items():
                path.write_bytes(value)
            comparison_archive.write_bytes(archived_bytes)
            study_archive.write_bytes(study_bytes)

    def test_coherently_rehashed_wrong_train_provenance_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixture = build_valid_study(
                root, PREPARE, study_id="semantic-tamper-self-test")
            pack = root / "pack"
            PREPARE.prepare(fixture["manifest_path"], pack)
            key_path = pack / "private" / "answer-key.json"
            key = json.loads(key_path.read_text(encoding="utf-8"))
            original_anchor = key["study_fingerprint"]

            def rewrite(path, value):
                path.write_text(json.dumps(value), encoding="utf-8")
                return hashlib.sha256(path.read_bytes()).hexdigest()

            analysis_key = key["archives"]["engineering_train_analysis"][0]
            cluster_id = analysis_key["cluster_id"]
            analysis_path = pack / analysis_key["path"]
            analysis = json.loads(analysis_path.read_text(encoding="utf-8"))
            raw_endpoint = analysis["endpoints"][0]
            wrong_input = dict(
                raw_endpoint["eligible_input_units"][0]["input_units"][1])
            raw_endpoint["eligible_input_units"][0]["input_units"][0] = wrong_input
            raw_endpoint["repeatability_samples"][0]["input_units"][0] = wrong_input
            old_analysis_sha256 = analysis_key["sha256"]
            analysis_sha256 = rewrite(analysis_path, analysis)

            result_path = pack / "private" / "engineering-derivation-result.json"
            result = json.loads(result_path.read_text(encoding="utf-8"))
            result_cluster = next(
                value for value in result["train_clusters"]
                if value["cluster_id"] == cluster_id)
            result_cluster["analysis_result"]["sha256"] = analysis_sha256
            result_sample = next(
                value for value in result["endpoints"][0][
                    "train_repeatability_samples"]
                if value["cluster_id"] == cluster_id and value["unit_index"] == 1)
            result_sample["input_units"][0] = wrong_input
            result_sha256 = rewrite(result_path, result)

            receipt_path = pack / "private" / "engineering-derivation-receipt.json"
            receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
            receipt["derivation_result"]["sha256"] = result_sha256
            receipt_sha256 = rewrite(receipt_path, receipt)

            registry_path = pack / "private" / "artifact-registry.json"
            registry = json.loads(registry_path.read_text(encoding="utf-8"))
            registry_artifact = next(
                value for value in registry["artifacts"]
                if value["sha256"] == old_analysis_sha256)
            registry_artifact["sha256"] = analysis_sha256
            registry_sha256 = rewrite(registry_path, registry)

            comparison_path = pack / "private" / "comparison-manifest.json"
            comparison = json.loads(comparison_path.read_text(encoding="utf-8"))
            comparison["engineering_freeze"]["derivation_receipt"][
                "sha256"] = receipt_sha256
            comparison["artifact_registry"]["sha256"] = registry_sha256
            comparison_sha256 = rewrite(comparison_path, comparison)

            study_path = pack / "private" / "study-manifest.json"
            study = json.loads(study_path.read_text(encoding="utf-8"))
            study["comparison_manifest"]["sha256"] = comparison_sha256
            study_sha256 = rewrite(study_path, study)

            analysis_key["sha256"] = analysis_sha256
            key["archives"]["engineering_derivation_result"][
                "sha256"] = result_sha256
            key["archives"]["engineering_derivation_receipt"][
                "sha256"] = receipt_sha256
            key["archives"]["artifact_registry"]["sha256"] = registry_sha256
            key["comparison_manifest"]["sha256"] = comparison_sha256
            key["source_manifest"]["sha256"] = study_sha256
            implementation = key["implementation"]
            new_anchor = hashlib.sha256(
                (f"{study_sha256}:{implementation['preparer_sha256']}:"
                 f"{implementation['runner_sha256']}:"
                 f"{implementation['scorer_sha256']}:"
                 f"{implementation['server_sha256']}").encode("ascii")
            ).hexdigest()
            key["study_fingerprint"] = new_anchor
            rewrite(key_path, key)
            for participant in key["participants"]:
                session_path = (pack / "public" / "sessions"
                                / f"{participant['session_token']}.json")
                session = json.loads(session_path.read_text(encoding="utf-8"))
                session["study_fingerprint"] = new_anchor
                rewrite(session_path, session)

            with self.assertRaisesRegex(
                    SCORE.ScoreError, "external expected fingerprint"):
                SCORE._validate_key(key_path, original_anchor)
            with self.assertRaisesRegex(
                    SCORE.ScoreError, "differs from canonical capture provenance"):
                SCORE._validate_key(key_path, new_anchor)
            with self.assertRaisesRegex(
                    ValueError, "differs from canonical capture provenance"):
                SERVER.make_server(pack, "127.0.0.1", 0, new_anchor)

    def test_uppercase_frozen_hex_round_trip(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            seed = hashlib.sha256(b"uppercase-round-trip").hexdigest().upper()
            fixture = build_valid_study(
                root, PREPARE, study_id="uppercase-self-test",
                presentation_seed=seed)
            study = fixture["manifest"]
            comparison = fixture["comparison"]
            study["presentation_seed"] = seed
            comparison["presentation_seed"] = seed
            comparison["freezes"]["analysis"][
                "listener_scorer_sha256"] = comparison["freezes"]["analysis"][
                    "listener_scorer_sha256"].upper()
            for study_pair, comparison_pair in zip(
                    study["practice"] + study["cells"],
                    comparison["practice"] + comparison["cells"]):
                for source in ("physical", "electry"):
                    study_pair[source]["sha256"] = study_pair[source][
                        "sha256"].upper()
                    comparison_pair[f"{source}_sha256"] = comparison_pair[
                        f"{source}_sha256"].upper()
            fixture["comparison_path"].write_text(
                json.dumps(comparison), encoding="utf-8")
            study["comparison_manifest"]["sha256"] = hashlib.sha256(
                fixture["comparison_path"].read_bytes()).hexdigest().upper()
            fixture["manifest_path"].write_text(
                json.dumps(study), encoding="utf-8")

            pack = root / "pack"
            PREPARE.prepare(fixture["manifest_path"], pack)
            key_path = pack / "private" / "answer-key.json"
            expected_fingerprint = json.loads(
                key_path.read_text(encoding="utf-8"))["study_fingerprint"]
            key, _, frozen_pack = SCORE._validate_key(
                key_path, expected_fingerprint.upper())
            self.assertEqual(key["presentation_seed"], seed.lower())
            self.assertEqual(frozen_pack["public"]["sessions"]["count"], 30)


if __name__ == "__main__":
    unittest.main()
