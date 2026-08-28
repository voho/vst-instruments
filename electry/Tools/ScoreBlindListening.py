#!/usr/bin/env python3
"""Validate and score one complete Electry blinded A/B cohort."""

from __future__ import annotations

import argparse
import hashlib
import itertools
import json
import math
import re
from datetime import datetime
from pathlib import Path
from typing import Any


KEY_SCHEMA = "electry-blind-answer-key/v1"
STUDY_SCHEMA = "electry-blind-study/v1"
SESSION_SCHEMA = "electry-blind-session/v1"
COMPARISON_SCHEMA = "electry-blind-comparison/v1"
RESULT_SCHEMA = "electry-blind-results/v1"
SCORE_SCHEMA = "electry-blind-score/v1"
ANALYSIS_SCHEMA = "electry-blind-analysis/v1"
BOOTSTRAP_REPLICATES = 20000
MAX_REPLAYS = 3
PARTICIPANTS = [f"p{number:03d}" for number in range(1, 31)]
STRATA = {
    "extended_range_guitarist": PARTICIPANTS[:15],
    "metal_producer": PARTICIPANTS[15:],
}
CELL_META = {
    1: ("palm", "e1_palm_single_dry"),
    2: ("palm", "e2_palm_single_dry"),
    3: ("dead", "e1_dead_single_dry"),
    4: ("dead", "e2_dead_single_dry"),
    5: ("palm", "e1_palm_rapid_dry"),
    6: ("palm", "e1_palm_rapid_processed"),
    7: ("dead", "dead_groove_dry"),
    8: ("dead", "dead_groove_processed"),
    9: ("palm", "palm_open_transition_dry"),
    10: ("palm", "palm_open_transition_processed"),
}
REPEAT_CELLS = (5, 7, 9)
CORE_FRAMES = {
    1: 30870, 2: 30870,
    3: 18963, 4: 18963,
    5: 53802, 6: 53802,
    7: 44541, 8: 44541,
    9: 67032, 10: 67032,
}
CAPTURE_TAKE_FILES = (
    "e1-open.wav", "e1-palm-near.wav", "e1-palm-middle.wav",
    "e1-palm-far.wav", "e1-dead.wav", "e2-open.wav",
    "e2-palm-near.wav", "e2-palm-middle.wav", "e2-palm-far.wav",
    "e2-dead.wav", "e1-palm-middle-rapid.wav", "e2-palm-middle-rapid.wav",
    "e1-dead-rapid.wav", "e2-dead-rapid.wav", "dead-e1-e2-groove.wav",
    "palm-open-e1-e2-groove.wav",
)
CAPTURE_TAKE_SPECS = {
    "e1-open.wav": {
        "kind": "isolated", "target": "e1", "string_number": 8,
        "articulation": "open", "pick_pattern": "hard_single_by_slot"},
    "e1-palm-near.wav": {
        "kind": "isolated", "target": "e1", "string_number": 8,
        "articulation": "palm", "palm_position": "near",
        "pick_pattern": "hard_single_by_slot"},
    "e1-palm-middle.wav": {
        "kind": "isolated", "target": "e1", "string_number": 8,
        "articulation": "palm", "palm_position": "middle",
        "pick_pattern": "hard_single_by_slot"},
    "e1-palm-far.wav": {
        "kind": "isolated", "target": "e1", "string_number": 8,
        "articulation": "palm", "palm_position": "far",
        "pick_pattern": "hard_single_by_slot"},
    "e1-dead.wav": {
        "kind": "isolated", "target": "e1", "string_number": 8,
        "articulation": "dead", "pick_pattern": "hard_single_by_slot"},
    "e2-open.wav": {
        "kind": "isolated", "target": "e2", "string_number": 6,
        "articulation": "open", "pick_pattern": "hard_single_by_slot"},
    "e2-palm-near.wav": {
        "kind": "isolated", "target": "e2", "string_number": 6,
        "articulation": "palm", "palm_position": "near",
        "pick_pattern": "hard_single_by_slot"},
    "e2-palm-middle.wav": {
        "kind": "isolated", "target": "e2", "string_number": 6,
        "articulation": "palm", "palm_position": "middle",
        "pick_pattern": "hard_single_by_slot"},
    "e2-palm-far.wav": {
        "kind": "isolated", "target": "e2", "string_number": 6,
        "articulation": "palm", "palm_position": "far",
        "pick_pattern": "hard_single_by_slot"},
    "e2-dead.wav": {
        "kind": "isolated", "target": "e2", "string_number": 6,
        "articulation": "dead", "pick_pattern": "hard_single_by_slot"},
    "e1-palm-middle-rapid.wav": {
        "kind": "rapid", "target": "e1", "string_number": 8,
        "articulation": "palm", "palm_position": "middle",
        "pick_pattern": "hard_alternate"},
    "e2-palm-middle-rapid.wav": {
        "kind": "rapid", "target": "e2", "string_number": 6,
        "articulation": "palm", "palm_position": "middle",
        "pick_pattern": "hard_alternate"},
    "e1-dead-rapid.wav": {
        "kind": "rapid", "target": "e1", "string_number": 8,
        "articulation": "dead", "pick_pattern": "hard_alternate"},
    "e2-dead-rapid.wav": {
        "kind": "rapid", "target": "e2", "string_number": 6,
        "articulation": "dead", "pick_pattern": "hard_alternate"},
    "dead-e1-e2-groove.wav": {
        "kind": "groove", "target": "mixed_e1_e2", "string_numbers": [8, 6],
        "articulation": "dead", "pick_pattern": "hard_alternate"},
    "palm-open-e1-e2-groove.wav": {
        "kind": "groove", "target": "mixed_e1_e2", "string_numbers": [8, 6],
        "articulation": "palm_open_transition", "palm_position": "middle",
        "pick_pattern": "hard_alternate"},
}
ENGINEERING_ENDPOINT_IDS = (
    "palm_harmonic_contour_rmse", "palm_onset_to_peak_error",
    "palm_rms_50_150_error", "palm_rms_150_500_error",
    "palm_rms_500_1000_error", "palm_band_below_500_error",
    "palm_band_above_500_error", "palm_direction_interaction_error",
    "palm_track_loss_error", "palm_harmonic_residual_error",
    "dead_rms_0_30_error", "dead_rms_30_100_error",
    "dead_rms_100_250_error", "dead_rms_250_380_error",
    "dead_centroid_0_30_error", "dead_centroid_30_100_error",
    "dead_centroid_100_250_error", "dead_harmonicity_30_250_error",
    "dead_partial_decay_rmse", "dead_nonharmonic_residual_error",
    "rapid_envelope_shape_error", "rapid_first30_rms_displacement_error",
    "rapid_hit_drift_error", "dead_groove_inter_hit_residual_error",
    "dead_groove_transition_residual_error",
    "palm_open_pre_hit_residual_error", "palm_open_lift_attack_band_error",
    "palm_open_replant_attack_band_error",
)
FROZEN_ANALYSIS_SETTINGS = {
    "bootstrap_percentile_method": "r7",
    "bootstrap_replicates": 20000,
    "bootstrap_unit": "whole_listener_within_stratum",
    "core_cells": list(range(1, 11)),
    "dead_cells": [3, 4, 7, 8],
    "gate_thresholds": {
        "a_side_choice_rate": {"maximum": 0.65, "minimum": 0.35},
        "articulation_electry_preference_minimum": 0.4,
        "articulation_identification": {"maximum": 0.65, "minimum": 0.35},
        "cell_electry_preference_minimum": 0.3,
        "cell_identification": {"maximum": 0.7, "minimum": 0.3},
        "overall_electry_preference_lower_minimum": 0.4,
        "overall_identification_interval": {"maximum": 0.6, "minimum": 0.4},
        "priority_cell_identification": {"maximum": 0.65, "minimum": 0.35},
        "repeat_same_source_agreement_minimum": 0.7,
    },
    "hidden_repeat_cells": [5, 7, 9],
    "identification_interval_percentiles": [5, 95],
    "listener_strata": {
        "extended_range_guitarist": 15,
        "metal_producer": 15,
    },
    "max_replays": 3,
    "palm_cells": [1, 2, 5, 6, 9, 10],
    "participant_count": 30,
    "preference_lower_percentile": 5,
    "preference_tie_value": 0.5,
    "priority_cells": [5, 6, 9, 10],
    "protocol": ANALYSIS_SCHEMA,
    "stratum_weight": 0.5,
}
DEFECTS = {None, "attack", "pitched_body", "decay", "repetition", "noise", "other"}
SHA256_PATTERN = re.compile(r"[0-9a-fA-F]{64}\Z")
SOURCE_ID_PATTERN = re.compile(r"[A-Za-z0-9][A-Za-z0-9._-]{0,79}\Z")
UTC_PATTERN = re.compile(
    r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{3})?Z\Z")


class ScoreError(ValueError):
    pass


def _duplicates_forbidden(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ScoreError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _nonfinite_forbidden(value: str) -> None:
    raise ScoreError(f"non-finite JSON number: {value}")


def _parse_json(raw: bytes, label: object) -> dict[str, Any]:
    try:
        value = json.loads(raw.decode("utf-8"),
                           object_pairs_hook=_duplicates_forbidden,
                           parse_constant=_nonfinite_forbidden)
    except (UnicodeError, json.JSONDecodeError) as error:
        raise ScoreError(f"could not parse {label}: {error}") from error
    if not isinstance(value, dict):
        raise ScoreError(f"{label} must contain a JSON object")
    return value


def _load_json(path: Path) -> tuple[dict[str, Any], str]:
    try:
        raw = path.read_bytes()
    except OSError as error:
        raise ScoreError(f"could not read {path}: {error}") from error
    value = _parse_json(raw, path)
    return value, hashlib.sha256(raw).hexdigest()


def _exact_keys(value: Any, keys: set[str], label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ScoreError(f"{label} must be a JSON object")
    if set(value) != keys:
        missing = ", ".join(sorted(keys - set(value))) or "none"
        extra = ", ".join(sorted(set(value) - keys)) or "none"
        raise ScoreError(f"{label} keys differ (missing: {missing}; extra: {extra})")
    return value


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _checked_digest(path: Path, expected: Any, label: str) -> dict[str, str]:
    if not isinstance(expected, str) or not SHA256_PATTERN.fullmatch(expected):
        raise ScoreError(f"{label} has an invalid SHA-256")
    if path.is_symlink():
        raise ScoreError(f"{label} must not be a symlink")
    try:
        actual = _sha256(path)
    except OSError as error:
        raise ScoreError(f"could not read {label}: {error}") from error
    if actual != expected:
        raise ScoreError(f"{label} SHA-256 does not match the answer key")
    return {"path": str(path.resolve()), "sha256": actual}


def _checked_bytes(path: Path, expected: Any,
                   label: str) -> tuple[bytes, dict[str, str]]:
    if not isinstance(expected, str) or not SHA256_PATTERN.fullmatch(expected):
        raise ScoreError(f"{label} has an invalid SHA-256")
    if path.is_symlink():
        raise ScoreError(f"{label} must not be a symlink")
    try:
        raw = path.read_bytes()
    except OSError as error:
        raise ScoreError(f"could not read {label}: {error}") from error
    actual = hashlib.sha256(raw).hexdigest()
    if actual != expected:
        raise ScoreError(f"{label} SHA-256 does not match the answer key")
    return raw, {"path": str(path.resolve()), "sha256": actual}


def _checked_json(path: Path, expected: Any,
                  label: str) -> tuple[dict[str, Any], dict[str, str]]:
    raw, input_record = _checked_bytes(path, expected, label)
    value = _parse_json(raw, label)
    return value, input_record


def _normalized_sha256(value: Any, label: str) -> str:
    if not isinstance(value, str) or not SHA256_PATTERN.fullmatch(value):
        raise ScoreError(f"{label} has an invalid SHA-256")
    return value.lower()


def _engineering_capture_source(capture: dict[str, Any], source: dict[str, Any],
                                split: str, label: str) -> dict[str, Any]:
    if capture.get("schema") != "electry-mute-capture/v1":
        raise ScoreError(f"{label} has the wrong capture schema")
    session = capture.get("session")
    instrument = capture.get("instrument")
    takes = capture.get("takes")
    if (not isinstance(session, dict) or not isinstance(instrument, dict)
            or not isinstance(takes, list)
            or [take.get("file") if isinstance(take, dict) else None for take in takes]
            != list(CAPTURE_TAKE_FILES)):
        raise ScoreError(f"{label} is not a canonical capture manifest")
    if ({"split": session.get("split"), "session_id": session.get("session_id"),
         "player_id": session.get("player_id"),
         "guitar_id": instrument.get("guitar_id")}
            != {"split": split, "session_id": source.get("session_id"),
                "player_id": source.get("player_id"),
                "guitar_id": source.get("guitar_id")}):
        raise ScoreError(f"{label} identity differs from the comparison")
    take_hashes: dict[str, str] = {}
    take_bpms: dict[str, Any] = {}
    for take in takes:
        file_name = take["file"]
        spec = CAPTURE_TAKE_SPECS[file_name]
        expected_keys = {"file", "frames", "sha256", *spec}
        if spec["kind"] == "rapid":
            expected_keys.add("run_bpms_in_order")
        take = _exact_keys(take, expected_keys, f"{label} take {file_name}")
        if any(take[field] != expected for field, expected in spec.items()):
            raise ScoreError(f"{label} take {file_name} is not canonical")
        frames = take["frames"]
        minimum_frames = (407007 if spec["kind"] == "rapid"
                          else 352800 if file_name == "dead-e1-e2-groove.wav"
                          else 429975)
        if (type(frames) is not int
                or (frames != 1719900 if spec["kind"] == "isolated"
                    else frames < minimum_frames)):
            raise ScoreError(f"{label} take {file_name} has invalid frames")
        take_hashes[file_name] = _normalized_sha256(
            take.get("sha256"), f"{label} take {file_name}")
        bpms = take.get("run_bpms_in_order")
        if spec["kind"] == "rapid" and (
                not isinstance(bpms, list) or len(bpms) != 3
                or sorted(bpms) != [120, 180, 240]
                or any(type(value) is not int for value in bpms)):
            raise ScoreError(f"{label} take {file_name} has invalid rapid BPM runs")
        take_bpms[file_name] = bpms
    if len(set(take_hashes.values())) != len(CAPTURE_TAKE_FILES):
        raise ScoreError(f"{label} reuses a capture-take SHA-256")
    return {
        "session_id": source["session_id"],
        "take_bpms": take_bpms,
        "take_hashes": take_hashes,
    }


def _engineering_take_files(endpoint_id: str) -> tuple[str, ...]:
    if endpoint_id.startswith("rapid_"):
        return (
            "e1-palm-middle-rapid.wav", "e2-palm-middle-rapid.wav",
            "e1-dead-rapid.wav", "e2-dead-rapid.wav")
    if endpoint_id.startswith("dead_groove_"):
        return ("dead-e1-e2-groove.wav",)
    if endpoint_id.startswith("palm_open_"):
        return ("palm-open-e1-e2-groove.wav",)
    if endpoint_id.startswith("dead_"):
        return ("e1-open.wav", "e1-dead.wav", "e2-open.wav", "e2-dead.wav")
    return (
        "e1-open.wav", "e1-palm-near.wav", "e1-palm-middle.wav",
        "e1-palm-far.wav", "e2-open.wav", "e2-palm-near.wav",
        "e2-palm-middle.wav", "e2-palm-far.wav")


def _engineering_eligible_units(
        cluster_id: str, sources: list[dict[str, Any]], endpoint_id: str,
        method: str) -> list[dict[str, Any]]:
    sources = sorted(sources, key=lambda source: source["session_id"])
    take_files = _engineering_take_files(endpoint_id)
    if any(take_file not in source["take_hashes"]
           for source in sources for take_file in take_files):
        raise ScoreError(
            f"engineering endpoint {endpoint_id} lacks a canonical train take")

    def capture_unit(source: dict[str, Any], take_file: str) -> dict[str, Any]:
        return {
            "capture_take_file": take_file,
            "capture_take_sha256": source["take_hashes"][take_file],
            "session_id": source["session_id"],
        }

    if method == "balanced_three_versus_three_isolated_repetitions":
        partitions = [combination for combination in itertools.combinations(
            range(1, 7), 3) if 1 in combination]
        return [{
            "cluster_id": cluster_id,
            "half_a_repetition_ids": list(half_a),
            "half_b_repetition_ids": [
                value for value in range(1, 7) if value not in half_a],
            "input_units": [
                {**capture_unit(source, take_file), "stroke": stroke}
                for source in sources for take_file in take_files
                for stroke in ("down", "up")],
            "partition_id": f"partition-{index:02d}",
            "unit_index": index,
            "unit_kind": "isolated_3v3_partition_cluster_aggregate",
        } for index, half_a in enumerate(partitions, 1)]
    if method == "complete_groove_run_resampling":
        return [{
            "cluster_id": cluster_id,
            "input_units": [
                {**capture_unit(source, take_file), "run_index": run_index}
                for source in sources for take_file in take_files],
            "run_index": run_index,
            "unit_index": run_index,
            "unit_kind": "complete_groove_run_cluster_aggregate",
        } for run_index in range(1, 4)]
    if method == "detector_analysis_quantization_only":
        input_units = []
        for source in sources:
            for take_file in take_files:
                bpms = source["take_bpms"][take_file]
                if (not isinstance(bpms, list) or len(bpms) != 3
                        or sorted(bpms) != [120, 180, 240]):
                    raise ScoreError(
                        f"engineering endpoint {endpoint_id} lacks 120/180/240 runs")
                input_units.append({
                    **capture_unit(source, take_file),
                    "bpm": 180,
                    "run_index": bpms.index(180) + 1,
                })
        return [{
            "cluster_id": cluster_id,
            "input_units": input_units,
            "unit_index": 1,
            "unit_kind": "rapid_180_bpm_cluster_input_set",
        }]
    raise ScoreError(f"engineering endpoint {endpoint_id} has an unknown method")


def _expected_stratum(participant: str) -> str:
    return ("extended_range_guitarist" if participant in STRATA["extended_range_guitarist"]
            else "metal_producer")


def _bootstrap_seed(presentation_seed: str) -> str:
    digest = hashlib.sha256(bytes.fromhex(presentation_seed))
    digest.update(b"\0electry-listener-bootstrap/v1")
    return digest.hexdigest()


def _mapping_commitment(seed: str, mapping: dict[str, Any]) -> str:
    canonical = json.dumps(
        mapping, ensure_ascii=True, separators=(",", ":"), sort_keys=True).encode(
            "utf-8")
    digest = hashlib.sha256(bytes.fromhex(seed))
    digest.update(b"\0electry-private-mapping/v1\0")
    digest.update(canonical)
    return digest.hexdigest()


def _opaque(seed: str, *parts: object, length: int = 24) -> str:
    digest = hashlib.sha256(bytes.fromhex(seed))
    for part in parts:
        digest.update(b"\0")
        digest.update(str(part).encode("utf-8"))
    return digest.hexdigest()[:length]


def _physical_a_participants(seed: str, pair_id: str | int) -> set[int]:
    ordinal = (int(pair_id.removeprefix("practice-"))
               if isinstance(pair_id, str) else pair_id)
    physical_a: set[int] = set()
    for start, end, gets_extra in (
            (1, 15, ordinal % 2 == 1),
            (16, 30, ordinal % 2 == 0)):
        ranked = sorted(range(start, end + 1), key=lambda participant: _opaque(
            seed, "physical-a", pair_id, participant, length=64))
        physical_a.update(ranked[:8 if gets_extra else 7])
    return physical_a


def _scored_order(seed: str, participant: int) -> list[tuple[int, bool]]:
    tokens = [(cell, False) for cell in range(1, 11)]
    tokens += [(cell, True) for cell in REPEAT_CELLS]
    for attempt in range(10000):
        order = sorted(tokens, key=lambda item: _opaque(
            seed, "order", participant, attempt, item[0], int(item[1]), length=64))
        positions = {item: index for index, item in enumerate(order)}
        if all(positions[(cell, True)] - positions[(cell, False)] >= 4
               for cell in REPEAT_CELLS):
            return order
    raise ScoreError(f"could not regenerate scored order for {participant}")


def _validate_presentation(key: dict[str, Any]) -> None:
    algorithm = _exact_keys(
        key["algorithm"], {"hidden_repeats", "order", "side_balance"},
        "answer key algorithm")
    expected_algorithm = {
        "hidden_repeats": (
            "cells 5, 7 and 9; later; at least three intervening trials; A/B reversed"),
        "order": "SHA-256 rank with deterministic rejection",
        "side_balance": (
            "per pair and stratum, seeded SHA-256 ranks IDs; odd pairs assign "
            "8/7 physical=A and even pairs 7/8"),
    }
    if algorithm != expected_algorithm:
        raise ScoreError("answer key presentation algorithm is not the frozen contract")

    seed = key["presentation_seed"]
    physical_a = {
        pair: _physical_a_participants(seed, pair)
        for pair in ["practice-1", "practice-2", *range(1, 11)]
    }
    for participant_number, participant in enumerate(key["participants"], 1):
        participant_id = participant["participant_id"]
        if participant["session_token"] != _opaque(
                seed, "session", participant_number, length=32):
            raise ScoreError(f"{participant_id} session token differs from the frozen seed")

        expected_practice = sorted(
            ("practice-1", "practice-2"), key=lambda pair: _opaque(
                seed, "practice-order", participant_number, pair, length=64))
        for position, (record, pair) in enumerate(
                zip(participant["practice"], expected_practice), 1):
            physical_on_a = participant_number in physical_a[pair]
            expected_a = "physical" if physical_on_a else "electry"
            expected_b = "electry" if physical_on_a else "physical"
            expected_trial = _opaque(
                seed, "trial", participant_number, "practice", position,
                pair, length=20)
            if (record["pair"] != pair or record["a_source"] != expected_a
                    or record["b_source"] != expected_b
                    or record["trial_id"] != expected_trial):
                raise ScoreError(
                    f"{participant_id} practice presentation differs from the frozen seed")

        expected_scored = _scored_order(seed, participant_number)
        for position, (record, (cell, repeated)) in enumerate(
                zip(participant["trials"], expected_scored), 1):
            physical_on_a = participant_number in physical_a[cell]
            if repeated:
                physical_on_a = not physical_on_a
            expected_a = "physical" if physical_on_a else "electry"
            expected_b = "electry" if physical_on_a else "physical"
            variant = "repeat" if repeated else "original"
            expected_trial = _opaque(
                seed, "trial", participant_number, "scored", position,
                cell, variant, length=20)
            if (record["cell"] != cell
                    or record["repeat_of"] != (cell if repeated else None)
                    or record["a_source"] != expected_a
                    or record["b_source"] != expected_b
                    or record["trial_id"] != expected_trial):
                raise ScoreError(
                    f"{participant_id} scored presentation differs from the frozen seed")


def _archived_study(pack_root: Path, key: dict[str, Any]) -> tuple[
        dict[str, Any], dict[tuple[str | int, str], str], dict[str, str]]:
    descriptor = _exact_keys(
        key["source_manifest"], {"path", "sha256"},
        "answer key source_manifest")
    if descriptor["path"] != "private/study-manifest.json":
        raise ScoreError("answer key must reference private/study-manifest.json")
    path = pack_root / descriptor["path"]
    study, input_record = _checked_json(
        path, descriptor["sha256"], "archived study manifest")
    _exact_keys(study, {
        "cells", "comparison_manifest", "participant_count", "practice",
        "presentation_seed", "schema", "status", "study_id"
    }, "archived study manifest")
    if (study["schema"] != STUDY_SCHEMA
            or study["status"] != "frozen_ready_to_pack"
            or study["study_id"] != key["study_id"]
            or _normalized_sha256(
                study["presentation_seed"],
                "archived study presentation_seed") != key["presentation_seed"]
            or type(study["participant_count"]) is not int
            or study["participant_count"] != len(PARTICIPANTS)):
        raise ScoreError("archived study manifest does not match the answer key")

    comparison = _exact_keys(
        study["comparison_manifest"],
        {"path", "required_schema", "required_status", "sha256"},
        "archived study comparison_manifest")
    key_comparison = key["comparison_manifest"]
    if (comparison["required_schema"] != COMPARISON_SCHEMA
            or comparison["required_status"] != "frozen"
            or _normalized_sha256(
                comparison["sha256"],
                "archived study comparison SHA-256") != key_comparison["sha256"]):
        raise ScoreError(
            "archived study comparison hash/schema/status does not match the answer key")

    source_hashes: dict[tuple[str | int, str], str] = {}

    def read_pairs(values: Any, expected_ids: list[str | int], label: str) -> None:
        if not isinstance(values, list) or len(values) != len(expected_ids):
            raise ScoreError(f"archived study {label} layout is invalid")
        for index, (value, expected_id) in enumerate(zip(values, expected_ids)):
            pair = _exact_keys(
                value, {"electry", "id", "physical"},
                f"archived study {label}[{index}]")
            if type(pair["id"]) is not type(expected_id) or pair["id"] != expected_id:
                raise ScoreError(f"archived study {label}[{index}] has the wrong ID")
            for source in ("physical", "electry"):
                item = _exact_keys(
                    pair[source], {"path", "sha256"},
                    f"archived study {label}[{index}].{source}")
                if not isinstance(item["path"], str) or not item["path"]:
                    raise ScoreError(
                        f"archived study {label}[{index}].{source} is invalid")
                source_hashes[(expected_id, source)] = _normalized_sha256(
                    item["sha256"],
                    f"archived study {label}[{index}].{source}")

    read_pairs(study["practice"], ["practice-1", "practice-2"], "practice")
    read_pairs(study["cells"], list(range(1, 11)), "cells")
    return study, source_hashes, input_record


def _validate_key_sources(key: dict[str, Any],
                          source_hashes: dict[tuple[str | int, str], str]) -> None:
    values = key["sources"]
    if not isinstance(values, list) or len(values) != len(source_hashes):
        raise ScoreError("answer key sources must contain exactly 24 source records")
    seen: set[tuple[str | int, str]] = set()
    frames_by_pair: dict[str | int, dict[str, int]] = {}
    for index, value in enumerate(values):
        item = _exact_keys(
            value, {"frames", "pair", "path", "sha256", "source"},
            f"answer key source {index}")
        pair = item["pair"]
        source = item["source"]
        valid_pair = ((type(pair) is int and pair in CORE_FRAMES)
                      or (isinstance(pair, str)
                          and pair in {"practice-1", "practice-2"}))
        identity = ((pair, source) if valid_pair and isinstance(source, str)
                    else None)
        if (identity is None or identity not in source_hashes or identity in seen
                or item["sha256"] != source_hashes.get(identity)
                or not isinstance(item["path"], str) or not item["path"]
                or type(item["frames"]) is not int or item["frames"] <= 0):
            raise ScoreError(f"answer key source {index} does not match the archived study")
        seen.add(identity)
        frames_by_pair.setdefault(pair, {})[source] = item["frames"]
    if seen != set(source_hashes):
        raise ScoreError("answer key sources do not cover the archived study")
    for pair, frames in frames_by_pair.items():
        if (set(frames) != {"physical", "electry"}
                or frames["physical"] != frames["electry"]
                or (type(pair) is int and frames["physical"] != CORE_FRAMES.get(pair))):
            raise ScoreError(f"answer key source frames are invalid for pair {pair}")


def _validate_participant_source_hashes(
        key: dict[str, Any],
        source_hashes: dict[tuple[str | int, str], str]) -> None:
    for participant in key["participants"]:
        for record in participant["practice"]:
            for side in ("a", "b"):
                expected = source_hashes[(record["pair"], record[f"{side}_source"])]
                if record[f"{side}_sha256"] != expected:
                    raise ScoreError(
                        f"{participant['participant_id']} practice source hash differs "
                        "from the archived study")
        for record in participant["trials"]:
            for side in ("a", "b"):
                expected = source_hashes[(record["cell"], record[f"{side}_source"])]
                if record[f"{side}_sha256"] != expected:
                    raise ScoreError(
                        f"{participant['participant_id']} scored source hash differs "
                        "from the archived study")


def _archived_comparison(pack_root: Path, key: dict[str, Any],
                         source_hashes: dict[tuple[str | int, str], str]) -> tuple[
                             dict[str, Any], dict[str, str]]:
    descriptor = _exact_keys(
        key["comparison_manifest"], {"path", "schema", "sha256", "status"},
        "answer key comparison_manifest")
    if (descriptor["path"] != "private/comparison-manifest.json"
            or descriptor["schema"] != COMPARISON_SCHEMA
            or descriptor["status"] != "frozen"):
        raise ScoreError("answer key does not reference a frozen comparison v1 manifest")
    path = pack_root / descriptor["path"]
    comparison, input_record = _checked_json(
        path, descriptor["sha256"], "archived comparison manifest")
    if (comparison.get("schema") != COMPARISON_SCHEMA
            or comparison.get("status") != "frozen"
            or comparison.get("study_id") != key["study_id"]
            or _normalized_sha256(
                comparison.get("presentation_seed"),
                "archived comparison presentation_seed") != key["presentation_seed"]
            or type(comparison.get("participant_count")) is not int
            or comparison.get("participant_count") != len(PARTICIPANTS)):
        raise ScoreError("archived comparison manifest does not match the answer key")

    for label, expected_ids in (
            ("practice", ["practice-1", "practice-2"]),
            ("cells", list(range(1, 11)))):
        values = comparison.get(label)
        if not isinstance(values, list) or len(values) != len(expected_ids):
            raise ScoreError(f"archived comparison {label} layout is invalid")
        for index, (item, pair) in enumerate(zip(values, expected_ids)):
            if (not isinstance(item, dict)
                    or type(item.get("id")) is not type(pair)
                    or item.get("id") != pair
                    or _normalized_sha256(
                        item.get("physical_sha256"),
                        f"archived comparison {label}[{index}].physical_sha256")
                    != source_hashes[(pair, "physical")]
                    or _normalized_sha256(
                        item.get("electry_sha256"),
                        f"archived comparison {label}[{index}].electry_sha256")
                    != source_hashes[(pair, "electry")]):
                raise ScoreError(
                    f"archived comparison {label}[{index}] source hashes do not match")
    try:
        analysis_value = comparison["freezes"]["analysis"]
    except (KeyError, TypeError) as error:
        raise ScoreError("archived comparison has no frozen listener scorer") from error
    analysis_value = _exact_keys(
        analysis_value,
        {"implementation_sha256", "listener_scorer_sha256", "settings",
         "settings_sha256"},
        "archived comparison analysis freeze")
    canonical_settings = json.dumps(
        analysis_value["settings"], ensure_ascii=True, separators=(",", ":"),
        sort_keys=True).encode("utf-8")
    if (analysis_value["settings"] != FROZEN_ANALYSIS_SETTINGS
            or _normalized_sha256(
                analysis_value["settings_sha256"],
                "archived comparison analysis settings_sha256")
            != hashlib.sha256(canonical_settings).hexdigest()):
        raise ScoreError("archived comparison analysis settings differ from this scorer")
    listener_scorer = analysis_value["listener_scorer_sha256"]
    if (_normalized_sha256(
            listener_scorer, "archived comparison listener_scorer_sha256")
            != key["implementation"]["scorer_sha256"]):
        raise ScoreError("archived comparison listener scorer does not match the answer key")
    return comparison, input_record


def _evidence_archives(pack_root: Path, key: dict[str, Any],
                       comparison: dict[str, Any]) -> dict[str, Any]:
    archives = _exact_keys(
        key["archives"],
        {"artifact_registry", "capture_manifests",
         "engineering_derivation_receipt", "engineering_derivation_result",
         "engineering_train_analysis", "event_records", "preparer",
         "selection_receipt"},
        "answer key archives")

    def descriptor(value: Any, expected_path: str, expected_sha256: str,
                   label: str) -> Path:
        item = _exact_keys(value, {"path", "sha256"}, label)
        if (item["path"] != expected_path
                or item["sha256"] != expected_sha256):
            raise ScoreError(f"{label} does not match the frozen comparison")
        return pack_root / expected_path

    raw_registry = _exact_keys(
        comparison.get("artifact_registry"),
        {"path", "required_schema", "required_status", "sha256"},
        "archived comparison artifact_registry")
    registry_sha256 = _normalized_sha256(
        raw_registry["sha256"], "archived comparison artifact_registry")
    registry_path = descriptor(
        archives["artifact_registry"], "private/artifact-registry.json",
        registry_sha256, "archived artifact registry")
    registry, registry_input = _checked_json(
        registry_path, registry_sha256, "archived artifact registry")
    registry = _exact_keys(
        registry, {"artifacts", "schema", "status"},
        "archived artifact registry")
    if (registry["schema"] != raw_registry["required_schema"]
            or registry["status"] != raw_registry["required_status"]):
        raise ScoreError("archived artifact registry has the wrong schema/status")
    artifact_values = registry["artifacts"]
    if not isinstance(artifact_values, list) or not artifact_values:
        raise ScoreError("archived artifact registry is empty")
    registry_ids: set[str] = set()
    registry_hashes: set[str] = set()
    for index, value in enumerate(artifact_values):
        artifact = _exact_keys(
            value, {"id", "sha256"}, f"archived artifact registry item {index}")
        artifact_id = artifact["id"]
        artifact_hash = _normalized_sha256(
            artifact["sha256"], f"archived artifact registry item {index}")
        if (not isinstance(artifact_id, str)
                or not SOURCE_ID_PATTERN.fullmatch(artifact_id)
                or artifact_id in registry_ids or artifact_hash in registry_hashes):
            raise ScoreError(f"archived artifact registry item {index} is duplicated")
        registry_ids.add(artifact_id)
        registry_hashes.add(artifact_hash)

    try:
        raw_receipt = comparison["freezes"]["selection"]["receipt"]
        selection_seed = comparison["freezes"]["selection"]["seed"]
    except (KeyError, TypeError) as error:
        raise ScoreError("archived comparison has no frozen selection receipt") from error
    raw_receipt = _exact_keys(
        raw_receipt, {"path", "required_schema", "required_status", "sha256"},
        "archived comparison selection receipt")
    receipt_sha256 = _normalized_sha256(
        raw_receipt["sha256"], "archived comparison selection receipt")
    receipt_path = descriptor(
        archives["selection_receipt"], "private/selection-receipt.json",
        receipt_sha256, "archived selection receipt")
    receipt, receipt_input = _checked_json(
        receipt_path, receipt_sha256, "archived selection receipt")
    if (receipt.get("schema") != raw_receipt["required_schema"]
            or receipt.get("status") != raw_receipt["required_status"]
            or receipt.get("study_id") != key["study_id"]
            or _normalized_sha256(
                receipt.get("selection_seed"),
                "archived selection receipt seed")
            != _normalized_sha256(selection_seed, "archived comparison selection seed")):
        raise ScoreError("archived selection receipt does not match the comparison")

    preparer_path = descriptor(
        archives["preparer"], "private/prepare.py",
        key["implementation"]["preparer_sha256"], "archived preparer")
    preparer = _checked_digest(
        preparer_path, key["implementation"]["preparer_sha256"],
        "archived preparer")

    cohort = _exact_keys(
        comparison.get("source_cohort"),
        {"engineering_train_clusters", "holdout_cluster_count",
         "holdout_clusters", "practice_train_session_ids"},
        "archived comparison source cohort")
    holdout_clusters = cohort["holdout_clusters"]
    train_clusters = cohort["engineering_train_clusters"]
    practice_session_ids = cohort["practice_train_session_ids"]
    if (type(cohort["holdout_cluster_count"]) is not int
            or cohort["holdout_cluster_count"] != 2
            or not isinstance(holdout_clusters, list)
            or len(holdout_clusters) != 2
            or not isinstance(train_clusters, list)
            or len(train_clusters) < 3
            or not isinstance(practice_session_ids, list)
            or len(practice_session_ids) != 2
            or any(not isinstance(value, str)
                   or not SOURCE_ID_PATTERN.fullmatch(value)
                   for value in practice_session_ids)
            or len(set(practice_session_ids)) != 2):
        raise ScoreError("archived comparison source cohort is invalid")
    raw_sessions: list[tuple[str, Any]] = []
    train_session_ids: set[str] = set()
    for split, clusters in (("holdout", holdout_clusters),
                            ("engineering train", train_clusters)):
        for cluster in clusters:
            if (not isinstance(cluster, dict)
                    or not isinstance(cluster.get("sessions"), list)
                    or not cluster["sessions"]):
                raise ScoreError(
                    f"archived comparison {split} cluster is invalid")
            raw_sessions.extend((split, session) for session in cluster["sessions"])
            if split == "engineering train":
                for session in cluster["sessions"]:
                    if isinstance(session, dict) and isinstance(
                            session.get("session_id"), str):
                        train_session_ids.add(session["session_id"])
    if not set(practice_session_ids) <= train_session_ids:
        raise ScoreError(
            "archived comparison practice sessions are not in the train cohort")
    expected_captures: dict[str, str] = {}
    source_contracts: dict[str, tuple[str, dict[str, Any]]] = {}
    for index, (split_label, session) in enumerate(raw_sessions):
        if (not isinstance(session, dict)
                or not isinstance(session.get("session_id"), str)
                or not SOURCE_ID_PATTERN.fullmatch(session["session_id"])):
            raise ScoreError(f"archived comparison source session {index} is invalid")
        capture = _exact_keys(
            session.get("capture_manifest"), {"path", "sha256"},
            f"archived comparison source session {index} capture_manifest")
        session_id = session["session_id"]
        if session_id in expected_captures:
            raise ScoreError(f"archived comparison repeats source session {session_id}")
        expected_captures[session_id] = _normalized_sha256(
            capture["sha256"],
            f"archived comparison source session {index} capture_manifest")
        source_contracts[session_id] = (
            "holdout" if split_label == "holdout" else "train", session)

    capture_values = archives["capture_manifests"]
    if not isinstance(capture_values, list) or len(capture_values) != len(expected_captures):
        raise ScoreError("answer key capture archives are incomplete")
    capture_inputs: list[dict[str, str]] = []
    capture_sources: dict[str, dict[str, Any]] = {}
    all_capture_take_hashes: set[str] = set()
    seen_sessions: set[str] = set()
    for index, value in enumerate(capture_values):
        item = _exact_keys(
            value, {"path", "session_id", "sha256"},
            f"answer key capture archive {index}")
        session_id = item["session_id"]
        expected_path = f"private/capture-manifests/{session_id}.json"
        if (not isinstance(session_id, str) or session_id not in expected_captures
                or session_id in seen_sessions or item["path"] != expected_path
                or item["sha256"] != expected_captures.get(session_id)):
            raise ScoreError(f"answer key capture archive {index} is invalid")
        seen_sessions.add(session_id)
        capture, input_record = _checked_json(
            pack_root / expected_path, item["sha256"],
            f"archived capture manifest {session_id}")
        split, source_contract = source_contracts[session_id]
        capture_source = _engineering_capture_source(
            capture, source_contract, split,
            f"archived capture manifest {session_id}")
        take_hashes = set(capture_source["take_hashes"].values())
        if take_hashes & all_capture_take_hashes:
            raise ScoreError("archived capture manifests reuse a take WAV SHA-256")
        all_capture_take_hashes.update(take_hashes)
        capture_sources[session_id] = capture_source
        capture_inputs.append({"session_id": session_id, **input_record})
    if seen_sessions != set(expected_captures):
        raise ScoreError("answer key capture archives do not cover the source cohort")

    try:
        engineering = comparison["engineering_freeze"]
        raw_derivation_receipt = engineering["derivation_receipt"]
    except (KeyError, TypeError) as error:
        raise ScoreError(
            "archived comparison has no engineering derivation receipt") from error
    engineering_contract = {
        name: value for name, value in engineering.items()
        if name not in {"derivation_receipt", "engineering_contract_sha256"}
    }
    try:
        engineering_contract_bytes = json.dumps(
            engineering_contract, ensure_ascii=True, separators=(",", ":"),
            sort_keys=True, allow_nan=False).encode("utf-8")
    except (TypeError, ValueError) as error:
        raise ScoreError("archived comparison engineering contract is invalid") from error
    engineering_contract_sha256 = hashlib.sha256(
        engineering_contract_bytes).hexdigest()
    if (_normalized_sha256(
            engineering.get("engineering_contract_sha256"),
            "archived comparison engineering contract")
            != engineering_contract_sha256):
        raise ScoreError("archived comparison engineering contract SHA-256 is stale")
    raw_derivation_receipt = _exact_keys(
        raw_derivation_receipt,
        {"path", "required_schema", "required_status", "sha256"},
        "archived comparison engineering derivation receipt")
    if (raw_derivation_receipt["required_schema"]
            != "electry-engineering-derivation-receipt/v1"
            or raw_derivation_receipt["required_status"] != "frozen"):
        raise ScoreError(
            "archived comparison requires the wrong engineering derivation receipt")
    derivation_receipt_sha256 = _normalized_sha256(
        raw_derivation_receipt["sha256"],
        "archived comparison engineering derivation receipt")
    derivation_receipt_path = descriptor(
        archives["engineering_derivation_receipt"],
        "private/engineering-derivation-receipt.json",
        derivation_receipt_sha256, "archived engineering derivation receipt")
    derivation_receipt, derivation_receipt_input = _checked_json(
        derivation_receipt_path, derivation_receipt_sha256,
        "archived engineering derivation receipt")
    derivation_receipt = _exact_keys(
        derivation_receipt,
        {"derivation_result", "schema", "status", "study_id"},
        "archived engineering derivation receipt")
    if (derivation_receipt["schema"]
            != raw_derivation_receipt["required_schema"]
            or derivation_receipt["status"]
            != raw_derivation_receipt["required_status"]
            or derivation_receipt["study_id"] != key["study_id"]):
        raise ScoreError(
            "archived engineering derivation receipt does not match the comparison")

    raw_derivation_result = _exact_keys(
        derivation_receipt["derivation_result"],
        {"path", "required_schema", "required_status", "sha256"},
        "archived engineering derivation result descriptor")
    if (raw_derivation_result["required_schema"]
            != "electry-engineering-derivation-result/v1"
            or raw_derivation_result["required_status"] != "frozen"):
        raise ScoreError(
            "archived derivation receipt requires the wrong engineering result")
    derivation_result_sha256 = _normalized_sha256(
        raw_derivation_result["sha256"],
        "archived engineering derivation result")
    derivation_result_path = descriptor(
        archives["engineering_derivation_result"],
        "private/engineering-derivation-result.json",
        derivation_result_sha256, "archived engineering derivation result")
    derivation_result, derivation_result_input = _checked_json(
        derivation_result_path, derivation_result_sha256,
        "archived engineering derivation result")
    derivation_result = _exact_keys(
        derivation_result,
        {"artifacts", "candidate_contour_rmse_reduction_min", "depth_mapping",
         "endpoints", "engineering_contract_sha256", "exclusions", "failure_rule",
         "percentile_method", "repeatability_percentile", "schema", "status", "study_id",
         "train_clusters"},
        "archived engineering derivation result")
    if (derivation_result["schema"] != raw_derivation_result["required_schema"]
            or derivation_result["status"]
            != raw_derivation_result["required_status"]
            or derivation_result["study_id"] != key["study_id"]
            or derivation_result["depth_mapping"] != engineering.get("depth_mapping")
            or derivation_result["candidate_contour_rmse_reduction_min"]
            != engineering.get("candidate_contour_rmse_reduction_min")
            or derivation_result["repeatability_percentile"] != 0.90
            or derivation_result["percentile_method"]
            != "r7_linear_interpolation"
            or derivation_result["failure_rule"]
            != "missing_train_derivation_or_sample_fails"
            or derivation_result["exclusions"] != []
            or _normalized_sha256(
                derivation_result["engineering_contract_sha256"],
                "archived engineering derivation contract")
            != engineering_contract_sha256):
        raise ScoreError(
            "archived engineering derivation result does not match the comparison")
    artifact_fields = (
        "analyzer_sha256", "baseline_build_sha256", "candidate_build_sha256",
        "evaluator_sha256", "event_generator_sha256")
    result_artifacts = _exact_keys(
        derivation_result["artifacts"], set(artifact_fields),
        "archived engineering derivation artifacts")
    if any(_normalized_sha256(
            result_artifacts[field],
            f"archived engineering derivation artifacts.{field}")
           != _normalized_sha256(
               engineering.get(field),
               f"archived comparison engineering_freeze.{field}")
           for field in artifact_fields):
        raise ScoreError(
            "archived engineering derivation artifacts differ from the comparison")

    expected_train_clusters: dict[str, list[dict[str, str]]] = {}
    train_cluster_sources: dict[str, list[dict[str, Any]]] = {}
    for index, cluster in enumerate(train_clusters):
        if not isinstance(cluster, dict):
            raise ScoreError(
                f"archived comparison engineering train cluster {index} is invalid")
        cluster_id = cluster.get("cluster_id")
        if (not isinstance(cluster_id, str)
                or not SOURCE_ID_PATTERN.fullmatch(cluster_id)
                or cluster_id in expected_train_clusters):
            raise ScoreError(
                f"archived comparison engineering train cluster {index} has an invalid ID")
        expected_train_clusters[cluster_id] = sorted(
            ({"capture_manifest_sha256": expected_captures[session["session_id"]],
              "session_id": session["session_id"]}
             for session in cluster["sessions"]),
            key=lambda value: value["session_id"])
        train_cluster_sources[cluster_id] = [
            capture_sources[session["session_id"]]
            for session in cluster["sessions"]]

    result_cluster_values = derivation_result["train_clusters"]
    if (not isinstance(result_cluster_values, list)
            or len(result_cluster_values) != len(expected_train_clusters)):
        raise ScoreError(
            "archived engineering derivation result has incomplete train clusters")
    result_analysis: dict[str, dict[str, Any]] = {}
    for index, value in enumerate(result_cluster_values):
        record = _exact_keys(
            value, {"analysis_result", "cluster_id", "sessions"},
            f"archived engineering derivation train cluster {index}")
        expected_cluster_id = sorted(expected_train_clusters)[index]
        if (record["cluster_id"] != expected_cluster_id
                or record["sessions"] != expected_train_clusters[expected_cluster_id]):
            raise ScoreError(
                f"archived engineering derivation train cluster {index} differs")
        analysis_descriptor = _exact_keys(
            record["analysis_result"],
            {"path", "required_schema", "required_status", "sha256"},
            f"archived engineering train analysis {expected_cluster_id}")
        if (analysis_descriptor["required_schema"]
                != "electry-engineering-train-analysis/v1"
                or analysis_descriptor["required_status"] != "frozen"):
            raise ScoreError(
                f"archived engineering train analysis {expected_cluster_id} has "
                "the wrong schema/status")
        result_analysis[expected_cluster_id] = analysis_descriptor

    engineering_endpoint_values = engineering.get("endpoints")
    if (not isinstance(engineering_endpoint_values, list)
            or [endpoint.get("id") if isinstance(endpoint, dict) else None
                for endpoint in engineering_endpoint_values]
            != list(ENGINEERING_ENDPOINT_IDS)):
        raise ScoreError("archived comparison engineering endpoints are invalid")
    endpoint_contracts: list[tuple[str, dict[str, Any], str]] = []
    for index, endpoint in enumerate(engineering_endpoint_values):
        if not isinstance(endpoint, dict):
            raise ScoreError(f"archived comparison engineering endpoint {index} is invalid")
        endpoint_id = endpoint.get("id")
        margin = _exact_keys(
            endpoint.get("no_regression_margin"),
            {"detector_analysis_quantization", "formula", "method",
             "train_repeatability_p90", "value"},
            f"archived comparison engineering endpoint {index} margin")
        method = margin["method"]
        expected_method = (
            "detector_analysis_quantization_only"
            if isinstance(endpoint_id, str) and endpoint_id.startswith("rapid_")
            else "complete_groove_run_resampling"
            if isinstance(endpoint_id, str)
            and endpoint_id.startswith(("dead_groove_", "palm_open_"))
            else "balanced_three_versus_three_isolated_repetitions")
        if (not isinstance(endpoint_id, str)
                or not SOURCE_ID_PATTERN.fullmatch(endpoint_id)
                or method != expected_method
                or any(previous[0] == endpoint_id
                       for previous in endpoint_contracts)):
            raise ScoreError(f"archived comparison engineering endpoint {index} is invalid")
        endpoint_contracts.append((endpoint_id, margin, method))

    analysis_values = archives["engineering_train_analysis"]
    if (not isinstance(analysis_values, list)
            or len(analysis_values) != len(result_analysis)):
        raise ScoreError("answer key engineering train analysis archives are incomplete")
    analysis_inputs: list[dict[str, str]] = []
    analysis_samples: dict[str, dict[str, list[Any]]] = {}
    seen_analysis: set[str] = set()
    for index, value in enumerate(analysis_values):
        item = _exact_keys(
            value, {"cluster_id", "path", "sha256"},
            f"answer key engineering train analysis archive {index}")
        cluster_id = item["cluster_id"]
        raw_analysis = result_analysis.get(cluster_id)
        expected_path = f"private/engineering-train-analysis/{cluster_id}.json"
        expected_analysis_sha256 = (
            _normalized_sha256(
                raw_analysis["sha256"],
                f"archived engineering train analysis {cluster_id}")
            if raw_analysis is not None else None)
        if (not isinstance(cluster_id, str) or raw_analysis is None
                or cluster_id in seen_analysis or item["path"] != expected_path
                or item["sha256"] != expected_analysis_sha256):
            raise ScoreError(
                f"answer key engineering train analysis archive {index} is invalid")
        seen_analysis.add(cluster_id)
        analysis, input_record = _checked_json(
            pack_root / expected_path, item["sha256"],
            f"archived engineering train analysis {cluster_id}")
        analysis = _exact_keys(
            analysis,
            {"cluster_id", "depth_mapping", "endpoints", "exclusions",
             "failure_flags", "passed", "schema", "sessions", "status",
             "study_id"},
            f"archived engineering train analysis {cluster_id}")
        if (analysis["schema"] != raw_analysis["required_schema"]
                or analysis["status"] != raw_analysis["required_status"]
                or analysis["study_id"] != key["study_id"]
                or analysis["cluster_id"] != cluster_id
                or analysis["sessions"] != expected_train_clusters[cluster_id]
                or analysis["depth_mapping"] != engineering.get("depth_mapping")
                or analysis["passed"] is not True
                or analysis["failure_flags"] != []
                or analysis["exclusions"] != []):
            raise ScoreError(
                f"archived engineering train analysis {cluster_id} differs")
        endpoint_values = analysis["endpoints"]
        if (not isinstance(endpoint_values, list)
                or len(endpoint_values) != len(endpoint_contracts)):
            raise ScoreError(
                f"archived engineering train analysis {cluster_id} endpoints are incomplete")
        cluster_samples: dict[str, list[Any]] = {}
        for endpoint_index, (endpoint_value, contract) in enumerate(
                zip(endpoint_values, endpoint_contracts)):
            endpoint_id, _, method = contract
            raw_endpoint = _exact_keys(
                endpoint_value,
                {"eligible_input_units", "excluded_input_units", "id",
                 "repeatability_samples"},
                f"archived engineering train analysis {cluster_id} endpoint "
                f"{endpoint_index}")
            eligible_units = raw_endpoint["eligible_input_units"]
            samples = raw_endpoint["repeatability_samples"]
            expected_units = _engineering_eligible_units(
                cluster_id, train_cluster_sources[cluster_id], endpoint_id, method)
            if (raw_endpoint["id"] != endpoint_id
                    or raw_endpoint["excluded_input_units"] != []
                    or eligible_units != expected_units
                    or not isinstance(samples, list)
                    or (samples != [] if method == "detector_analysis_quantization_only"
                        else len(samples) != len(expected_units))):
                raise ScoreError(
                    f"archived engineering train analysis {cluster_id} endpoint "
                    f"{endpoint_id} differs from canonical capture provenance")
            for unit_index, unit in enumerate(expected_units, 1):
                if method != "detector_analysis_quantization_only":
                    sample = _exact_keys(
                        samples[unit_index - 1], {*unit, "value"},
                        f"archived engineering train analysis {cluster_id} endpoint "
                        f"{endpoint_id} sample {unit_index}")
                    if ({name: sample[name] for name in unit} != unit
                            or type(sample["value"]) not in (int, float)
                            or not math.isfinite(float(sample["value"]))
                            or sample["value"] < 0):
                        raise ScoreError(
                            f"archived engineering train analysis {cluster_id} endpoint "
                            f"{endpoint_id} sample {unit_index} is invalid")
            cluster_samples[endpoint_id] = samples
        analysis_samples[cluster_id] = cluster_samples
        analysis_inputs.append({"cluster_id": cluster_id, **input_record})
    if seen_analysis != set(result_analysis):
        raise ScoreError(
            "answer key engineering train analysis archives do not cover the result")
    analysis_hashes = {
        _normalized_sha256(value["sha256"],
                           f"archived engineering train analysis {cluster_id}")
        for cluster_id, value in result_analysis.items()
    }
    try:
        freezes = comparison["freezes"]
        chain_assets = freezes["chain"]["assets"]
        required_artifact_values = [
            freezes[section]["implementation_sha256"]
            for section in ("selection", "render", "chain", "analysis")
        ]
        required_artifact_values.extend((
            freezes["analysis"]["listener_scorer_sha256"],
            freezes["chain"]["preset_sha256"],
            *(value["sha256"] for value in chain_assets),
            *(result_artifacts[field] for field in artifact_fields),
            *analysis_hashes,
        ))
    except (KeyError, TypeError) as error:
        raise ScoreError("archived comparison frozen artifact list is invalid") from error
    if not isinstance(chain_assets, list):
        raise ScoreError("archived comparison chain assets are invalid")
    required_artifact_hashes = {
        _normalized_sha256(value, "archived comparison frozen artifact")
        for value in required_artifact_values
    }
    if not required_artifact_hashes <= registry_hashes:
        raise ScoreError(
            "artifact registry omits a frozen implementation/build/asset SHA-256")

    result_endpoint_values = derivation_result["endpoints"]
    if (not isinstance(result_endpoint_values, list)
            or len(result_endpoint_values) != len(endpoint_contracts)):
        raise ScoreError("archived engineering derivation endpoints are incomplete")
    for index, (value, contract) in enumerate(
            zip(result_endpoint_values, endpoint_contracts)):
        endpoint_id, margin, method = contract
        endpoint = _exact_keys(
            value,
            {"detector_analysis_quantization", "formula", "id", "method",
             "repeatability_input_summary", "train_repeatability_p90",
             "train_repeatability_samples", "value"},
            f"archived engineering derivation endpoint {index}")
        samples = [
            sample
            for cluster_id in sorted(analysis_samples)
            for sample in analysis_samples[cluster_id][endpoint_id]
        ]
        expected_summary = {
            "cluster_ids": ([] if method == "detector_analysis_quantization_only"
                            else sorted(expected_train_clusters)),
            "sample_count": len(samples),
            "unit": (
                "none_no_within_session_same_tempo_repeat"
                if method == "detector_analysis_quantization_only"
                else "balanced_three_versus_three_halves"
                if method == "balanced_three_versus_three_isolated_repetitions"
                else "complete_groove_runs"),
        }
        quantization = margin["detector_analysis_quantization"]
        if (type(quantization) not in (int, float)
                or not math.isfinite(float(quantization)) or quantization < 0):
            raise ScoreError(
                f"archived engineering derivation endpoint {endpoint_id} "
                "has invalid quantization")
        if any(type(margin[field]) not in (int, float)
               or not math.isfinite(float(margin[field])) or margin[field] < 0
               for field in ("train_repeatability_p90", "value")):
            raise ScoreError(
                f"archived engineering derivation endpoint {endpoint_id} "
                "has an invalid margin")
        values = [sample["value"] for sample in samples]
        expected_p90 = (0.0 if method == "detector_analysis_quantization_only"
                        else _percentile(values, 0.90))
        expected_formula = (
            "detector_analysis_quantization"
            if method == "detector_analysis_quantization_only"
            else "max(detector_analysis_quantization,train_repeatability_p90)")
        expected_value = (quantization if method == "detector_analysis_quantization_only"
                          else max(quantization, expected_p90))
        if (endpoint["id"] != endpoint_id
                or margin["train_repeatability_p90"] != expected_p90
                or margin["formula"] != expected_formula
                or margin["value"] != expected_value
                or any(endpoint[field] != margin[field] for field in (
                    "detector_analysis_quantization", "formula", "method",
                    "train_repeatability_p90", "value"))
                or endpoint["train_repeatability_samples"] != samples
                or endpoint["repeatability_input_summary"] != expected_summary):
            raise ScoreError(
                f"archived engineering derivation endpoint {endpoint_id} differs")

    expected_events: set[str] = set()
    for label in ("practice", "cells"):
        values = comparison.get(label)
        if not isinstance(values, list):
            raise ScoreError(f"archived comparison {label} is invalid")
        for index, value in enumerate(values):
            try:
                event = value["provenance"]["electry_event_or_score"]
            except (KeyError, TypeError) as error:
                raise ScoreError(
                    f"archived comparison {label}[{index}] has no event record") from error
            event = _exact_keys(
                event, {"path", "sha256"},
                f"archived comparison {label}[{index}] event record")
            expected_events.add(_normalized_sha256(
                event["sha256"],
                f"archived comparison {label}[{index}] event record"))

    event_values = archives["event_records"]
    if not isinstance(event_values, list) or len(event_values) != len(expected_events):
        raise ScoreError("answer key event archives are incomplete")
    event_inputs: list[dict[str, str]] = []
    seen_events: set[str] = set()
    for index, value in enumerate(event_values):
        item = _exact_keys(
            value, {"path", "sha256"}, f"answer key event archive {index}")
        sha256 = item["sha256"]
        expected_path = f"private/event-scores/{sha256}.bin"
        if (not isinstance(sha256, str) or sha256 not in expected_events
                or sha256 in seen_events or item["path"] != expected_path):
            raise ScoreError(f"answer key event archive {index} is invalid")
        seen_events.add(sha256)
        event_path = descriptor(
            item, expected_path, sha256, f"answer key event archive {index}")
        event_inputs.append(_checked_digest(
            event_path, sha256, f"archived event record {sha256}"))
    if seen_events != expected_events:
        raise ScoreError("answer key event archives do not cover the comparison")

    return {
        "artifact_registry": registry_input,
        "capture_manifests": sorted(
            capture_inputs, key=lambda value: value["session_id"]),
        "event_records": sorted(event_inputs, key=lambda value: value["sha256"]),
        "engineering_derivation_receipt": derivation_receipt_input,
        "engineering_derivation_result": derivation_result_input,
        "engineering_train_analysis": sorted(
            analysis_inputs, key=lambda value: value["cluster_id"]),
        "preparer": preparer,
        "selection_receipt": receipt_input,
    }


def _implementation_files(pack_root: Path, key: dict[str, Any]) -> dict[str, dict[str, str]]:
    implementation = _exact_keys(
        key["implementation"],
        {"preparer_sha256", "runner_sha256", "scorer_sha256", "server_sha256"},
        "answer key implementation")
    paths = {
        "preparer": (pack_root / "private" / "prepare.py", "preparer_sha256"),
        "runner": (pack_root / "public" / "index.html", "runner_sha256"),
        "scorer": (pack_root / "score.py", "scorer_sha256"),
        "server": (pack_root / "serve.py", "server_sha256"),
    }
    inputs = {
        name: _checked_digest(path, implementation[field], f"archived {name}")
        for name, (path, field) in paths.items()
    }
    if _sha256(Path(__file__).resolve()) != implementation["scorer_sha256"]:
        raise ScoreError("answer key was frozen against a different running scorer")
    return inputs


def _public_pack(pack_root: Path, key: dict[str, Any]) -> dict[str, Any]:
    public = pack_root / "public"
    if public.is_symlink() or not public.is_dir():
        raise ScoreError("prepared public root is missing or is a symlink")
    expected_sessions: set[str] = set()
    expected_stimuli: set[str] = set()
    session_hashes: list[str] = []
    stimulus_hashes: list[str] = []
    file_hashes = {
        "index.html": key["implementation"]["runner_sha256"],
    }
    audio_digest_cache: dict[tuple[Any, ...], str] = {}

    for participant_number, participant in enumerate(key["participants"], 1):
        participant_id = participant["participant_id"]
        token = participant["session_token"]
        relative_session = f"sessions/{token}.json"
        expected_sessions.add(relative_session)
        session_path = public / relative_session
        if session_path.is_symlink():
            raise ScoreError(f"public session for {participant_id} is a symlink")
        session, session_digest = _load_json(session_path)
        _exact_keys(session, {
            "mapping_commitment", "max_replays", "practice", "schema",
            "session_token", "study_fingerprint", "study_id", "trials"
        }, f"public session for {participant_id}")
        if (session["schema"] != SESSION_SCHEMA
                or session["session_token"] != token
                or session["study_id"] != key["study_id"]
                or session["study_fingerprint"] != key["study_fingerprint"]
                or session["mapping_commitment"] != participant["mapping_commitment"]
                or type(session["max_replays"]) is not int
                or session["max_replays"] != MAX_REPLAYS):
            raise ScoreError(f"public session for {participant_id} does not match the key")
        session_hashes.append(f"{relative_session}:{session_digest}")
        file_hashes[relative_session] = session_digest

        for section, public_records, key_records in (
                ("practice", session["practice"], participant["practice"]),
                ("scored", session["trials"], participant["trials"])):
            if (not isinstance(public_records, list)
                    or len(public_records) != len(key_records)):
                raise ScoreError(
                    f"public {section} records for {participant_id} are incomplete")
            for position, (record, key_record) in enumerate(
                    zip(public_records, key_records), 1):
                item = _exact_keys(
                    record, {"a", "b", "trial_id"},
                    f"public {section} record {position} for {participant_id}")
                if item["trial_id"] != key_record["trial_id"]:
                    raise ScoreError(
                        f"public {section} trial ID {position} for {participant_id} differs")
                for side in ("a", "b"):
                    relative_audio = item[side]
                    expected_audio = (
                        "stimuli/"
                        + _opaque(key["presentation_seed"], "audio",
                                  participant_number, section, position, side)
                        + ".wav")
                    if (not isinstance(relative_audio, str)
                            or not re.fullmatch(
                                r"stimuli/[0-9a-f]{24}\.wav", relative_audio)
                            or relative_audio != expected_audio
                            or relative_audio in expected_stimuli):
                        raise ScoreError(
                            f"public {section} audio path {position}{side} for "
                            f"{participant_id} is invalid or reused")
                    expected_stimuli.add(relative_audio)
                    audio_path = public / relative_audio
                    if audio_path.is_symlink():
                        raise ScoreError(
                            f"public {section} audio {position}{side} for "
                            f"{participant_id} is a symlink")
                    try:
                        stat = audio_path.stat()
                    except OSError as error:
                        raise ScoreError(
                            f"could not read public {section} audio {position}{side} "
                            f"for {participant_id}: {error}") from error
                    identity = ((stat.st_dev, stat.st_ino)
                                if stat.st_ino else ("path", str(audio_path.resolve())))
                    cache_key = (*identity, stat.st_size, stat.st_mtime_ns)
                    actual_digest = audio_digest_cache.get(cache_key)
                    if actual_digest is None:
                        actual_digest = _sha256(audio_path)
                        audio_digest_cache[cache_key] = actual_digest
                    if actual_digest != key_record[f"{side}_sha256"]:
                        raise ScoreError(
                            f"public {section} audio {position}{side} for "
                            f"{participant_id} SHA-256 does not match the answer key")
                    stimulus_hashes.append(
                        f"{relative_audio}:{actual_digest}")
                    file_hashes[relative_audio] = actual_digest

    actual_files: set[str] = set()
    actual_directories: set[str] = set()
    for path in public.rglob("*"):
        relative = path.relative_to(public).as_posix()
        if path.is_symlink():
            raise ScoreError(f"prepared public tree contains symlink: {relative}")
        if path.is_dir():
            actual_directories.add(relative)
        elif path.is_file():
            actual_files.add(relative)
        else:
            raise ScoreError(f"prepared public tree contains non-regular entry: {relative}")
    expected_files = {"README.txt", "index.html", *expected_sessions, *expected_stimuli}
    if actual_directories != {"sessions", "stimuli"} or actual_files != expected_files:
        raise ScoreError("prepared public tree contains missing or unexpected entries")
    expected_readme = (
        "Run `python3 serve.py --expected-fingerprint "
        "<externally-recorded-64-hex-fingerprint>` from the pack root; "
        "do not use a directory-listing server.\n"
        "Open only the opaque URL assigned by the coordinator.\n"
        "Never expose the private directory or participant-links.tsv.\n")
    try:
        readme = (public / "README.txt").read_text(encoding="utf-8")
    except (OSError, UnicodeError) as error:
        raise ScoreError(f"could not read public README.txt: {error}") from error
    if readme != expected_readme:
        raise ScoreError("public README.txt differs from the frozen pack")
    file_hashes["README.txt"] = hashlib.sha256(
        readme.encode("utf-8")).hexdigest()

    return {
        "files": dict(sorted(file_hashes.items())),
        "sessions": {
            "count": len(expected_sessions),
            "index_sha256": hashlib.sha256(
                "\n".join(sorted(session_hashes)).encode("ascii")).hexdigest(),
        },
        "stimuli": {
            "count": len(expected_stimuli),
            "index_sha256": hashlib.sha256(
                "\n".join(sorted(stimulus_hashes)).encode("ascii")).hexdigest(),
        },
    }


def _participant_links(pack_root: Path, key: dict[str, Any]) -> dict[str, str]:
    descriptor = _exact_keys(
        key["participant_links"], {"path", "sha256"},
        "answer key participant_links")
    if descriptor["path"] != "private/participant-links.tsv":
        raise ScoreError("answer key participant_links has the wrong archive path")
    lines = ["participant_id\tlistener_stratum\tsession_token\tquery"]
    lines.extend(
        f"{participant['participant_id']}\t{participant['listener_stratum']}\t"
        f"{participant['session_token']}\t?session={participant['session_token']}"
        for participant in key["participants"])
    expected = ("\n".join(lines) + "\n").encode("utf-8")
    raw, input_record = _checked_bytes(
        pack_root / descriptor["path"], descriptor["sha256"],
        "archived participant links")
    if raw != expected:
        raise ScoreError("archived participant links do not match the answer key")
    return input_record


def _validate_frozen_pack(answer_key_path: Path,
                          key: dict[str, Any]) -> dict[str, Any]:
    pack_root = answer_key_path.parent.parent
    implementation = _implementation_files(pack_root, key)
    _, source_hashes, study_input = _archived_study(pack_root, key)
    _validate_key_sources(key, source_hashes)
    _validate_participant_source_hashes(key, source_hashes)
    comparison, comparison_input = _archived_comparison(
        pack_root, key, source_hashes)
    evidence = _evidence_archives(pack_root, key, comparison)

    hashes = key["implementation"]
    fingerprint = hashlib.sha256(
        (f"{key['source_manifest']['sha256']}:{hashes['preparer_sha256']}:"
         f"{hashes['runner_sha256']}:{hashes['scorer_sha256']}:"
         f"{hashes['server_sha256']}").encode("ascii")).hexdigest()
    if fingerprint != key["study_fingerprint"]:
        raise ScoreError("study fingerprint does not match the frozen pack hashes")

    return {
        "archives": {
            "comparison_manifest": comparison_input,
            "evidence": evidence,
            "participant_links": _participant_links(pack_root, key),
            "study_manifest": study_input,
            **implementation,
        },
        "public": _public_pack(pack_root, key),
    }


def _validate_key(path: Path, expected_fingerprint: str) -> tuple[
        dict[str, Any], str, dict[str, Any]]:
    expected_fingerprint = _normalized_sha256(
        expected_fingerprint, "external expected fingerprint")
    key, digest = _load_json(path)
    _exact_keys(key, {
        "algorithm", "analysis", "archives", "comparison_manifest", "implementation",
        "participant_links", "participants", "presentation_seed", "schema", "source_manifest",
        "sources", "study_fingerprint", "study_id"
    }, "answer key")
    if key["schema"] != KEY_SCHEMA:
        raise ScoreError(f"unexpected answer-key schema: {key['schema']}")
    if not isinstance(key["study_id"], str) or not key["study_id"]:
        raise ScoreError("answer key has no study_id")
    if (not isinstance(key["study_fingerprint"], str)
            or not SHA256_PATTERN.fullmatch(key["study_fingerprint"])):
        raise ScoreError("answer key has an invalid study_fingerprint")
    if key["study_fingerprint"].lower() != expected_fingerprint:
        raise ScoreError(
            "external expected fingerprint does not match the answer key")
    if (not isinstance(key["presentation_seed"], str)
            or not SHA256_PATTERN.fullmatch(key["presentation_seed"])):
        raise ScoreError("answer key has an invalid presentation_seed")

    implementation = _exact_keys(
        key["implementation"],
        {"preparer_sha256", "runner_sha256", "scorer_sha256", "server_sha256"},
        "answer key implementation")
    if any(not isinstance(value, str) or not SHA256_PATTERN.fullmatch(value)
           for value in implementation.values()):
        raise ScoreError("answer key implementation contains an invalid SHA-256")

    analysis = _exact_keys(
        key["analysis"],
        {"schema", "bootstrap_replicates", "bootstrap_seed_sha256"},
        "answer key analysis")
    expected_seed = _bootstrap_seed(key["presentation_seed"])
    if (analysis["schema"] != ANALYSIS_SCHEMA
            or type(analysis["bootstrap_replicates"]) is not int
            or analysis["bootstrap_replicates"] != BOOTSTRAP_REPLICATES
            or analysis["bootstrap_seed_sha256"] != expected_seed):
        raise ScoreError("answer key analysis contract does not match this scorer")

    participants = key["participants"]
    if not isinstance(participants, list) or len(participants) != 30:
        raise ScoreError("answer key must contain exactly 30 participants")
    all_trial_ids: set[str] = set()
    session_tokens: set[str] = set()
    for index, participant in enumerate(participants):
        participant_id = PARTICIPANTS[index]
        item = _exact_keys(
            participant,
            {"listener_stratum", "mapping_commitment", "participant_id",
             "practice", "session_token", "trials"},
            f"answer key participant {participant_id}")
        if item["participant_id"] != participant_id:
            raise ScoreError(f"answer key participant order must contain {participant_id}")
        if item["listener_stratum"] != _expected_stratum(participant_id):
            raise ScoreError(f"answer key has the wrong stratum for {participant_id}")
        if (not isinstance(item["session_token"], str)
                or not re.fullmatch(r"[0-9a-f]{32}", item["session_token"])
                or item["session_token"] in session_tokens):
            raise ScoreError(f"answer key has an invalid session token for {participant_id}")
        session_tokens.add(item["session_token"])
        mapping = {name: item[name] for name in (
            "listener_stratum", "participant_id", "practice", "session_token", "trials")}
        if (not isinstance(item["mapping_commitment"], str)
                or not SHA256_PATTERN.fullmatch(item["mapping_commitment"])
                or item["mapping_commitment"] != _mapping_commitment(
                    key["presentation_seed"], mapping)):
            raise ScoreError(f"answer key mapping commitment failed for {participant_id}")
        if not isinstance(item["practice"], list) or len(item["practice"]) != 2:
            raise ScoreError(f"{participant_id} must have two practice trials")
        if not isinstance(item["trials"], list) or len(item["trials"]) != 13:
            raise ScoreError(f"{participant_id} must have thirteen scored trials")

        for position, practice in enumerate(item["practice"], 1):
            record = _exact_keys(practice, {
                "a_sha256", "a_source", "b_sha256", "b_source", "pair",
                "position", "trial_id"
            }, f"{participant_id} practice {position}")
            if (type(record["position"]) is not int
                    or record["position"] != position
                    or not isinstance(record["pair"], str)
                    or record["pair"] not in {"practice-1", "practice-2"}):
                raise ScoreError(f"{participant_id} practice order is invalid")
            _validate_sources(record, f"{participant_id} practice {position}")
            _unique_trial_id(record["trial_id"], all_trial_ids)

        originals: dict[int, dict[str, Any]] = {}
        repeats: dict[int, dict[str, Any]] = {}
        for position, trial in enumerate(item["trials"], 1):
            record = _exact_keys(trial, {
                "a_sha256", "a_source", "b_sha256", "b_source", "cell",
                "position", "repeat_of", "trial_id"
            }, f"{participant_id} scored trial {position}")
            if (type(record["position"]) is not int
                    or record["position"] != position
                    or type(record["cell"]) is not int):
                raise ScoreError(f"{participant_id} scored order is invalid")
            _validate_sources(record, f"{participant_id} scored trial {position}")
            _unique_trial_id(record["trial_id"], all_trial_ids)
            if record["repeat_of"] is None:
                originals[record["cell"]] = record
            elif record["repeat_of"] == record["cell"] in REPEAT_CELLS:
                repeats[record["cell"]] = record
            else:
                raise ScoreError(f"{participant_id} has an invalid hidden repeat")
        if set(originals) != set(range(1, 11)) or set(repeats) != set(REPEAT_CELLS):
            raise ScoreError(f"{participant_id} has an incomplete core/repeat layout")
        for cell, repeat in repeats.items():
            original = originals[cell]
            if (repeat["position"] - original["position"] < 4
                    or repeat["a_source"] == original["a_source"]
                    or repeat["a_sha256"] != original["b_sha256"]
                    or repeat["b_sha256"] != original["a_sha256"]):
                raise ScoreError(f"{participant_id} repeat {cell} is not later/reversed")
    _validate_presentation(key)
    frozen_pack = _validate_frozen_pack(path, key)
    return key, digest, frozen_pack


def _validate_sources(record: dict[str, Any], label: str) -> None:
    if (not isinstance(record["a_source"], str)
            or not isinstance(record["b_source"], str)
            or {record["a_source"], record["b_source"]} != {"physical", "electry"}):
        raise ScoreError(f"{label} does not map A/B to both sources")
    for side in ("a", "b"):
        value = record[f"{side}_sha256"]
        if not isinstance(value, str) or not SHA256_PATTERN.fullmatch(value):
            raise ScoreError(f"{label} has an invalid {side.upper()} hash")


def _unique_trial_id(value: Any, seen: set[str]) -> None:
    if not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{20}", value):
        raise ScoreError("answer key has an invalid trial_id")
    if value in seen:
        raise ScoreError(f"answer key repeats trial_id {value}")
    seen.add(value)


def _utc(value: Any, label: str) -> datetime:
    if not isinstance(value, str) or not UTC_PATTERN.fullmatch(value):
        raise ScoreError(f"{label} must be an ISO-8601 UTC timestamp")
    try:
        return datetime.fromisoformat(value[:-1] + "+00:00")
    except ValueError as error:
        raise ScoreError(f"{label} is not a real UTC timestamp") from error


def _validate_responses(key: dict[str, Any], directory: Path) -> tuple[dict[str, Any], list[dict[str, str]]]:
    if not directory.is_dir():
        raise ScoreError(f"response directory does not exist: {directory}")
    paths = sorted(directory.rglob("*.json"))
    if len(paths) != 30:
        raise ScoreError(f"response directory must contain exactly 30 JSON files, found {len(paths)}")
    expected_by_token = {item["session_token"]: item for item in key["participants"]}
    responses: dict[str, Any] = {}
    inputs: list[dict[str, str]] = []
    for path in paths:
        result, digest = _load_json(path)
        _exact_keys(result, {
            "completed_utc", "mapping_commitment", "playback_screen_confirmed",
            "responses", "schema", "session_token", "started_utc",
            "study_fingerprint", "study_id"
        }, str(path))
        session_token = result["session_token"]
        if (not isinstance(session_token, str)
                or not re.fullmatch(r"[0-9a-f]{32}", session_token)
                or session_token not in expected_by_token):
            raise ScoreError(f"unexpected session_token in {path}")
        participant = expected_by_token[session_token]["participant_id"]
        if participant in responses:
            raise ScoreError(f"duplicate response for participant_id: {participant}")
        if (result["schema"] != RESULT_SCHEMA
                or result["study_id"] != key["study_id"]
                or result["study_fingerprint"] != key["study_fingerprint"]):
            raise ScoreError(f"{participant} response does not match the frozen study")
        if result["mapping_commitment"] != expected_by_token[session_token][
                "mapping_commitment"]:
            raise ScoreError(f"{participant} response mapping commitment does not match")
        if result["playback_screen_confirmed"] is not True:
            raise ScoreError(f"{participant} did not confirm the playback screen")
        started = _utc(result["started_utc"], f"{participant}.started_utc")
        completed = _utc(result["completed_utc"], f"{participant}.completed_utc")
        if completed < started:
            raise ScoreError(f"{participant} completed before starting")

        answer_records = result["responses"]
        if not isinstance(answer_records, list) or len(answer_records) != 15:
            raise ScoreError(f"{participant} must contain exactly 15 complete responses")
        participant_key = expected_by_token[session_token]
        key_records = [("practice", record) for record in participant_key["practice"]]
        key_records += [("scored", record) for record in participant_key["trials"]]
        for sequence, (answer, (section, key_record)) in enumerate(
                zip(answer_records, key_records), 1):
            record = _exact_keys(answer, {
                "confidence", "defect", "elapsed_ms", "physical_choice",
                "plays_a", "plays_b", "preference", "replay_count",
                "section", "sequence", "trial_id"
            }, f"{participant} response {sequence}")
            if (type(record["sequence"]) is not int
                    or record["sequence"] != sequence
                    or record["section"] != section
                    or record["trial_id"] != key_record["trial_id"]):
                raise ScoreError(f"{participant} response sequence/trial IDs do not match the key")
            if (not isinstance(record["physical_choice"], str)
                    or record["physical_choice"] not in {"a", "b"}):
                raise ScoreError(f"{participant} response {sequence} has no forced A/B choice")
            if (not isinstance(record["preference"], str)
                    or record["preference"] not in {"a", "b", "tie"}):
                raise ScoreError(f"{participant} response {sequence} has an invalid preference")
            if type(record["confidence"]) is not int or not 1 <= record["confidence"] <= 5:
                raise ScoreError(f"{participant} response {sequence} has invalid confidence")
            if (record["defect"] is not None
                    and (not isinstance(record["defect"], str)
                         or record["defect"] not in DEFECTS)):
                raise ScoreError(f"{participant} response {sequence} has an invalid defect")
            if type(record["elapsed_ms"]) is not int or record["elapsed_ms"] < 0:
                raise ScoreError(f"{participant} response {sequence} has invalid elapsed time")
            if (type(record["plays_a"]) is not int or record["plays_a"] < 1
                    or type(record["plays_b"]) is not int or record["plays_b"] < 1
                    or type(record["replay_count"]) is not int
                    or record["replay_count"] != max(0, record["plays_a"] - 1)
                                                   + max(0, record["plays_b"] - 1)
                    or record["replay_count"] > 3):
                raise ScoreError(f"{participant} response {sequence} has invalid play counts")
        responses[participant] = result
        inputs.append({
            "participant_id": participant,
            "path": str(path.resolve()),
            "session_token": session_token,
            "sha256": digest,
        })
    if set(responses) != set(PARTICIPANTS):
        raise ScoreError("responses do not cover exactly p001-p030")
    inputs.sort(key=lambda item: item["participant_id"])
    return responses, inputs


def _rate(rows: list[dict[str, Any]], field: str, name: str) -> dict[str, Any]:
    return {"n": len(rows), name: sum(row[field] for row in rows) / len(rows)}


def _percentile(values: list[float], probability: float) -> float:
    ordered = sorted(values)
    position = (len(ordered) - 1) * probability
    lower = math.floor(position)
    upper = math.ceil(position)
    fraction = position - lower
    return ordered[lower] + fraction * (ordered[upper] - ordered[lower])


def _sample_index(seed: str, replicate: int, stratum: str, slot: int) -> int:
    limit = (1 << 64) - ((1 << 64) % 15)
    retry = 0
    while True:
        digest = hashlib.sha256(bytes.fromhex(seed))
        digest.update(f"\0{replicate}\0{stratum}\0{slot}\0{retry}".encode("ascii"))
        value = int.from_bytes(digest.digest()[:8], "big")
        if value < limit:
            return value % 15
        retry += 1


def _bootstrap(listener_rows: dict[str, dict[str, int]], seed: str) -> dict[str, Any]:
    identification: list[float] = []
    preference: list[float] = []
    for replicate in range(BOOTSTRAP_REPLICATES):
        identification_units = 0
        preference_units = 0
        for stratum, participants in STRATA.items():
            sampled = [participants[_sample_index(seed, replicate, stratum, slot)]
                       for slot in range(15)]
            identification_units += sum(
                listener_rows[item]["identification"] for item in sampled)
            preference_units += sum(
                listener_rows[item]["preference"] for item in sampled)
        # Each identification unit is one correct answer out of ten; each
        # preference unit is half a point out of ten. Divide the complete
        # two-stratum integer total once so Python's version-dependent float
        # summation cannot change a frozen endpoint by one ulp.
        identification.append(identification_units / 300.0)
        preference.append(preference_units / 600.0)
    return {
        "identification_90_percentile_interval": [
            _percentile(identification, 0.05),
            _percentile(identification, 0.95),
        ],
        "preference_one_sided_95_lower_bound": _percentile(preference, 0.05),
    }


def score(answer_key_path: Path, response_directory: Path,
          expected_fingerprint: str) -> dict[str, Any]:
    answer_key_path = answer_key_path.resolve()
    response_directory = response_directory.resolve()
    expected_fingerprint = _normalized_sha256(
        expected_fingerprint, "external expected fingerprint")
    key, key_digest, frozen_pack = _validate_key(
        answer_key_path, expected_fingerprint)
    responses, response_inputs = _validate_responses(key, response_directory)

    core_rows: list[dict[str, Any]] = []
    repeat_agreement: list[int] = []
    scored_a_choices: list[int] = []
    listener_rows: dict[str, dict[str, int]] = {}
    for participant_key in key["participants"]:
        participant = participant_key["participant_id"]
        answers = {item["trial_id"]: item for item in responses[participant]["responses"]}
        originals = {item["cell"]: item for item in participant_key["trials"]
                     if item["repeat_of"] is None}
        repeats = {item["cell"]: item for item in participant_key["trials"]
                   if item["repeat_of"] is not None}
        scored_a_choices.extend(
            int(answers[item["trial_id"]]["physical_choice"] == "a")
            for item in participant_key["trials"])
        participant_core: list[dict[str, Any]] = []
        for cell, trial in originals.items():
            answer = answers[trial["trial_id"]]
            selected_source = trial[f"{answer['physical_choice']}_source"]
            preference = answer["preference"]
            electry_score = (0.5 if preference == "tie" else
                             float(trial[f"{preference}_source"] == "electry"))
            row = {
                "articulation": CELL_META[cell][0],
                "cell": cell,
                "electry_preference": electry_score,
                "identification": float(selected_source == "physical"),
                "participant": participant,
            }
            core_rows.append(row)
            participant_core.append(row)
        for cell, repeat in repeats.items():
            original = originals[cell]
            original_answer = answers[original["trial_id"]]
            repeat_answer = answers[repeat["trial_id"]]
            original_source = original[f"{original_answer['physical_choice']}_source"]
            repeat_source = repeat[f"{repeat_answer['physical_choice']}_source"]
            repeat_agreement.append(int(original_source == repeat_source))
        listener_rows[participant] = {
            "identification": sum(
                int(row["identification"]) for row in participant_core),
            "preference": sum(
                int(2.0 * row["electry_preference"])
                for row in participant_core),
        }

    identification_cells = {}
    preference_cells = {}
    for cell in range(1, 11):
        rows = [row for row in core_rows if row["cell"] == cell]
        identification_cells[str(cell)] = {
            "label": CELL_META[cell][1],
            **_rate(rows, "identification", "physical_source_accuracy"),
        }
        preference_cells[str(cell)] = {
            "label": CELL_META[cell][1],
            **_rate(rows, "electry_preference", "electry_preference_score"),
        }
    identification_articulations = {}
    preference_articulations = {}
    for articulation in ("palm", "dead"):
        rows = [row for row in core_rows if row["articulation"] == articulation]
        identification_articulations[articulation] = _rate(
            rows, "identification", "physical_source_accuracy")
        preference_articulations[articulation] = _rate(
            rows, "electry_preference", "electry_preference_score")

    bootstrap = _bootstrap(listener_rows, key["analysis"]["bootstrap_seed_sha256"])
    identification_core = _rate(core_rows, "identification", "physical_source_accuracy")
    preference_core = _rate(core_rows, "electry_preference", "electry_preference_score")
    a_side_rate = sum(scored_a_choices) / len(scored_a_choices)
    repeat_rate = sum(repeat_agreement) / len(repeat_agreement)
    interval = bootstrap["identification_90_percentile_interval"]

    gates = {
        "a_side_scored_rate_inside_0.35_0.65": 0.35 <= a_side_rate <= 0.65,
        "identification_all_cells_inside_0.30_0.70": all(
            0.30 <= value["physical_source_accuracy"] <= 0.70
            for value in identification_cells.values()),
        "identification_articulations_inside_0.35_0.65": all(
            0.35 <= value["physical_source_accuracy"] <= 0.65
            for value in identification_articulations.values()),
        "identification_overall_90_interval_inside_0.40_0.60": (
            interval[0] >= 0.40 and interval[1] <= 0.60),
        "identification_priority_cells_inside_0.35_0.65": all(
            0.35 <= identification_cells[str(cell)]["physical_source_accuracy"] <= 0.65
            for cell in (5, 6, 9, 10)),
        "preference_all_cells_at_least_0.30": all(
            value["electry_preference_score"] >= 0.30
            for value in preference_cells.values()),
        "preference_articulations_at_least_0.40": all(
            value["electry_preference_score"] >= 0.40
            for value in preference_articulations.values()),
        "preference_overall_one_sided_95_lower_at_least_0.40": (
            bootstrap["preference_one_sided_95_lower_bound"] >= 0.40),
        "repeat_same_source_agreement_at_least_0.70": repeat_rate >= 0.70,
    }
    gates["all"] = all(gates.values())

    return {
        "analysis": {
            "bootstrap_replicates": BOOTSTRAP_REPLICATES,
            "bootstrap_seed_sha256": key["analysis"]["bootstrap_seed_sha256"],
            "percentiles": "R-7 linear interpolation at 0.05 and 0.95",
            "resampling": "15 listeners with replacement within each fixed stratum; 50/50 stratum mean",
        },
        "endpoints": {
            "a_side_choice": {
                "a_choice_rate": a_side_rate,
                "n": len(scored_a_choices),
            },
            "electry_preference": {
                "articulations": preference_articulations,
                "bootstrap_one_sided_95_lower_bound": bootstrap[
                    "preference_one_sided_95_lower_bound"],
                "cells": preference_cells,
                "core": preference_core,
            },
            "hidden_repeat_identification": {
                "n": len(repeat_agreement),
                "same_source_agreement": repeat_rate,
            },
            "physical_source_identification": {
                "articulations": identification_articulations,
                "bootstrap_90_percentile_interval": interval,
                "cells": identification_cells,
                "core": identification_core,
            },
        },
        "gates": gates,
        "inputs": {
            "answer_key": {
                "path": str(answer_key_path),
                "sha256": key_digest,
            },
            "external_expected_fingerprint": expected_fingerprint,
            "frozen_pack": frozen_pack,
            "responses": response_inputs,
            "scorer": {
                "path": str(Path(__file__).resolve()),
                "sha256": _sha256(Path(__file__).resolve()),
            },
        },
        "listener_strata": {
            name: {"n": len(participants), "participants": participants}
            for name, participants in STRATA.items()
        },
        "schema": SCORE_SCHEMA,
        "study_fingerprint": key["study_fingerprint"],
        "study_id": key["study_id"],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("answer_key", type=Path)
    parser.add_argument("responses", type=Path, help="directory containing exactly 30 response JSONs")
    parser.add_argument("output", type=Path, help="new score JSON")
    parser.add_argument(
        "--expected-fingerprint", required=True,
        help="externally recorded pre-listening 64-hex study fingerprint")
    arguments = parser.parse_args()
    if arguments.output.exists():
        parser.error(f"refusing to overwrite existing output: {arguments.output}")
    try:
        result = score(
            arguments.answer_key, arguments.responses,
            arguments.expected_fingerprint)
    except ScoreError as error:
        parser.error(str(error))
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"Scored complete cohort: gates.all={str(result['gates']['all']).lower()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
