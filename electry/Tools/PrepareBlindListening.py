#!/usr/bin/env python3
"""Prepare an opaque, deterministic Electry blind-listening pack.

The input is a private manifest that points at already cropped, level-matched
stimuli.  This tool verifies their declared hashes and WAV containers; it does
not create captures, crop audio, level-match it, or certify usage rights.
"""

from __future__ import annotations

import argparse
import hashlib
import io
import itertools
import json
import math
import os
import re
import shutil
import struct
import tempfile
from pathlib import Path
from typing import Any


SCHEMA = "electry-blind-study/v1"
SESSION_SCHEMA = "electry-blind-session/v1"
KEY_SCHEMA = "electry-blind-answer-key/v1"
ANALYSIS_SCHEMA = "electry-blind-analysis/v1"
COMPARISON_SCHEMA = "electry-blind-comparison/v1"
PARTICIPANT_COUNT = 30
BOOTSTRAP_REPLICATES = 20000
REPEAT_CELLS = (5, 7, 9)
MAX_REPLAYS = 3
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
CELL_SEMANTICS = {
    1: ("palm_single_e1", "dry", "slot", 1),
    2: ("palm_single_e2", "dry", "slot", 1),
    3: ("dead_single_e1", "dry", "slot", 1),
    4: ("dead_single_e2", "dry", "slot", 1),
    5: ("palm_rapid_e1", "dry", "run", 12),
    6: ("palm_rapid_e1", "common_chain", "run", 12),
    7: ("dead_e1_e2_groove", "dry", "run", 8),
    8: ("dead_e1_e2_groove", "common_chain", "run", 8),
    9: ("palm_open_e1_e2_groove", "dry", "run", 8),
    10: ("palm_open_e1_e2_groove", "common_chain", "run", 8),
}
CELL_CAPTURE_TAKES = {
    1: ("e1-palm-middle.wav",),
    2: ("e2-palm-middle.wav",),
    3: ("e1-dead.wav",),
    4: ("e2-dead.wav",),
    5: ("e1-palm-middle-rapid.wav",),
    6: ("e1-palm-middle-rapid.wav",),
    7: ("dead-e1-e2-groove.wav",),
    8: ("dead-e1-e2-groove.wav",),
    9: ("palm-open-e1-e2-groove.wav",),
    10: ("palm-open-e1-e2-groove.wav",),
}
SELECTION_SETTINGS = {
    "candidate_rank":
        "sha256_seed_nul_protocol_nul_domain_nul_canonical_candidate_json_lowest_first",
    "core_cluster_assignment": "lowest_rank_of_all_exact_5_5_group_assignments",
    "single_direction_assignment": "lowest_rank_exactly_two_down_two_up_cells_1_4",
    "single_slot_draw": "lowest_rank_complete_six_slot_fixed_direction_pool",
    "phrase_run_draw": "lowest_rank_complete_eligible_session_take_run_pool",
    "rapid_run_bpm": 180,
    "palm_landmark": "middle_selected_on_train",
    "selected_failure_rule": "missing_gate_failure_no_substitution",
}
RENDER_SETTINGS = {
    "sample_rate_hz": 44100,
    "channels": 1,
    "bits_per_sample": 24,
    "metadata": "none",
    "single_pre_roll_frames": 2205,
    "palm_single_frames": 30870,
    "dead_single_frames": 18963,
    "palm_level_match_window_frames": 2205,
    "dead_level_match_window_frames": 1323,
    "phrase_level_match": "median_corresponding_per_hit_rms",
    "only_attenuate_louder": True,
    "max_pair_attenuation_db": 6.0,
    "retained_match_tolerance_db": 0.1,
    "final_true_peak_dbtp": -3.0,
}
CHAIN_PARAMETERS = {"distortion": 0.45, "amp": 0.95, "compressor": 0.6}
ANALYSIS_SETTINGS = {
    "protocol": ANALYSIS_SCHEMA,
    "participant_count": PARTICIPANT_COUNT,
    "listener_strata": {
        "extended_range_guitarist": 15,
        "metal_producer": 15,
    },
    "core_cells": list(range(1, 11)),
    "palm_cells": [1, 2, 5, 6, 9, 10],
    "dead_cells": [3, 4, 7, 8],
    "priority_cells": [5, 6, 9, 10],
    "hidden_repeat_cells": list(REPEAT_CELLS),
    "bootstrap_replicates": BOOTSTRAP_REPLICATES,
    "bootstrap_unit": "whole_listener_within_stratum",
    "bootstrap_percentile_method": "r7",
    "identification_interval_percentiles": [5, 95],
    "preference_lower_percentile": 5,
    "stratum_weight": 0.5,
    "preference_tie_value": 0.5,
    "max_replays": MAX_REPLAYS,
    "gate_thresholds": {
        "a_side_choice_rate": {"minimum": 0.35, "maximum": 0.65},
        "cell_identification": {"minimum": 0.30, "maximum": 0.70},
        "articulation_identification": {"minimum": 0.35, "maximum": 0.65},
        "overall_identification_interval": {"minimum": 0.40, "maximum": 0.60},
        "priority_cell_identification": {"minimum": 0.35, "maximum": 0.65},
        "cell_electry_preference_minimum": 0.30,
        "articulation_electry_preference_minimum": 0.40,
        "overall_electry_preference_lower_minimum": 0.40,
        "repeat_same_source_agreement_minimum": 0.70,
    },
}
LISTENER_PROTOCOL = {
    "qualification": {
        "extended_range_guitarist_rule":
            "actively_plays_7_or_8_string_guitar_in_heavy_music",
        "metal_producer_rule": "actively_produces_or_engineers_metal_music",
        "playback_screen_rule": "passes_declared_headphone_and_playback_screen",
        "dual_qualified_assignment": "least_filled_stratum_before_listening",
    },
    "recruitment": {
        "valid_extended_range_guitarists": 15,
        "valid_metal_producers": 15,
        "replacement_reasons": [
            "qualification_failure", "playback_screen_failure", "incomplete_session"],
        "replacement_assignment": "inherits_excluded_listener_id",
    },
    "stopping": {
        "target_valid_responses": PARTICIPANT_COUNT,
        "outcome_based_stopping": False,
        "outcomes_access": "sealed_until_all_valid_responses_arrive",
    },
}
ENGINEERING_ENDPOINTS = {
    "palm_harmonic_contour_rmse": ("weighted_rmse", "per_cluster_harmonic_contact_stroke"),
    "palm_onset_to_peak_error": ("absolute_error", "per_cluster_string_contact_stroke"),
    "palm_rms_50_150_error": ("absolute_error", "per_cluster_string_contact_stroke"),
    "palm_rms_150_500_error": ("absolute_error", "per_cluster_string_contact_stroke"),
    "palm_rms_500_1000_error": ("absolute_error", "per_cluster_string_contact_stroke"),
    "palm_band_below_500_error": ("absolute_error", "per_cluster_string_contact_stroke"),
    "palm_band_above_500_error": ("absolute_error", "per_cluster_string_contact_stroke"),
    "palm_direction_interaction_error": ("weighted_rmse", "per_cluster_string_contact"),
    "palm_track_loss_error": ("absolute_error", "per_cluster_string_contact_stroke"),
    "palm_harmonic_residual_error": ("absolute_error", "per_cluster_string_contact_stroke"),
    "dead_rms_0_30_error": ("absolute_error", "per_cluster_string_stroke"),
    "dead_rms_30_100_error": ("absolute_error", "per_cluster_string_stroke"),
    "dead_rms_100_250_error": ("absolute_error", "per_cluster_string_stroke"),
    "dead_rms_250_380_error": ("absolute_error", "per_cluster_string_stroke"),
    "dead_centroid_0_30_error": ("absolute_error", "per_cluster_string_stroke"),
    "dead_centroid_30_100_error": ("absolute_error", "per_cluster_string_stroke"),
    "dead_centroid_100_250_error": ("absolute_error", "per_cluster_string_stroke"),
    "dead_harmonicity_30_250_error": ("absolute_error", "per_cluster_string_stroke"),
    "dead_partial_decay_rmse": ("weighted_rmse", "per_cluster_string_stroke"),
    "dead_nonharmonic_residual_error": ("absolute_error", "per_cluster_string_stroke"),
    "rapid_envelope_shape_error": ("one_minus_correlation", "per_cluster_phrase"),
    "rapid_first30_rms_displacement_error": ("absolute_error", "per_cluster_phrase"),
    "rapid_hit_drift_error": ("weighted_rmse", "per_cluster_phrase"),
    "dead_groove_inter_hit_residual_error": ("absolute_error", "per_cluster_phrase"),
    "dead_groove_transition_residual_error": ("absolute_error", "per_cluster_transition_stroke"),
    "palm_open_pre_hit_residual_error": ("absolute_error", "per_cluster_transition_stroke"),
    "palm_open_lift_attack_band_error": ("weighted_rmse", "per_cluster_transition_stroke"),
    "palm_open_replant_attack_band_error": ("weighted_rmse", "per_cluster_transition_stroke"),
}
ENGINEERING_MARGIN_METHODS = {
    endpoint_id: (
        "detector_analysis_quantization_only"
        if endpoint_id.startswith("rapid_")
        else "complete_groove_run_resampling"
        if endpoint_id.startswith(("dead_groove_", "palm_open_"))
        else "balanced_three_versus_three_isolated_repetitions")
    for endpoint_id in ENGINEERING_ENDPOINTS
}


def _selection_kind_for_take(take_file: str, label: str) -> str:
    spec = CAPTURE_TAKE_SPECS.get(take_file)
    if spec is None:
        raise StudyError(f"{label} is not a canonical v1 take")
    return "slot" if spec["kind"] == "isolated" else "run"


def _rapid_bpms(value: Any, label: str) -> list[int]:
    if (not isinstance(value, list) or len(value) != 3
            or any(type(bpm) is not int for bpm in value)
            or sorted(value) != [120, 180, 240]):
        raise StudyError(f"{label} must be a 120/180/240 permutation")
    return value


def _engineering_take_files(endpoint_id: str) -> tuple[str, ...]:
    if endpoint_id.startswith("rapid_"):
        return (
            "e1-palm-middle-rapid.wav", "e2-palm-middle-rapid.wav",
            "e1-dead-rapid.wav", "e2-dead-rapid.wav",
        )
    if endpoint_id.startswith("dead_groove_"):
        return ("dead-e1-e2-groove.wav",)
    if endpoint_id.startswith("palm_open_"):
        return ("palm-open-e1-e2-groove.wav",)
    if endpoint_id.startswith("dead_"):
        return ("e1-open.wav", "e1-dead.wav", "e2-open.wav", "e2-dead.wav")
    return (
        "e1-open.wav", "e1-palm-near.wav", "e1-palm-middle.wav",
        "e1-palm-far.wav", "e2-open.wav", "e2-palm-near.wav",
        "e2-palm-middle.wav", "e2-palm-far.wav",
    )


def _engineering_eligible_units(
        cluster: dict[str, Any], endpoint_id: str, method: str) -> list[dict[str, Any]]:
    """Derive complete cluster-aggregate inputs for one endpoint."""
    sources = sorted(cluster["sources"], key=lambda item: item["session_id"])
    take_files = _engineering_take_files(endpoint_id)
    for source in sources:
        for take_file in take_files:
            if take_file not in source["take_hashes"]:
                raise StudyError(
                    f"engineering endpoint {endpoint_id} requires {take_file} "
                    f"from train session {source['session_id']}")

    def capture_unit(source: dict[str, Any], take_file: str) -> dict[str, Any]:
        return {
            "session_id": source["session_id"],
            "capture_take_file": take_file,
            "capture_take_sha256": source["take_hashes"][take_file],
        }

    partitions = [
        combination for combination in itertools.combinations(range(1, 7), 3)
        if 1 in combination
    ]
    if method == "balanced_three_versus_three_isolated_repetitions":
        units = []
        for unit_index, half_a in enumerate(partitions, 1):
            units.append({
                "cluster_id": cluster["cluster_id"],
                "unit_kind": "isolated_3v3_partition_cluster_aggregate",
                "unit_index": unit_index,
                "partition_id": f"partition-{unit_index:02d}",
                "half_a_repetition_ids": list(half_a),
                "half_b_repetition_ids": [
                    value for value in range(1, 7) if value not in half_a],
                "input_units": [
                    {**capture_unit(source, take_file), "stroke": stroke}
                    for source in sources
                    for take_file in take_files
                    for stroke in ("down", "up")
                ],
            })
        return units
    if method == "complete_groove_run_resampling":
        return [
            {
                "cluster_id": cluster["cluster_id"],
                "unit_kind": "complete_groove_run_cluster_aggregate",
                "unit_index": run_index,
                "run_index": run_index,
                "input_units": [
                    {**capture_unit(source, take_file), "run_index": run_index}
                    for source in sources
                    for take_file in take_files
                ],
            }
            for run_index in range(1, 4)
        ]
    if method == "detector_analysis_quantization_only":
        input_units = []
        for source in sources:
            for take_file in take_files:
                bpms = source["take_metadata"][take_file]["run_bpms_in_order"]
                if (not isinstance(bpms, list) or len(bpms) != 3
                        or sorted(bpms) != [120, 180, 240]):
                    raise StudyError(
                        f"engineering endpoint {endpoint_id} requires a valid "
                        f"120/180/240 BPM order for {source['session_id']}:{take_file}")
                run_index = bpms.index(180) + 1
                input_units.append({
                    **capture_unit(source, take_file),
                    "run_index": run_index,
                    "bpm": 180,
                })
        return [{
            "cluster_id": cluster["cluster_id"],
            "unit_kind": "rapid_180_bpm_cluster_input_set",
            "unit_index": 1,
            "input_units": input_units,
        }]
    raise StudyError(f"engineering endpoint {endpoint_id} has an unknown margin method")
SHA256_PATTERN = re.compile(r"[0-9a-fA-F]{64}\Z")
STUDY_ID_PATTERN = re.compile(r"[A-Za-z0-9][A-Za-z0-9._-]{0,79}\Z")


class StudyError(ValueError):
    pass


def _reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise StudyError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _parse_json_bytes(data: bytes, label: str) -> dict[str, Any]:
    def reject_constant(value: str) -> None:
        raise StudyError(f"{label} contains non-finite JSON number {value}")

    try:
        value = json.loads(
            data.decode("utf-8"), object_pairs_hook=_reject_duplicate_keys,
            parse_constant=reject_constant)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise StudyError(f"could not parse {label}: {error}") from error
    if not isinstance(value, dict):
        raise StudyError(f"{label} root must be a JSON object")
    return value


def _read_json_blob(path: Path, label: str) -> tuple[bytes, dict[str, Any]]:
    try:
        data = path.read_bytes()
    except OSError as error:
        raise StudyError(f"could not read {path}: {error}") from error
    return data, _parse_json_bytes(data, label)


def _read_json(path: Path) -> dict[str, Any]:
    return _read_json_blob(path, "study manifest")[1]


def _exact_keys(value: Any, keys: set[str], label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise StudyError(f"{label} must be a JSON object")
    actual = set(value)
    if actual != keys:
        missing = ", ".join(sorted(keys - actual)) or "none"
        extra = ", ".join(sorted(actual - keys)) or "none"
        raise StudyError(f"{label} keys differ (missing: {missing}; extra: {extra})")
    return value


def _require_exact(value: Any, expected: Any, label: str) -> None:
    if type(value) is not type(expected):
        raise StudyError(f"{label} must be {expected!r}")
    if isinstance(expected, dict):
        _exact_keys(value, set(expected), label)
        for key, expected_value in expected.items():
            _require_exact(value[key], expected_value, f"{label}.{key}")
    elif isinstance(expected, list):
        if len(value) != len(expected):
            raise StudyError(f"{label} must be {expected!r}")
        for index, expected_value in enumerate(expected):
            _require_exact(value[index], expected_value, f"{label}[{index}]")
    elif value != expected:
        raise StudyError(f"{label} must be {expected!r}")


def _number(value: Any, label: str, minimum: float | None = None,
            maximum: float | None = None) -> float:
    if type(value) not in (int, float) or not math.isfinite(value):
        raise StudyError(f"{label} must be a finite number")
    result = float(value)
    if minimum is not None and result < minimum:
        raise StudyError(f"{label} must be at least {minimum}")
    if maximum is not None and result > maximum:
        raise StudyError(f"{label} must be at most {maximum}")
    return result


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _resolve_file(manifest_path: Path, value: Any, label: str) -> Path:
    if not isinstance(value, str) or not value or value.startswith("REPLACE"):
        raise StudyError(f"{label} must name a filled file path")
    path = Path(value)
    if not path.is_absolute():
        path = manifest_path.parent / path
    path = path.resolve()
    if not path.is_file():
        raise StudyError(f"{label} does not exist: {path}")
    return path


def _verify_digest(path: Path, declared: Any, label: str) -> str:
    declared = _filled_sha256(declared, f"{label}.sha256")
    actual = _sha256(path)
    if actual != declared:
        raise StudyError(f"{label} SHA-256 mismatch for {path}")
    return actual


def _verified_json_descriptor(base_path: Path, value: Any,
                              label: str) -> dict[str, Any]:
    item = _exact_keys(value, {"path", "sha256"}, label)
    path = _resolve_file(base_path, item["path"], f"{label}.path")
    declared = _filled_sha256(item["sha256"], f"{label}.sha256")
    data, parsed = _read_json_blob(path, label)
    actual = hashlib.sha256(data).hexdigest()
    if actual != declared:
        raise StudyError(f"{label} SHA-256 mismatch for {path}")
    return {"bytes": data, "path": path, "sha256": actual, "value": parsed}


def _verified_file_descriptor(base_path: Path, value: Any,
                              label: str) -> dict[str, Any]:
    item = _exact_keys(value, {"path", "sha256"}, label)
    path = _resolve_file(base_path, item["path"], f"{label}.path")
    declared = _filled_sha256(item["sha256"], f"{label}.sha256")
    try:
        data = path.read_bytes()
    except OSError as error:
        raise StudyError(f"could not read {path}: {error}") from error
    actual = hashlib.sha256(data).hexdigest()
    if actual != declared:
        raise StudyError(f"{label} SHA-256 mismatch for {path}")
    return {"bytes": data, "path": path, "sha256": actual}


def _filled_sha256(value: Any, label: str) -> str:
    if (not isinstance(value, str)
            or not SHA256_PATTERN.fullmatch(value)
            or set(value) == {"0"}):
        raise StudyError(f"{label} must be a filled SHA-256")
    return value.lower()


def _source_id(value: Any, label: str) -> str:
    if not isinstance(value, str) or not STUDY_ID_PATTERN.fullmatch(value):
        raise StudyError(f"{label} must be a safe non-placeholder ID")
    if value.startswith("REPLACE"):
        raise StudyError(f"{label} must be a safe non-placeholder ID")
    return value


def _private_seed(value: Any, label: str) -> str:
    if (not isinstance(value, str)
            or not SHA256_PATTERN.fullmatch(value)
            or set(value) == {"0"}):
        raise StudyError(f"{label} must be a filled 256-bit hexadecimal value")
    seed = value.lower()
    periodic = any(
        seed == seed[:width] * (len(seed) // width)
        for width in (1, 2, 4, 8, 16, 32))
    if len(set(seed)) < 12 or periodic:
        raise StudyError(
            f"{label} looks guessable; generate it with secrets.token_hex(32)")
    return seed


def _settings_freeze(value: Any, label: str) -> dict[str, Any]:
    item = _exact_keys(
        value, {"implementation_sha256", "settings", "settings_sha256"}, label)
    implementation = _filled_sha256(
        item["implementation_sha256"], f"{label}.implementation_sha256")
    settings = item["settings"]
    if not isinstance(settings, dict) or not settings:
        raise StudyError(f"{label}.settings must be a non-empty JSON object")

    def reject_placeholders(setting: Any, setting_label: str) -> None:
        if isinstance(setting, str) and setting.startswith("REPLACE"):
            raise StudyError(f"{setting_label} contains an unfilled placeholder")
        if isinstance(setting, dict):
            for key, nested in setting.items():
                reject_placeholders(nested, f"{setting_label}.{key}")
        elif isinstance(setting, list):
            for index, nested in enumerate(setting):
                reject_placeholders(nested, f"{setting_label}[{index}]")

    reject_placeholders(settings, f"{label}.settings")
    try:
        canonical = json.dumps(
            settings, ensure_ascii=True, separators=(",", ":"),
            sort_keys=True, allow_nan=False).encode("utf-8")
    except (TypeError, ValueError) as error:
        raise StudyError(f"{label}.settings is not canonical JSON: {error}") from error
    settings_digest = _filled_sha256(
        item["settings_sha256"], f"{label}.settings_sha256")
    if hashlib.sha256(canonical).hexdigest() != settings_digest:
        raise StudyError(f"{label}.settings_sha256 does not match canonical settings")
    return {
        "implementation_sha256": implementation,
        "settings": settings,
        "settings_sha256": settings_digest,
    }


def _inspect_pcm24_wav_bytes(data: bytes, label: object) -> int:
    """Return frames for a metadata-free mono 44.1-kHz 24-bit PCM WAV."""
    file_size = len(data)
    with io.BytesIO(data) as source:
        header = source.read(12)
        if len(header) != 12 or header[:4] != b"RIFF" or header[8:] != b"WAVE":
            raise StudyError(f"{label} is not a RIFF/WAVE file")
        if struct.unpack_from("<I", header, 4)[0] + 8 != file_size:
            raise StudyError(f"{label} has a truncated or trailing RIFF payload")

        chunks: list[bytes] = []
        format_data = b""
        data_bytes = -1
        offset = 12
        while offset < file_size:
            source.seek(offset)
            chunk_header = source.read(8)
            if len(chunk_header) != 8:
                raise StudyError(f"{label} has a truncated chunk header")
            chunk_id, chunk_size = struct.unpack("<4sI", chunk_header)
            data_start = offset + 8
            data_end = data_start + chunk_size
            next_offset = data_end + (chunk_size & 1)
            if data_end > file_size or next_offset > file_size:
                raise StudyError(f"{label} has a truncated RIFF chunk")
            if chunk_id not in (b"fmt ", b"data"):
                raise StudyError(
                    f"{label} contains {chunk_id!r}; listening stimuli must be metadata-free")
            if chunk_id in chunks:
                raise StudyError(f"{label} repeats its {chunk_id!r} chunk")
            if chunk_size & 1:
                source.seek(data_end)
                if source.read(1) != b"\0":
                    raise StudyError(f"{label} must use a zero RIFF pad byte")
            chunks.append(chunk_id)
            if chunk_id == b"fmt ":
                source.seek(data_start)
                format_data = source.read(chunk_size)
            else:
                data_bytes = chunk_size
            offset = next_offset

    if chunks != [b"fmt ", b"data"]:
        raise StudyError(f"{label} must contain one fmt chunk followed by one data chunk")
    if len(format_data) < 16:
        raise StudyError(f"{label} has a short fmt chunk")

    (format_tag, channels, sample_rate, byte_rate,
     block_alignment, bits_per_sample) = struct.unpack_from("<HHIIHH", format_data)
    if (format_tag != 1 or len(format_data) != 16 or channels != 1 or sample_rate != 44100
            or byte_rate != 132300 or block_alignment != 3
            or bits_per_sample != 24):
        raise StudyError(
            f"{label} must use the canonical 16-byte mono 44.1-kHz 24-bit PCM format")
    if data_bytes <= 0 or data_bytes % 3:
        raise StudyError(f"{label} has an invalid 24-bit mono data payload")
    return data_bytes // 3


def _inspect_pcm24_wav(path: Path) -> int:
    try:
        data = path.read_bytes()
    except OSError as error:
        raise StudyError(f"could not read {path}: {error}") from error
    return _inspect_pcm24_wav_bytes(data, path)


def _descriptor(manifest_path: Path, value: Any, label: str,
                wav: bool) -> dict[str, Any]:
    item = _exact_keys(value, {"path", "sha256"}, label)
    path = _resolve_file(manifest_path, item["path"], f"{label}.path")
    declared = _filled_sha256(item["sha256"], f"{label}.sha256")
    try:
        data = path.read_bytes()
    except OSError as error:
        raise StudyError(f"could not read {path}: {error}") from error
    digest = hashlib.sha256(data).hexdigest()
    if digest != declared:
        raise StudyError(f"{label} SHA-256 mismatch for {path}")
    return {
        "bytes": data,
        "path": path,
        "sha256": digest,
        "frames": _inspect_pcm24_wav_bytes(data, path) if wav else None,
    }


def _capture_source(comparison_path: Path, value: Any, label: str,
                    expected_split: str) -> dict[str, Any]:
    item = _exact_keys(
        value, {"session_id", "player_id", "guitar_id", "capture_manifest"}, label)
    session_id = _source_id(item["session_id"], f"{label}.session_id")
    player_id = _source_id(item["player_id"], f"{label}.player_id")
    guitar_id = _source_id(item["guitar_id"], f"{label}.guitar_id")
    descriptor = _verified_json_descriptor(
        comparison_path, item["capture_manifest"], f"{label}.capture_manifest")
    capture = descriptor["value"]
    if capture.get("schema") != "electry-mute-capture/v1":
        raise StudyError(f"{label} capture manifest has the wrong schema")
    session = capture.get("session")
    instrument = capture.get("instrument")
    takes = capture.get("takes")
    if not isinstance(session, dict) or not isinstance(instrument, dict):
        raise StudyError(f"{label} capture manifest lacks session/instrument identity")
    expected_identity = {
        "split": expected_split,
        "session_id": session_id,
        "player_id": player_id,
        "guitar_id": guitar_id,
    }
    actual_identity = {
        "split": session.get("split"),
        "session_id": session.get("session_id"),
        "player_id": session.get("player_id"),
        "guitar_id": instrument.get("guitar_id"),
    }
    if actual_identity != expected_identity:
        raise StudyError(f"{label} identity does not match its capture manifest")
    if not isinstance(takes, list) or not takes:
        raise StudyError(f"{label} capture manifest must contain takes")
    take_hashes: dict[str, str] = {}
    take_metadata: dict[str, dict[str, Any]] = {}
    for index, take in enumerate(takes):
        if not isinstance(take, dict):
            raise StudyError(f"{label} capture take {index} must be an object")
        file_name = take.get("file")
        if file_name not in CAPTURE_TAKE_SPECS:
            raise StudyError(
                f"{label} capture take {index} is not a canonical v1 take")
        if file_name in take_hashes:
            raise StudyError(f"{label} capture manifest repeats take {file_name}")
        expected_spec = CAPTURE_TAKE_SPECS[file_name]
        expected_keys = {"file", "frames", "sha256", *expected_spec}
        if expected_spec["kind"] == "rapid":
            expected_keys.add("run_bpms_in_order")
        _exact_keys(take, expected_keys, f"{label} capture take {file_name}")
        for field, expected in expected_spec.items():
            _require_exact(
                take[field], expected, f"{label} capture take {file_name}.{field}")
        frames = take["frames"]
        if type(frames) is not int:
            raise StudyError(
                f"{label} capture take {file_name}.frames must be an integer")
        if expected_spec["kind"] == "isolated":
            if frames != 1719900:
                raise StudyError(
                    f"{label} capture take {file_name}.frames must be 1719900")
        else:
            minimum_frames = (407007 if expected_spec["kind"] == "rapid"
                              else 352800 if file_name == "dead-e1-e2-groove.wav"
                              else 429975)
            if frames < minimum_frames:
                raise StudyError(
                    f"{label} capture take {file_name}.frames must be at least "
                    f"{minimum_frames}")
        take_hashes[file_name] = _filled_sha256(
            take.get("sha256"), f"{label} capture take {file_name}.sha256")
        bpms = take.get("run_bpms_in_order")
        if expected_spec["kind"] == "rapid":
            _rapid_bpms(
                bpms, f"{label} capture take {file_name}.run_bpms_in_order")
        take_metadata[file_name] = {
            "run_bpms_in_order": bpms,
        }
    if [take.get("file") for take in takes] != list(CAPTURE_TAKE_FILES):
        missing = sorted(set(CAPTURE_TAKE_FILES) - set(take_hashes))
        extra = sorted(set(take_hashes) - set(CAPTURE_TAKE_FILES))
        raise StudyError(
            f"{label} capture manifest must contain the 16 canonical v1 takes "
            "in their frozen order "
            f"(missing={missing}, extra={extra})")
    if len(set(take_hashes.values())) != len(CAPTURE_TAKE_FILES):
        raise StudyError(f"{label} capture manifest reuses a take WAV SHA-256")
    return {
        "archive_name": f"capture-manifests/{session_id}.json",
        "bytes": descriptor["bytes"],
        "guitar_id": guitar_id,
        "path": descriptor["path"],
        "player_id": player_id,
        "session_id": session_id,
        "sha256": descriptor["sha256"],
        "take_hashes": take_hashes,
        "take_metadata": take_metadata,
    }


def _selection_unit(value: Any, label: str, expected_kind: str | None) -> dict[str, Any]:
    item = _exact_keys(value, {"kind", "index", "stroke"}, label)
    kind = item["kind"]
    if expected_kind is not None and kind != expected_kind:
        raise StudyError(f"{label}.kind must be {expected_kind}")
    if kind not in ("slot", "run"):
        raise StudyError(f"{label}.kind must be slot or run")
    index = item["index"]
    limit = 12 if kind == "slot" else 3
    if type(index) is not int or not 1 <= index <= limit:
        raise StudyError(f"{label}.index must be in 1-{limit}")
    expected_stroke = ("down" if index % 2 else "up") if kind == "slot" else "down_first"
    if item["stroke"] != expected_stroke:
        raise StudyError(f"{label}.stroke must be {expected_stroke}")
    return {"index": index, "kind": kind, "stroke": expected_stroke}


SELECTION_GROUPS = ((1,), (2,), (3,), (4,), (5, 6), (7, 8), (9, 10))


def _selection_rank(seed: str, domain: str, candidate: Any) -> str:
    canonical = json.dumps(
        candidate, ensure_ascii=True, separators=(",", ":"),
        sort_keys=True, allow_nan=False).encode("utf-8")
    digest = hashlib.sha256(bytes.fromhex(seed))
    digest.update(b"\0electry-stimulus-selection/v1\0")
    digest.update(domain.encode("utf-8"))
    digest.update(b"\0")
    digest.update(canonical)
    return digest.hexdigest()


def _rank_candidates(seed: str, domain: str,
                     candidates: list[dict[str, Any]]) -> list[dict[str, Any]]:
    ranked = [
        {**candidate, "rank_sha256": _selection_rank(seed, domain, candidate)}
        for candidate in candidates
    ]
    return sorted(ranked, key=lambda candidate: candidate["rank_sha256"])


def _cluster_assignment_candidates(
        seed: str, cluster_ids: list[str]) -> list[dict[str, Any]]:
    assignments: list[dict[str, Any]] = []
    for choices in itertools.product(cluster_ids, repeat=len(SELECTION_GROUPS)):
        counts = {cluster_id: 0 for cluster_id in cluster_ids}
        for group, cluster_id in zip(SELECTION_GROUPS, choices):
            counts[cluster_id] += len(group)
        if set(counts.values()) != {5}:
            continue
        assignments.append({
            "groups": [
                {"cells": list(group), "source_cluster_id": cluster_id}
                for group, cluster_id in zip(SELECTION_GROUPS, choices)
            ],
        })
    return _rank_candidates(seed, "cluster-assignment", assignments)


def _single_direction_candidates(seed: str) -> list[dict[str, Any]]:
    directions = [
        {
            "down_cells": list(down_cells),
            "up_cells": [cell for cell in range(1, 5) if cell not in down_cells],
        }
        for down_cells in itertools.combinations(range(1, 5), 2)
    ]
    return _rank_candidates(seed, "single-directions", directions)


def _selection_receipt_value(
        study_id: str, seed: str, practice: dict[str, dict[str, Any]],
        cells: dict[int, dict[str, Any]], sessions: dict[str, dict[str, Any]],
        holdout_sessions: dict[str, str]) -> dict[str, Any]:
    cluster_ids = sorted(set(holdout_sessions.values()))
    if len(cluster_ids) != 2:
        raise StudyError("selection requires exactly two active holdout clusters")

    ranked_assignments = _cluster_assignment_candidates(seed, cluster_ids)
    selected_assignment = ranked_assignments[0]
    selected_clusters = {
        cell: group["source_cluster_id"]
        for group in selected_assignment["groups"]
        for cell in group["cells"]
    }
    for cell in range(1, 11):
        if cells[cell]["source_cluster_id"] != selected_clusters[cell]:
            raise StudyError(
                f"comparison manifest cell {cell} does not match the seeded cluster rotation")

    ranked_directions = _single_direction_candidates(seed)
    selected_directions = ranked_directions[0]
    down_cells = set(selected_directions["down_cells"])
    for cell in range(1, 5):
        expected_stroke = "down" if cell in down_cells else "up"
        if cells[cell]["provenance"]["selection_unit"]["stroke"] != expected_stroke:
            raise StudyError(
                f"comparison manifest cell {cell} does not match the seeded stroke pool")

    draws: list[dict[str, Any]] = []

    def add_draw(pair_id: str | int, pair: dict[str, Any],
                 candidate_sources: list[dict[str, Any]], indices: list[int],
                 kind: str, stroke: str, cluster_id: str | None) -> None:
        candidates: list[dict[str, Any]] = []
        take_file = pair["provenance"]["capture_take_file"]
        for source in candidate_sources:
            if take_file not in source["take_hashes"]:
                continue
            for index in indices:
                candidate = {
                    "source_session_id": source["session_id"],
                    "capture_take_file": take_file,
                    "capture_take_sha256": source["take_hashes"][take_file],
                    "selection_unit": {"kind": kind, "index": index, "stroke": stroke},
                }
                if cluster_id is not None:
                    candidate["source_cluster_id"] = cluster_id
                candidates.append(candidate)
        if not candidates:
            raise StudyError(f"selection draw {pair_id} has no eligible candidates")
        ranked = _rank_candidates(seed, f"draw:{pair_id}", candidates)
        selected = {key: value for key, value in ranked[0].items()
                    if key != "rank_sha256"}
        actual = {
            "source_session_id": pair["source_session_id"],
            "capture_take_file": pair["provenance"]["capture_take_file"],
            "capture_take_sha256": pair["provenance"]["capture_take_sha256"],
            "selection_unit": pair["provenance"]["selection_unit"],
        }
        if cluster_id is not None:
            actual["source_cluster_id"] = pair["source_cluster_id"]
        if actual != selected:
            raise StudyError(
                f"comparison manifest selection {pair_id} is not the first seeded rank")
        draws.append({
            "id": pair_id,
            "candidates": ranked,
            "selected_rank_sha256": ranked[0]["rank_sha256"],
        })

    for pair_id in ("practice-1", "practice-2"):
        pair = practice[pair_id]
        unit = pair["provenance"]["selection_unit"]
        indices = list(range(1, 13 if unit["kind"] == "slot" else 4))
        strokes = (("down", "up") if unit["kind"] == "slot" else ("down_first",))
        candidates: list[dict[str, Any]] = []
        source = sessions[pair["source_session_id"]]
        for stroke in strokes:
            stroke_indices = ([index for index in indices
                               if (index % 2 == 1) == (stroke == "down")]
                              if unit["kind"] == "slot" else indices)
            for index in stroke_indices:
                candidates.append({
                    "source_session_id": source["session_id"],
                    "capture_take_file": pair["provenance"]["capture_take_file"],
                    "capture_take_sha256": pair["provenance"]["capture_take_sha256"],
                    "selection_unit": {
                        "kind": unit["kind"], "index": index, "stroke": stroke},
                })
        ranked = _rank_candidates(seed, f"draw:{pair_id}", candidates)
        actual = {
            "source_session_id": pair["source_session_id"],
            "capture_take_file": pair["provenance"]["capture_take_file"],
            "capture_take_sha256": pair["provenance"]["capture_take_sha256"],
            "selection_unit": unit,
        }
        if actual != {key: value for key, value in ranked[0].items()
                      if key != "rank_sha256"}:
            raise StudyError(
                f"comparison manifest selection {pair_id} is not the first seeded rank")
        draws.append({"id": pair_id, "candidates": ranked,
                      "selected_rank_sha256": ranked[0]["rank_sha256"]})

    for group in SELECTION_GROUPS:
        cell = group[0]
        pair = cells[cell]
        cluster_id = pair["source_cluster_id"]
        sources = [
            source for session_id, source in sessions.items()
            if holdout_sessions.get(session_id) == cluster_id
        ]
        if cell <= 4:
            stroke = "down" if cell in down_cells else "up"
            indices = list(range(1 if stroke == "down" else 2, 13, 2))
            kind = "slot"
        elif cell == 5:
            kind = "run"
            stroke = "down_first"
            indices = []
            take_file = pair["provenance"]["capture_take_file"]
            sources = [source for source in sources
                       if take_file in source["take_hashes"]]
            for source in sources:
                bpms = source["take_metadata"][take_file]["run_bpms_in_order"]
                if (not isinstance(bpms, list) or len(bpms) != 3
                        or sorted(bpms) != [120, 180, 240]):
                    raise StudyError(
                        f"selection source {source['session_id']} lacks a valid rapid BPM order")
            # Every valid rapid take has exactly one eligible 180-BPM run, but its
            # index can differ by session, so build that candidate pool directly.
            candidates = []
            for source in sources:
                bpms = source["take_metadata"][take_file]["run_bpms_in_order"]
                index = bpms.index(180) + 1
                candidates.append({
                    "source_cluster_id": cluster_id,
                    "source_session_id": source["session_id"],
                    "capture_take_file": take_file,
                    "capture_take_sha256": source["take_hashes"][take_file],
                    "selection_unit": {
                        "kind": "run", "index": index, "stroke": "down_first"},
                })
            ranked = _rank_candidates(seed, f"draw:{cell}", candidates)
            actual = {
                "source_cluster_id": cluster_id,
                "source_session_id": pair["source_session_id"],
                "capture_take_file": take_file,
                "capture_take_sha256": pair["provenance"]["capture_take_sha256"],
                "selection_unit": pair["provenance"]["selection_unit"],
            }
            if not ranked or actual != {
                    key: value for key, value in ranked[0].items()
                    if key != "rank_sha256"}:
                raise StudyError(
                    f"comparison manifest selection {cell} is not the first seeded rank")
            draws.append({"id": cell, "candidates": ranked,
                          "selected_rank_sha256": ranked[0]["rank_sha256"]})
            continue
        else:
            kind = "run"
            stroke = "down_first"
            indices = [1, 2, 3]
        add_draw(cell, pair, sources, indices, kind, stroke, cluster_id)

    return {
        "schema": "electry-blind-selection-receipt/v1",
        "status": "frozen",
        "study_id": study_id,
        "selection_seed": seed,
        "algorithm": "sha256_seed_nul_protocol_nul_domain_nul_canonical_candidate_json",
        "cluster_assignment_candidates": ranked_assignments,
        "single_direction_candidates": ranked_directions,
        "draws": draws,
    }


def generate_selection_receipt(input_path: Path, output_path: Path) -> None:
    """Generate the exact seeded v1 choice plan before stimuli are rendered."""
    _, value = _read_json_blob(input_path.resolve(), "selection input")
    selection_input = _exact_keys(
        value,
        {"schema", "study_id", "selection_seed", "sessions", "practice", "cells"},
        "selection input")
    _require_exact(selection_input["schema"], "electry-blind-selection-input/v1",
                   "selection input.schema")
    study_id = _source_id(selection_input["study_id"], "selection input.study_id")
    seed = _private_seed(selection_input["selection_seed"],
                         "selection input.selection_seed")
    session_values = selection_input["sessions"]
    if not isinstance(session_values, list) or not session_values:
        raise StudyError("selection input.sessions must be a non-empty list")
    sessions: dict[str, dict[str, Any]] = {}
    holdout_sessions: dict[str, str] = {}
    session_splits: dict[str, str] = {}
    for index, session_value in enumerate(session_values):
        label = f"selection input.sessions[{index}]"
        session = _exact_keys(
            session_value, {"session_id", "split", "source_cluster_id", "takes"},
            label)
        session_id = _source_id(session["session_id"], f"{label}.session_id")
        cluster_id = _source_id(
            session["source_cluster_id"], f"{label}.source_cluster_id")
        split = session["split"]
        if split not in ("train", "holdout"):
            raise StudyError(f"{label}.split must be train or holdout")
        if session_id in sessions:
            raise StudyError(f"selection input repeats session {session_id}")
        takes = session["takes"]
        if not isinstance(takes, list) or not takes:
            raise StudyError(f"{label}.takes must be a non-empty list")
        take_hashes: dict[str, str] = {}
        take_metadata: dict[str, dict[str, Any]] = {}
        for take_index, take_value in enumerate(takes):
            take_label = f"{label}.takes[{take_index}]"
            take = _exact_keys(
                take_value, {"file", "sha256", "run_bpms_in_order"}, take_label)
            file_name = take["file"]
            if (not isinstance(file_name, str) or Path(file_name).name != file_name
                    or not file_name.endswith(".wav") or file_name in take_hashes):
                raise StudyError(f"{take_label}.file must be a unique WAV basename")
            take_hashes[file_name] = _filled_sha256(
                take["sha256"], f"{take_label}.sha256")
            bpms = take["run_bpms_in_order"]
            if bpms is not None:
                _rapid_bpms(bpms, f"{take_label}.run_bpms_in_order")
            take_metadata[file_name] = {"run_bpms_in_order": bpms}
        sessions[session_id] = {
            "session_id": session_id,
            "take_hashes": take_hashes,
            "take_metadata": take_metadata,
        }
        session_splits[session_id] = split
        if split == "holdout":
            holdout_sessions[session_id] = cluster_id
    cluster_ids = sorted(set(holdout_sessions.values()))
    if len(cluster_ids) != 2:
        raise StudyError("selection input must contain exactly two holdout clusters")

    practice_values = selection_input["practice"]
    if not isinstance(practice_values, list) or len(practice_values) != 2:
        raise StudyError("selection input.practice must contain exactly two pairs")
    practice: dict[str, dict[str, Any]] = {}
    for index, practice_value in enumerate(practice_values, 1):
        label = f"selection input.practice[{index - 1}]"
        item = _exact_keys(
            practice_value,
            {"id", "source_session_id", "capture_take_file", "selection_kind"},
            label)
        pair_id = f"practice-{index}"
        _require_exact(item["id"], pair_id, f"{label}.id")
        session_id = _source_id(item["source_session_id"], f"{label}.source_session_id")
        if session_id not in sessions:
            raise StudyError(f"{label}.source_session_id is unknown")
        if session_splits[session_id] != "train":
            raise StudyError(f"{label}.source_session_id must be a train session")
        take_file = item["capture_take_file"]
        source = sessions[session_id]
        if take_file not in source["take_hashes"]:
            raise StudyError(f"{label}.capture_take_file is unknown")
        kind = _selection_kind_for_take(take_file, f"{label}.capture_take_file")
        _require_exact(item["selection_kind"], kind, f"{label}.selection_kind")
        if CAPTURE_TAKE_SPECS[take_file]["kind"] == "rapid":
            _rapid_bpms(
                source["take_metadata"][take_file]["run_bpms_in_order"],
                f"{label}.capture_take_file run_bpms_in_order")
        candidates = []
        for unit_index in range(1, 13 if kind == "slot" else 4):
            candidates.append({
                "source_session_id": session_id,
                "capture_take_file": take_file,
                "capture_take_sha256": source["take_hashes"][take_file],
                "selection_unit": {
                    "kind": kind,
                    "index": unit_index,
                    "stroke": (("down" if unit_index % 2 else "up")
                               if kind == "slot" else "down_first"),
                },
            })
        selected = _rank_candidates(seed, f"draw:{pair_id}", candidates)[0]
        practice[pair_id] = {
            "source_session_id": session_id,
            "provenance": {
                "capture_take_file": take_file,
                "capture_take_sha256": source["take_hashes"][take_file],
                "selection_unit": selected["selection_unit"],
            },
        }

    cell_values = selection_input["cells"]
    if not isinstance(cell_values, list) or len(cell_values) != 7:
        raise StudyError("selection input.cells must contain group roots 1,2,3,4,5,7,9")
    cell_takes: dict[int, str] = {}
    for index, cell_value in enumerate(cell_values):
        label = f"selection input.cells[{index}]"
        item = _exact_keys(cell_value, {"id", "capture_take_file"}, label)
        expected_id = (1, 2, 3, 4, 5, 7, 9)[index]
        _require_exact(item["id"], expected_id, f"{label}.id")
        take_file = item["capture_take_file"]
        if take_file not in CELL_CAPTURE_TAKES[expected_id]:
            raise StudyError(f"{label}.capture_take_file is not valid for cell {expected_id}")
        cell_takes[expected_id] = take_file

    assignment = _cluster_assignment_candidates(seed, cluster_ids)[0]
    cluster_by_cell = {
        cell: group["source_cluster_id"]
        for group in assignment["groups"] for cell in group["cells"]
    }
    directions = _single_direction_candidates(seed)[0]
    down_cells = set(directions["down_cells"])
    cells: dict[int, dict[str, Any]] = {}
    for group in SELECTION_GROUPS:
        cell = group[0]
        cluster_id = cluster_by_cell[cell]
        take_file = cell_takes[cell]
        sources = [source for session_id, source in sessions.items()
                   if holdout_sessions.get(session_id) == cluster_id
                   and take_file in source["take_hashes"]]
        candidates = []
        for source in sources:
            if cell <= 4:
                stroke = "down" if cell in down_cells else "up"
                indices = range(1 if stroke == "down" else 2, 13, 2)
            elif cell == 5:
                stroke = "down_first"
                bpms = source["take_metadata"][take_file]["run_bpms_in_order"]
                if (not isinstance(bpms, list) or len(bpms) != 3
                        or sorted(bpms) != [120, 180, 240]):
                    raise StudyError(
                        f"selection input session {source['session_id']} lacks rapid BPM order")
                indices = [bpms.index(180) + 1]
            else:
                stroke = "down_first"
                indices = range(1, 4)
            for unit_index in indices:
                candidates.append({
                    "source_cluster_id": cluster_id,
                    "source_session_id": source["session_id"],
                    "capture_take_file": take_file,
                    "capture_take_sha256": source["take_hashes"][take_file],
                    "selection_unit": {
                        "kind": "slot" if cell <= 4 else "run",
                        "index": unit_index,
                        "stroke": stroke,
                    },
                })
        if not candidates:
            raise StudyError(f"selection input cell {cell} has no eligible candidate")
        selected = _rank_candidates(seed, f"draw:{cell}", candidates)[0]
        normalized = {
            "source_cluster_id": selected["source_cluster_id"],
            "source_session_id": selected["source_session_id"],
            "provenance": {
                "capture_take_file": selected["capture_take_file"],
                "capture_take_sha256": selected["capture_take_sha256"],
                "selection_unit": selected["selection_unit"],
            },
        }
        for grouped_cell in group:
            cells[grouped_cell] = normalized

    receipt = _selection_receipt_value(
        study_id, seed, practice, cells, sessions, holdout_sessions)
    if output_path.exists():
        raise StudyError(f"refusing to overwrite existing output: {output_path}")
    _write_json(output_path, receipt)


def _provenance(value: Any, label: str, source: dict[str, Any],
                comparison_path: Path, event_records: dict[str, dict[str, Any]],
                expected_kind: str | None,
                allowed_takes: tuple[str, ...] | None = None,
                expected_bpm: int | None = None) -> dict[str, Any]:
    item = _exact_keys(
        value,
        {"capture_take_file", "capture_take_sha256", "selection_unit",
         "electry_event_or_score"}, label)
    take_file = item["capture_take_file"]
    if not isinstance(take_file, str) or take_file not in source["take_hashes"]:
        raise StudyError(f"{label}.capture_take_file is absent from the capture manifest")
    if allowed_takes is not None and take_file not in allowed_takes:
        raise StudyError(f"{label}.capture_take_file does not match the cell content")
    take_sha256 = _filled_sha256(
        item["capture_take_sha256"], f"{label}.capture_take_sha256")
    if source["take_hashes"][take_file] != take_sha256:
        raise StudyError(f"{label}.capture_take_sha256 does not match the capture manifest")
    take_kind = _selection_kind_for_take(take_file, f"{label}.capture_take_file")
    if expected_kind is None:
        expected_kind = take_kind
    else:
        _require_exact(take_kind, expected_kind, f"{label}.capture_take_file kind")
    bpms = None
    if CAPTURE_TAKE_SPECS[take_file]["kind"] == "rapid":
        bpms = _rapid_bpms(
            source["take_metadata"][take_file]["run_bpms_in_order"],
            f"{label}.capture_take_file run_bpms_in_order")
    event = _verified_file_descriptor(
        comparison_path, item["electry_event_or_score"],
        f"{label}.electry_event_or_score")
    event["archive_name"] = f"event-scores/{event['sha256']}.bin"
    event_records.setdefault(event["sha256"], event)
    selection_unit = _selection_unit(
        item["selection_unit"], f"{label}.selection_unit", expected_kind)
    if expected_bpm is not None:
        if bpms is None or bpms[selection_unit["index"] - 1] != expected_bpm:
            raise StudyError(f"{label} does not select the required {expected_bpm}-BPM run")
    return {
        "capture_take_file": take_file,
        "capture_take_sha256": take_sha256,
        "electry_event_or_score_sha256": event["sha256"],
        "selection_unit": selection_unit,
    }


def _qc_record(value: Any, label: str, frames: int,
               expected_hits: int | None) -> dict[str, Any]:
    keys = {
        "physical_onset_frames", "electry_onset_frames",
        "physical_attenuation_db", "electry_attenuation_db",
        "post_match_delta_db", "final_true_peak_dbtp", "passed",
        "failure_flags", "record_sha256",
    }
    item = _exact_keys(value, keys, label)
    onset_lists: list[list[int]] = []
    for field in ("physical_onset_frames", "electry_onset_frames"):
        onsets = item[field]
        if (not isinstance(onsets, list) or not onsets
                or (expected_hits is not None and len(onsets) != expected_hits)):
            expected = expected_hits if expected_hits is not None else "one or more"
            raise StudyError(f"{label}.{field} must contain {expected} onsets")
        if any(type(onset) is not int or not 0 <= onset < frames for onset in onsets):
            raise StudyError(f"{label}.{field} contains an invalid onset")
        if any(right <= left for left, right in zip(onsets, onsets[1:])):
            raise StudyError(f"{label}.{field} must be strictly increasing")
        onset_lists.append(onsets)
    if len(onset_lists[0]) != len(onset_lists[1]):
        raise StudyError(f"{label} physical/electry onset counts differ")
    if expected_hits == 1 and onset_lists != [[2205], [2205]]:
        raise StudyError(f"{label} single-note onsets must equal the 2205-frame pre-roll")
    attenuations = [
        _number(item[field], f"{label}.{field}", 0.0, 6.0)
        for field in ("physical_attenuation_db", "electry_attenuation_db")
    ]
    if min(attenuations) != 0.0:
        raise StudyError(f"{label} must attenuate only the louder source")
    _number(item["post_match_delta_db"], f"{label}.post_match_delta_db", -0.1, 0.1)
    _number(item["final_true_peak_dbtp"], f"{label}.final_true_peak_dbtp",
            maximum=-3.0)
    if item["passed"] is not True or item["failure_flags"] != []:
        raise StudyError(f"{label} must be passed with no failure flags")
    record = {key: item[key] for key in keys - {"record_sha256"}}
    canonical = json.dumps(
        record, ensure_ascii=True, separators=(",", ":"),
        sort_keys=True, allow_nan=False).encode("utf-8")
    if hashlib.sha256(canonical).hexdigest() != _filled_sha256(
            item["record_sha256"], f"{label}.record_sha256"):
        raise StudyError(f"{label}.record_sha256 does not match the QC record")
    return item


def _engineering_freeze(value: Any) -> dict[str, Any]:
    label = "comparison manifest engineering_freeze"
    item = _exact_keys(
        value,
        {"baseline_build_sha256", "candidate_build_sha256", "analyzer_sha256",
         "evaluator_sha256", "event_generator_sha256", "depth_mapping",
         "harmonic_grid_hz", "time_grid_ms", "db_convention",
         "endpoint_weights", "aggregation", "missing_partial_rule",
         "missing_cell_rule", "primary_cells",
         "candidate_contour_rmse_reduction_min", "endpoints",
         "derivation_receipt", "engineering_contract_sha256"}, label)
    baseline = _filled_sha256(item["baseline_build_sha256"],
                              f"{label}.baseline_build_sha256")
    candidate = _filled_sha256(item["candidate_build_sha256"],
                               f"{label}.candidate_build_sha256")
    artifacts = {
        "baseline_build_sha256": baseline,
        "candidate_build_sha256": candidate,
    }
    artifact_hashes = {baseline, candidate}
    if baseline == candidate:
        raise StudyError(f"{label} baseline and candidate builds must differ")
    for field in ("analyzer_sha256", "evaluator_sha256", "event_generator_sha256"):
        artifacts[field] = _filled_sha256(item[field], f"{label}.{field}")
        artifact_hashes.add(artifacts[field])
    mapping = _exact_keys(
        item["depth_mapping"], {"palm_near", "palm_middle", "palm_far", "dead"},
        f"{label}.depth_mapping")
    for field, setting in mapping.items():
        _number(setting, f"{label}.depth_mapping.{field}", 0.0, 1.0)
    for field, minimum in (("harmonic_grid_hz", 0.0), ("time_grid_ms", -1e-15)):
        grid = item[field]
        if not isinstance(grid, list) or len(grid) < 2:
            raise StudyError(f"{label}.{field} must contain at least two values")
        values = [_number(point, f"{label}.{field}[{index}]", minimum=minimum)
                  for index, point in enumerate(grid)]
        if any(right <= left for left, right in zip(values, values[1:])):
            raise StudyError(f"{label}.{field} must be strictly increasing")
    _require_exact(item["db_convention"], "20_log10_rms_ratio",
                   f"{label}.db_convention")
    _require_exact(item["aggregation"], "per_player_guitar_cluster_then_median_iqr",
                   f"{label}.aggregation")
    _require_exact(item["missing_partial_rule"],
                   "shared_retained_partial_rule_missing_primary_fails",
                   f"{label}.missing_partial_rule")
    _require_exact(item["missing_cell_rule"], "missing_primary_cell_fails",
                   f"{label}.missing_cell_rule")
    _require_exact(item["primary_cells"], list(range(1, 11)),
                   f"{label}.primary_cells")
    _require_exact(item["candidate_contour_rmse_reduction_min"], 0.25,
                   f"{label}.candidate_contour_rmse_reduction_min")
    endpoints = item["endpoints"]
    if not isinstance(endpoints, list) or len(endpoints) != len(ENGINEERING_ENDPOINTS):
        raise StudyError(f"{label}.endpoints must contain the complete v1 endpoint set")
    endpoint_ids: set[str] = set()
    margins: list[dict[str, Any]] = []
    for index, endpoint_value in enumerate(endpoints):
        endpoint_label = f"{label}.endpoints[{index}]"
        endpoint = _exact_keys(
            endpoint_value,
            {"id", "orientation", "metric", "aggregation", "no_regression_margin"},
            endpoint_label)
        endpoint_id = _source_id(endpoint["id"], f"{endpoint_label}.id")
        expected_id = list(ENGINEERING_ENDPOINTS)[index]
        if endpoint_id != expected_id:
            raise StudyError(f"{endpoint_label}.id must be {expected_id}")
        endpoint_ids.add(endpoint_id)
        _require_exact(endpoint["orientation"], "lower_is_better",
                       f"{endpoint_label}.orientation")
        expected_metric, expected_aggregation = ENGINEERING_ENDPOINTS[endpoint_id]
        _require_exact(endpoint["metric"], expected_metric, f"{endpoint_label}.metric")
        _require_exact(endpoint["aggregation"], expected_aggregation,
                       f"{endpoint_label}.aggregation")
        margin = _exact_keys(
            endpoint["no_regression_margin"],
            {"method", "detector_analysis_quantization", "train_repeatability_p90",
             "value", "formula"}, f"{endpoint_label}.no_regression_margin")
        method = ENGINEERING_MARGIN_METHODS[endpoint_id]
        _require_exact(
            margin["method"], method,
            f"{endpoint_label}.no_regression_margin.method")
        quantization = _number(
            margin["detector_analysis_quantization"],
            f"{endpoint_label}.no_regression_margin.detector_analysis_quantization", 0.0)
        repeatability = _number(
            margin["train_repeatability_p90"],
            f"{endpoint_label}.no_regression_margin.train_repeatability_p90", 0.0)
        selected = _number(
            margin["value"], f"{endpoint_label}.no_regression_margin.value", 0.0)
        if method == "detector_analysis_quantization_only":
            _require_exact(
                margin["train_repeatability_p90"], 0.0,
                f"{endpoint_label}.no_regression_margin.train_repeatability_p90")
            if selected != quantization:
                raise StudyError(
                    f"{endpoint_label} no-regression margin must equal quantization")
            formula = "detector_analysis_quantization"
        else:
            if selected != max(quantization, repeatability):
                raise StudyError(
                    f"{endpoint_label} no-regression margin is not the frozen max")
            formula = "max(detector_analysis_quantization,train_repeatability_p90)"
        _require_exact(margin["formula"], formula,
                       f"{endpoint_label}.no_regression_margin.formula")
        margins.append({"id": endpoint_id, **margin})
    weights = item["endpoint_weights"]
    if not isinstance(weights, dict) or set(weights) != endpoint_ids:
        raise StudyError(f"{label}.endpoint_weights must exactly name every endpoint")
    weight_values = [
        _number(weight, f"{label}.endpoint_weights.{endpoint}", 0.0)
        for endpoint, weight in weights.items()
    ]
    if not math.isclose(sum(weight_values), 1.0, rel_tol=0.0, abs_tol=1e-12):
        raise StudyError(f"{label}.endpoint_weights must sum to 1")
    contract = {key: setting for key, setting in item.items()
                if key not in {"derivation_receipt", "engineering_contract_sha256"}}
    contract_sha256 = hashlib.sha256(json.dumps(
        contract, ensure_ascii=True, separators=(",", ":"), sort_keys=True,
        allow_nan=False).encode("utf-8")).hexdigest()
    _require_exact(
        _filled_sha256(item["engineering_contract_sha256"],
                       f"{label}.engineering_contract_sha256"),
        contract_sha256, f"{label}.engineering_contract_sha256")
    return {
        "artifact_hashes": artifact_hashes,
        "artifacts": artifacts,
        "candidate_contour_rmse_reduction_min":
            item["candidate_contour_rmse_reduction_min"],
        "depth_mapping": item["depth_mapping"],
        "derivation_receipt": item["derivation_receipt"],
        "engineering_contract_sha256": contract_sha256,
        "margins": margins,
    }


def _percentile_r7(values: list[float], probability: float) -> float:
    ordered = sorted(values)
    position = (len(ordered) - 1) * probability
    lower = math.floor(position)
    fraction = position - lower
    if lower == len(ordered) - 1:
        return ordered[lower]
    return ordered[lower] + fraction * (ordered[lower + 1] - ordered[lower])


def _engineering_derivation_receipt(
        comparison_path: Path, value: Any, study_id: str,
        train_clusters: list[dict[str, Any]],
        engineering: dict[str, Any]) -> dict[str, Any]:
    label = "comparison manifest engineering_freeze.derivation_receipt"
    item = _exact_keys(
        value, {"path", "sha256", "required_schema", "required_status"}, label)
    if (item["required_schema"] != "electry-engineering-derivation-receipt/v1"
            or item["required_status"] != "frozen"):
        raise StudyError(f"{label} must require the frozen v1 receipt")
    receipt_descriptor = _verified_json_descriptor(
        comparison_path, {"path": item["path"], "sha256": item["sha256"]}, label)
    receipt = _exact_keys(
        receipt_descriptor.pop("value"),
        {"schema", "status", "study_id", "derivation_result"}, label)
    _require_exact(receipt["schema"], item["required_schema"], f"{label}.schema")
    _require_exact(receipt["status"], item["required_status"], f"{label}.status")
    _require_exact(receipt["study_id"], study_id, f"{label}.study_id")

    result_item = _exact_keys(
        receipt["derivation_result"],
        {"path", "sha256", "required_schema", "required_status"},
        f"{label}.derivation_result")
    if (result_item["required_schema"] != "electry-engineering-derivation-result/v1"
            or result_item["required_status"] != "frozen"):
        raise StudyError(f"{label}.derivation_result must require the frozen v1 result")
    result_descriptor = _verified_json_descriptor(
        receipt_descriptor["path"],
        {"path": result_item["path"], "sha256": result_item["sha256"]},
        f"{label}.derivation_result")
    result = _exact_keys(
        result_descriptor.pop("value"),
        {"schema", "status", "study_id", "train_clusters", "artifacts",
         "depth_mapping", "repeatability_percentile",
         "percentile_method", "candidate_contour_rmse_reduction_min",
         "engineering_contract_sha256", "failure_rule", "exclusions", "endpoints"},
        f"{label}.derivation_result")
    _require_exact(result["schema"], result_item["required_schema"],
                   f"{label}.derivation_result.schema")
    _require_exact(result["status"], result_item["required_status"],
                   f"{label}.derivation_result.status")
    _require_exact(result["study_id"], study_id,
                   f"{label}.derivation_result.study_id")
    sorted_clusters = sorted(train_clusters, key=lambda cluster: cluster["cluster_id"])
    cluster_values = result["train_clusters"]
    if (not isinstance(cluster_values, list)
            or len(cluster_values) != len(sorted_clusters)):
        raise StudyError(f"{label}.derivation_result.train_clusters is incomplete")
    raw_samples: dict[str, list[dict[str, Any]]] = {
        margin["id"]: [] for margin in engineering["margins"]
    }
    train_analysis: list[dict[str, Any]] = []
    analysis_hashes: set[str] = set()

    for cluster_index, (cluster_value, cluster) in enumerate(
            zip(cluster_values, sorted_clusters)):
        cluster_label = f"{label}.derivation_result.train_clusters[{cluster_index}]"
        record = _exact_keys(
            cluster_value, {"cluster_id", "sessions", "analysis_result"},
            cluster_label)
        _require_exact(record["cluster_id"], cluster["cluster_id"],
                       f"{cluster_label}.cluster_id")
        expected_sessions = [
            {"session_id": source["session_id"],
             "capture_manifest_sha256": source["sha256"]}
            for source in sorted(cluster["sources"], key=lambda source: source["session_id"])
        ]
        _require_exact(record["sessions"], expected_sessions,
                       f"{cluster_label}.sessions")
        analysis_item = _exact_keys(
            record["analysis_result"],
            {"path", "sha256", "required_schema", "required_status"},
            f"{cluster_label}.analysis_result")
        if (analysis_item["required_schema"] != "electry-engineering-train-analysis/v1"
                or analysis_item["required_status"] != "frozen"):
            raise StudyError(f"{cluster_label}.analysis_result must require frozen v1")
        analysis_descriptor = _verified_json_descriptor(
            result_descriptor["path"],
            {"path": analysis_item["path"], "sha256": analysis_item["sha256"]},
            f"{cluster_label}.analysis_result")
        if analysis_descriptor["sha256"] in analysis_hashes:
            raise StudyError(f"{label} repeats a train analysis result SHA-256")
        analysis_hashes.add(analysis_descriptor["sha256"])
        analysis = _exact_keys(
            analysis_descriptor.pop("value"),
            {"schema", "status", "study_id", "cluster_id", "sessions",
             "depth_mapping", "passed", "failure_flags", "exclusions", "endpoints"},
            f"{cluster_label}.analysis_result")
        _require_exact(analysis["schema"], analysis_item["required_schema"],
                       f"{cluster_label}.analysis_result.schema")
        _require_exact(analysis["status"], analysis_item["required_status"],
                       f"{cluster_label}.analysis_result.status")
        _require_exact(analysis["study_id"], study_id,
                       f"{cluster_label}.analysis_result.study_id")
        _require_exact(analysis["cluster_id"], cluster["cluster_id"],
                       f"{cluster_label}.analysis_result.cluster_id")
        _require_exact(analysis["sessions"], expected_sessions,
                       f"{cluster_label}.analysis_result.sessions")
        _require_exact(analysis["depth_mapping"], engineering["depth_mapping"],
                       f"{cluster_label}.analysis_result.depth_mapping")
        if analysis["passed"] is not True or analysis["failure_flags"] != []:
            raise StudyError(f"{cluster_label}.analysis_result must pass without failures")
        if analysis["exclusions"] != []:
            raise StudyError(
                f"{cluster_label}.analysis_result must have no train-input exclusions")
        endpoint_values = analysis["endpoints"]
        if (not isinstance(endpoint_values, list)
                or len(endpoint_values) != len(engineering["margins"])):
            raise StudyError(f"{cluster_label}.analysis_result endpoints are incomplete")
        for endpoint_index, (endpoint_value, margin) in enumerate(
                zip(endpoint_values, engineering["margins"])):
            raw_label = (f"{cluster_label}.analysis_result.endpoints"
                         f"[{endpoint_index}]")
            raw_endpoint = _exact_keys(
                endpoint_value,
                {"id", "eligible_input_units", "excluded_input_units",
                 "repeatability_samples"}, raw_label)
            _require_exact(raw_endpoint["id"], margin["id"], f"{raw_label}.id")
            method = margin["method"]
            expected_units = _engineering_eligible_units(
                cluster, margin["id"], method)
            _require_exact(raw_endpoint["eligible_input_units"], expected_units,
                           f"{raw_label}.eligible_input_units")
            if raw_endpoint["excluded_input_units"] != []:
                raise StudyError(f"{raw_label}.excluded_input_units must be empty")
            samples = raw_endpoint["repeatability_samples"]
            if not isinstance(samples, list):
                raise StudyError(f"{raw_label}.repeatability_samples must be a list")
            if method == "detector_analysis_quantization_only":
                if samples:
                    raise StudyError(f"{raw_label} rapid endpoint must have no samples")
            else:
                if len(samples) != len(expected_units):
                    raise StudyError(
                        f"{raw_label} must contain one sample for every eligible input unit")
                validated: list[dict[str, Any]] = []
                for sample_index, (sample_value, expected_unit) in enumerate(
                        zip(samples, expected_units)):
                    sample_label = (
                        f"{raw_label}.repeatability_samples[{sample_index}]")
                    sample = _exact_keys(
                        sample_value, {*expected_unit, "value"}, sample_label)
                    _require_exact(
                        {key: sample[key] for key in expected_unit}, expected_unit,
                        f"{sample_label}.provenance")
                    _number(sample["value"], f"{sample_label}.value", 0.0)
                    validated.append(sample)
                raw_samples[margin["id"]].extend(validated)
        analysis_descriptor["archive_name"] = (
            f"engineering-train-analysis/{cluster['cluster_id']}.json")
        analysis_descriptor["cluster_id"] = cluster["cluster_id"]
        train_analysis.append(analysis_descriptor)

    _require_exact(result["failure_rule"], "missing_train_derivation_or_sample_fails",
                   f"{label}.derivation_result.failure_rule")
    _require_exact(result["exclusions"], [],
                   f"{label}.derivation_result.exclusions")
    _require_exact(result["artifacts"], engineering["artifacts"],
                   f"{label}.derivation_result.artifacts")
    _require_exact(result["depth_mapping"], engineering["depth_mapping"],
                   f"{label}.derivation_result.depth_mapping")
    _require_exact(result["repeatability_percentile"], 0.90,
                   f"{label}.derivation_result.repeatability_percentile")
    _require_exact(result["percentile_method"], "r7_linear_interpolation",
                   f"{label}.derivation_result.percentile_method")
    _require_exact(result["candidate_contour_rmse_reduction_min"],
                   engineering["candidate_contour_rmse_reduction_min"],
                   f"{label}.derivation_result.candidate_contour_rmse_reduction_min")
    _require_exact(result["engineering_contract_sha256"],
                   engineering["engineering_contract_sha256"],
                   f"{label}.derivation_result.engineering_contract_sha256")

    endpoints = result["endpoints"]
    if not isinstance(endpoints, list) or len(endpoints) != len(engineering["margins"]):
        raise StudyError(f"{label}.derivation_result.endpoints is incomplete")
    for index, (record_value, expected_margin) in enumerate(
            zip(endpoints, engineering["margins"])):
        endpoint_label = f"{label}.derivation_result.endpoints[{index}]"
        record = _exact_keys(
            record_value,
            {"id", "method", "detector_analysis_quantization",
             "repeatability_input_summary", "train_repeatability_samples",
             "train_repeatability_p90", "value", "formula"}, endpoint_label)
        for field in ("id", "method", "detector_analysis_quantization",
                      "train_repeatability_p90", "value", "formula"):
            _require_exact(record[field], expected_margin[field],
                           f"{endpoint_label}.{field}")
        samples = record["train_repeatability_samples"]
        method = expected_margin["method"]
        _require_exact(samples, raw_samples[expected_margin["id"]],
                       f"{endpoint_label}.train_repeatability_samples")
        values = [float(sample["value"]) for sample in samples]
        if method == "detector_analysis_quantization_only":
            expected_summary = {
                "unit": "none_no_within_session_same_tempo_repeat",
                "sample_count": 0,
                "cluster_ids": [],
            }
            if values or record["train_repeatability_p90"] != 0.0:
                raise StudyError(f"{endpoint_label} rapid margin must have no train allowance")
        else:
            expected_summary = {
                "unit": ("balanced_three_versus_three_halves"
                         if method
                         == "balanced_three_versus_three_isolated_repetitions"
                         else "complete_groove_runs"),
                "sample_count": len(values),
                "cluster_ids": [cluster["cluster_id"] for cluster in sorted_clusters],
            }
            if record["train_repeatability_p90"] != _percentile_r7(values, 0.90):
                raise StudyError(f"{endpoint_label} train repeatability P90 is not R-7")
        _require_exact(record["repeatability_input_summary"], expected_summary,
                       f"{endpoint_label}.repeatability_input_summary")

    receipt_descriptor["archive_name"] = "engineering-derivation-receipt.json"
    result_descriptor["archive_name"] = "engineering-derivation-result.json"
    return {
        "analysis_result_hashes": analysis_hashes,
        "receipt": receipt_descriptor,
        "result": result_descriptor,
        "train_analysis": train_analysis,
    }


def _artifact_registry(comparison_path: Path, value: Any) -> dict[str, Any]:
    label = "comparison manifest artifact_registry"
    item = _exact_keys(
        value, {"path", "sha256", "required_schema", "required_status"}, label)
    if (item["required_schema"] != "electry-artifact-registry/v1"
            or item["required_status"] != "sealed"):
        raise StudyError(f"{label} must require the sealed v1 registry")
    descriptor = _verified_json_descriptor(
        comparison_path, {"path": item["path"], "sha256": item["sha256"]}, label)
    registry = _exact_keys(
        descriptor.pop("value"), {"schema", "status", "artifacts"}, label)
    if (registry["schema"] != item["required_schema"]
            or registry["status"] != item["required_status"]):
        raise StudyError(f"{label} referenced file is not sealed v1")
    artifacts = registry["artifacts"]
    if not isinstance(artifacts, list) or not artifacts:
        raise StudyError(f"{label}.artifacts must be a non-empty list")
    artifact_ids: set[str] = set()
    artifact_hashes: set[str] = set()
    for index, artifact_value in enumerate(artifacts):
        artifact_label = f"{label}.artifacts[{index}]"
        artifact = _exact_keys(artifact_value, {"id", "sha256"}, artifact_label)
        artifact_id = _source_id(artifact["id"], f"{artifact_label}.id")
        artifact_hash = _filled_sha256(artifact["sha256"], f"{artifact_label}.sha256")
        if artifact_id in artifact_ids or artifact_hash in artifact_hashes:
            raise StudyError(f"{label} repeats an artifact ID or SHA-256")
        artifact_ids.add(artifact_id)
        artifact_hashes.add(artifact_hash)
    descriptor.update({
        "archive_name": "artifact-registry.json",
        "artifact_hashes": artifact_hashes,
    })
    return descriptor


def _selection_receipt(comparison_path: Path, value: Any, study_id: str,
                       selection_seed: str, practice: dict[str, dict[str, Any]],
                       cells: dict[int, dict[str, Any]],
                       sessions: dict[str, dict[str, Any]],
                       holdout_sessions: dict[str, str]) -> dict[str, Any]:
    label = "comparison manifest freezes.selection.receipt"
    item = _exact_keys(
        value, {"path", "sha256", "required_schema", "required_status"}, label)
    if (item["required_schema"] != "electry-blind-selection-receipt/v1"
            or item["required_status"] != "frozen"):
        raise StudyError(f"{label} must require the frozen v1 receipt")
    descriptor = _verified_json_descriptor(
        comparison_path, {"path": item["path"], "sha256": item["sha256"]}, label)
    receipt = descriptor.pop("value")
    expected = _selection_receipt_value(
        study_id, selection_seed, practice, cells, sessions, holdout_sessions)
    _require_exact(receipt, expected, label)
    descriptor["archive_name"] = "selection-receipt.json"
    return descriptor


def _comparison_descriptor(manifest_path: Path, value: Any) -> dict[str, Any]:
    item = _exact_keys(
        value, {"path", "sha256", "required_schema", "required_status"},
        "comparison_manifest")
    if (item["required_schema"] != COMPARISON_SCHEMA
            or item["required_status"] != "frozen"):
        raise StudyError("comparison_manifest must require the frozen v1 schema/status")
    descriptor = _verified_json_descriptor(
        manifest_path, {"path": item["path"], "sha256": item["sha256"]},
        "comparison_manifest")
    comparison = _exact_keys(
        descriptor.pop("value"),
        {"schema", "status", "study_id", "presentation_seed",
         "participant_count", "source_cohort", "practice", "cells", "freezes",
         "engineering_freeze", "listener_protocol", "artifact_registry"},
        "comparison manifest")
    if comparison["schema"] != COMPARISON_SCHEMA or comparison["status"] != "frozen":
        raise StudyError(
            "referenced comparison manifest must declare electry-blind-comparison/v1 and frozen")
    study_id = _source_id(comparison["study_id"], "comparison manifest study_id")
    presentation_seed = _private_seed(
        comparison["presentation_seed"], "comparison manifest presentation_seed")
    if (type(comparison["participant_count"]) is not int
            or comparison["participant_count"] != PARTICIPANT_COUNT):
        raise StudyError(
            f"comparison manifest participant_count must be {PARTICIPANT_COUNT}")

    cohort = _exact_keys(
        comparison["source_cohort"],
        {"holdout_cluster_count", "holdout_clusters", "engineering_train_clusters",
         "practice_train_session_ids"},
        "comparison manifest source_cohort")
    holdout_cluster_values = cohort["holdout_clusters"]
    if (type(cohort["holdout_cluster_count"]) is not int
            or cohort["holdout_cluster_count"] != 2
            or not isinstance(holdout_cluster_values, list)
            or len(holdout_cluster_values) != cohort["holdout_cluster_count"]):
        raise StudyError("comparison manifest must freeze exactly two holdout clusters")
    train_cluster_values = cohort["engineering_train_clusters"]
    if not isinstance(train_cluster_values, list) or len(train_cluster_values) < 3:
        raise StudyError(
            "comparison manifest must freeze at least three engineering train clusters")

    all_sessions: dict[str, dict[str, Any]] = {}
    capture_hashes: set[str] = set()
    capture_take_hashes: set[str] = set()
    event_records: dict[str, dict[str, Any]] = {}
    holdout_sessions: dict[str, str] = {}
    train_sessions: dict[str, str] = {}
    cluster_ids: set[str] = set()
    player_ids: set[str] = set()
    guitar_ids: set[str] = set()

    def register_source(source_value: Any, label: str,
                        split: str) -> dict[str, Any]:
        source = _capture_source(descriptor["path"], source_value, label, split)
        if source["session_id"] in all_sessions:
            raise StudyError(f"comparison manifest repeats session_id {source['session_id']}")
        if source["sha256"] in capture_hashes:
            raise StudyError("comparison manifest reuses a capture manifest SHA-256")
        repeated_take_hashes = capture_take_hashes & set(source["take_hashes"].values())
        if repeated_take_hashes:
            raise StudyError(
                "comparison manifest reuses a capture take WAV SHA-256 across sessions")
        all_sessions[source["session_id"]] = source
        capture_hashes.add(source["sha256"])
        capture_take_hashes.update(source["take_hashes"].values())
        return source

    def register_cluster(cluster_value: Any, label: str, split: str,
                         membership: dict[str, str]) -> dict[str, Any]:
        cluster = _exact_keys(
            cluster_value, {"cluster_id", "player_ids", "guitar_ids", "sessions"},
            label)
        cluster_id = _source_id(cluster["cluster_id"], f"{label}.cluster_id")
        if cluster_id in cluster_ids:
            raise StudyError(f"comparison manifest repeats cluster_id {cluster_id}")
        cluster_ids.add(cluster_id)
        declared_ids: dict[str, set[str]] = {}
        for field, seen in (("player_ids", player_ids), ("guitar_ids", guitar_ids)):
            values = cluster[field]
            if not isinstance(values, list) or not values:
                raise StudyError(f"{label}.{field} must be a non-empty list")
            declared: set[str] = set()
            for value_index, source_value in enumerate(values):
                source_id = _source_id(source_value, f"{label}.{field}[{value_index}]")
                if source_id in declared or source_id in seen:
                    raise StudyError(
                        f"{label}.{field} ID {source_id} occurs in more than one cluster")
                declared.add(source_id)
                seen.add(source_id)
            declared_ids[field] = declared
        session_values = cluster["sessions"]
        if not isinstance(session_values, list) or not session_values:
            raise StudyError(f"{label}.sessions must be a non-empty list")
        sources = [
            register_source(source_value, f"{label}.sessions[{source_index}]", split)
            for source_index, source_value in enumerate(session_values)
        ]
        if ({source["player_id"] for source in sources} != declared_ids["player_ids"]
                or {source["guitar_id"] for source in sources}
                != declared_ids["guitar_ids"]):
            raise StudyError(f"{label} player/guitar IDs do not match its capture manifests")
        connected = [sources[0]]
        remaining = sources[1:]
        connected_players = {sources[0]["player_id"]}
        connected_guitars = {sources[0]["guitar_id"]}
        while remaining:
            for source in remaining:
                if (source["player_id"] in connected_players
                        or source["guitar_id"] in connected_guitars):
                    connected.append(source)
                    connected_players.add(source["player_id"])
                    connected_guitars.add(source["guitar_id"])
                    remaining.remove(source)
                    break
            else:
                raise StudyError(f"{label} contains disconnected player/guitar sessions")
        for source in connected:
            membership[source["session_id"]] = cluster_id
        return {"cluster_id": cluster_id, "sources": sources}

    holdout_clusters = [
        register_cluster(
            cluster_value,
            f"comparison manifest source_cohort.holdout_clusters[{index}]",
            "holdout", holdout_sessions)
        for index, cluster_value in enumerate(holdout_cluster_values)
    ]
    train_clusters = [
        register_cluster(
            cluster_value,
            f"comparison manifest source_cohort.engineering_train_clusters[{index}]",
            "train", train_sessions)
        for index, cluster_value in enumerate(train_cluster_values)
    ]
    practice_train_session_values = cohort["practice_train_session_ids"]
    if (not isinstance(practice_train_session_values, list)
            or len(practice_train_session_values) != 2):
        raise StudyError(
            "comparison manifest must freeze exactly two practice train session IDs")
    practice_train_sessions = {
        _source_id(value, f"comparison manifest source_cohort."
                   f"practice_train_session_ids[{index}]")
        for index, value in enumerate(practice_train_session_values)
    }
    if len(practice_train_sessions) != 2:
        raise StudyError("comparison manifest practice train sessions must be distinct")
    if not practice_train_sessions <= set(train_sessions):
        raise StudyError(
            "comparison manifest practice sessions must belong to engineering train clusters")

    practice_values = comparison["practice"]
    cell_values = comparison["cells"]
    if not isinstance(practice_values, list) or len(practice_values) != 2:
        raise StudyError("comparison manifest practice must contain exactly two pairs")
    if not isinstance(cell_values, list) or len(cell_values) != 10:
        raise StudyError("comparison manifest cells must contain exactly cells 1-10")

    practice: dict[str, dict[str, Any]] = {}
    cells: dict[int, dict[str, Any]] = {}

    def pair_hashes(pair: dict[str, Any], label: str) -> dict[str, str]:
        physical = _filled_sha256(pair["physical_sha256"], f"{label}.physical_sha256")
        electry = _filled_sha256(pair["electry_sha256"], f"{label}.electry_sha256")
        if physical == electry:
            raise StudyError(f"{label} physical/electry sources are byte-identical")
        return {"physical_sha256": physical, "electry_sha256": electry}

    for index, pair_value in enumerate(practice_values, 1):
        pair_id = f"practice-{index}"
        label = f"comparison manifest practice[{index - 1}]"
        pair = _exact_keys(
            pair_value,
            {"id", "content", "processing", "frames", "source_session_id",
             "physical_sha256", "electry_sha256", "provenance", "qc"},
            label)
        if pair["id"] != pair_id:
            raise StudyError(f"{label}.id must be {pair_id!r}")
        session_id = _source_id(pair["source_session_id"], f"{label}.source_session_id")
        if session_id not in practice_train_sessions:
            raise StudyError(
                f"{label} source_session_id is not in practice_train_session_ids")
        content = _source_id(pair["content"], f"{label}.content")
        if pair["processing"] not in ("dry", "common_chain"):
            raise StudyError(f"{label}.processing must be dry or common_chain")
        frames = pair["frames"]
        if type(frames) is not int or frames <= 2205:
            raise StudyError(f"{label}.frames must be an exact positive frame count")
        source = all_sessions[session_id]
        practice[pair_id] = {
            "content": content,
            "frames": frames,
            "processing": pair["processing"],
            "provenance": _provenance(
                pair["provenance"], f"{label}.provenance", source,
                descriptor["path"], event_records, None),
            "qc": _qc_record(pair["qc"], f"{label}.qc", frames, None),
            "source_session_id": session_id,
            **pair_hashes(pair, label),
        }
    if ({pair["source_session_id"] for pair in practice.values()}
            != practice_train_sessions):
        raise StudyError(
            "comparison manifest practice_train_session_ids must exactly match practice sources")

    cluster_cell_counts = {
        cluster["cluster_id"]: 0 for cluster in holdout_clusters
    }
    for index, pair_value in enumerate(cell_values, 1):
        label = f"comparison manifest cells[{index - 1}]"
        pair = _exact_keys(
            pair_value,
            {"id", "content", "processing", "frames", "source_cluster_id",
             "source_session_id", "physical_sha256", "electry_sha256",
             "provenance", "qc"}, label)
        if type(pair["id"]) is not int or pair["id"] != index:
            raise StudyError(f"{label}.id must be {index}")
        cluster_id = _source_id(pair["source_cluster_id"], f"{label}.source_cluster_id")
        session_id = _source_id(pair["source_session_id"], f"{label}.source_session_id")
        if cluster_id not in cluster_ids or holdout_sessions.get(session_id) != cluster_id:
            raise StudyError(f"{label} source session/cluster is not in the holdout cohort")
        content, processing, selection_kind, hit_count = CELL_SEMANTICS[index]
        _require_exact(pair["content"], content, f"{label}.content")
        _require_exact(pair["processing"], processing, f"{label}.processing")
        _require_exact(pair["frames"], CORE_FRAMES[index], f"{label}.frames")
        source = all_sessions[session_id]
        cluster_cell_counts[cluster_id] += 1
        cells[index] = {
            "content": content,
            "frames": CORE_FRAMES[index],
            "processing": processing,
            "provenance": _provenance(
                pair["provenance"], f"{label}.provenance", source,
                descriptor["path"], event_records, selection_kind,
                CELL_CAPTURE_TAKES[index], 180 if index in (5, 6) else None),
            "qc": _qc_record(
                pair["qc"], f"{label}.qc", CORE_FRAMES[index], hit_count),
            "source_cluster_id": cluster_id,
            "source_session_id": session_id,
            **pair_hashes(pair, label),
        }
    if max(cluster_cell_counts.values()) - min(cluster_cell_counts.values()) > 1:
        raise StudyError("comparison manifest core cells are not balanced across holdout clusters")
    for dry_cell, processed_cell in ((5, 6), (7, 8), (9, 10)):
        for field in ("source_cluster_id", "source_session_id", "provenance"):
            if cells[dry_cell][field] != cells[processed_cell][field]:
                raise StudyError(
                    f"comparison manifest cells {dry_cell}/{processed_cell} must use the same phrase")

    freezes = _exact_keys(
        comparison["freezes"], {"selection", "render", "chain", "analysis"},
        "comparison manifest freezes")
    selection_value = _exact_keys(
        freezes["selection"],
        {"seed", "implementation_sha256", "settings", "settings_sha256", "receipt"},
        "comparison manifest freezes.selection")
    selection_seed = _private_seed(
        selection_value["seed"], "comparison manifest freezes.selection.seed")
    if selection_seed == presentation_seed:
        raise StudyError("selection and presentation seeds must be independently generated")
    selection = _settings_freeze(
        {key: selection_value[key]
         for key in ("implementation_sha256", "settings", "settings_sha256")},
        "comparison manifest freezes.selection")
    _require_exact(selection["settings"], SELECTION_SETTINGS,
                   "comparison manifest freezes.selection.settings")
    selection_receipt = _selection_receipt(
        descriptor["path"], selection_value["receipt"], study_id,
        selection_seed, practice, cells, all_sessions, holdout_sessions)
    render = _settings_freeze(freezes["render"], "comparison manifest freezes.render")
    _require_exact(render["settings"], RENDER_SETTINGS,
                   "comparison manifest freezes.render.settings")

    chain_value = _exact_keys(
        freezes["chain"],
        {"implementation_sha256", "preset_sha256", "assets", "settings",
         "settings_sha256"},
        "comparison manifest freezes.chain")
    chain = _settings_freeze(
        {key: chain_value[key]
         for key in ("implementation_sha256", "settings", "settings_sha256")},
        "comparison manifest freezes.chain")
    chain_settings = _exact_keys(
        chain["settings"], {"sample_rate_hz", "oversampling", "parameters"},
        "comparison manifest freezes.chain.settings")
    _require_exact(chain_settings["sample_rate_hz"], 44100,
                   "comparison manifest freezes.chain.settings.sample_rate_hz")
    _require_exact(chain_settings["oversampling"], 8,
                   "comparison manifest freezes.chain.settings.oversampling")
    _require_exact(chain_settings["parameters"], CHAIN_PARAMETERS,
                   "comparison manifest freezes.chain.settings.parameters")
    chain_preset_sha256 = _filled_sha256(
        chain_value["preset_sha256"],
        "comparison manifest freezes.chain.preset_sha256")
    assets = chain_value["assets"]
    if not isinstance(assets, list):
        raise StudyError("comparison manifest freezes.chain.assets must be a list")
    asset_ids: set[str] = set()
    asset_hashes: set[str] = set()
    for index, asset_value in enumerate(assets):
        label = f"comparison manifest freezes.chain.assets[{index}]"
        asset = _exact_keys(asset_value, {"id", "sha256"}, label)
        asset_id = _source_id(asset["id"], f"{label}.id")
        if asset_id in asset_ids:
            raise StudyError(f"comparison manifest repeats chain asset ID {asset_id}")
        asset_ids.add(asset_id)
        asset_hash = _filled_sha256(asset["sha256"], f"{label}.sha256")
        if asset_hash in asset_hashes:
            raise StudyError("comparison manifest repeats a chain asset SHA-256")
        asset_hashes.add(asset_hash)

    analysis_value = _exact_keys(
        freezes["analysis"],
        {"implementation_sha256", "listener_scorer_sha256", "settings",
         "settings_sha256"},
        "comparison manifest freezes.analysis")
    analysis = _settings_freeze(
        {key: analysis_value[key]
         for key in ("implementation_sha256", "settings", "settings_sha256")},
        "comparison manifest freezes.analysis")
    _require_exact(analysis["settings"], ANALYSIS_SETTINGS,
                   "comparison manifest freezes.analysis.settings")
    listener_scorer_sha256 = _filled_sha256(
        analysis_value["listener_scorer_sha256"],
        "comparison manifest freezes.analysis.listener_scorer_sha256")
    engineering = _engineering_freeze(comparison["engineering_freeze"])
    engineering_derivation = _engineering_derivation_receipt(
        descriptor["path"], engineering["derivation_receipt"], study_id,
        train_clusters, engineering)
    _require_exact(comparison["listener_protocol"], LISTENER_PROTOCOL,
                   "comparison manifest listener_protocol")
    registry = _artifact_registry(descriptor["path"], comparison["artifact_registry"])
    declared_artifacts = {
        selection["implementation_sha256"], render["implementation_sha256"],
        chain["implementation_sha256"], analysis["implementation_sha256"],
        listener_scorer_sha256, chain_preset_sha256, *asset_hashes,
        *engineering["artifact_hashes"],
        *engineering_derivation["analysis_result_hashes"],
    }
    missing_artifacts = declared_artifacts - registry["artifact_hashes"]
    if missing_artifacts:
        raise StudyError(
            "comparison manifest artifact registry omits a frozen implementation/build/asset")

    descriptor.update({
        "analysis_implementation_sha256": analysis["implementation_sha256"],
        "chain_implementation_sha256": chain["implementation_sha256"],
        "cells": cells,
        "capture_manifests": list(all_sessions.values()),
        "event_records": list(event_records.values()),
        "artifact_registry": registry,
        "engineering_derivation": engineering_derivation,
        "listener_scorer_sha256": listener_scorer_sha256,
        "participant_count": comparison["participant_count"],
        "practice": practice,
        "presentation_seed": presentation_seed,
        "render_implementation_sha256": render["implementation_sha256"],
        "schema": COMPARISON_SCHEMA,
        "selection_implementation_sha256": selection["implementation_sha256"],
        "selection_seed": selection_seed,
        "selection_receipt": selection_receipt,
        "status": "frozen",
        "study_id": study_id,
    })
    return descriptor


def _opaque(seed: str, *parts: object, length: int = 24) -> str:
    digest = hashlib.sha256(bytes.fromhex(seed))
    for part in parts:
        digest.update(b"\0")
        digest.update(str(part).encode("utf-8"))
    return digest.hexdigest()[:length]


def _bootstrap_seed(presentation_seed: str) -> str:
    digest = hashlib.sha256(bytes.fromhex(presentation_seed))
    digest.update(b"\0electry-listener-bootstrap/v1")
    return digest.hexdigest()


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


def _mapping_commitment(seed: str, mapping: dict[str, Any]) -> str:
    canonical = json.dumps(
        mapping, ensure_ascii=True, separators=(",", ":"), sort_keys=True).encode(
            "utf-8")
    digest = hashlib.sha256(bytes.fromhex(seed))
    digest.update(b"\0electry-private-mapping/v1\0")
    digest.update(canonical)
    return digest.hexdigest()


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
    raise StudyError(f"could not place hidden repeats for participant {participant}")


def _validate_study(manifest_path: Path) -> dict[str, Any]:
    manifest_bytes, manifest_value = _read_json_blob(
        manifest_path, "blind study manifest")
    study = _exact_keys(
        manifest_value,
        {"schema", "status", "study_id", "presentation_seed",
         "participant_count", "comparison_manifest", "practice", "cells"},
        "study")
    if study["schema"] != SCHEMA:
        raise StudyError(f"unexpected schema: {study['schema']}")
    if study["status"] != "frozen_ready_to_pack":
        raise StudyError(
            "study.status must be frozen_ready_to_pack; licensed finalized stimuli are still missing")
    if (not isinstance(study["study_id"], str)
            or not STUDY_ID_PATTERN.fullmatch(study["study_id"])
            or study["study_id"].startswith("REPLACE")):
        raise StudyError("study_id must be 1-80 safe filename characters")
    seed = _private_seed(study["presentation_seed"], "presentation_seed")
    if type(study["participant_count"]) is not int or study["participant_count"] != PARTICIPANT_COUNT:
        raise StudyError(f"participant_count must be the frozen value {PARTICIPANT_COUNT}")

    comparison = _comparison_descriptor(manifest_path, study["comparison_manifest"])
    practice_values = study["practice"]
    cell_values = study["cells"]
    if not isinstance(practice_values, list) or len(practice_values) != 2:
        raise StudyError("practice must contain exactly two pairs")
    if not isinstance(cell_values, list) or len(cell_values) != 10:
        raise StudyError("cells must contain exactly cells 1-10")

    assets: list[dict[str, Any]] = []
    practice: dict[str, dict[str, Any]] = {}
    cells: dict[int, dict[str, Any]] = {}

    def read_pair(value: Any, expected_id: str | int, label: str) -> dict[str, Any]:
        pair = _exact_keys(value, {"id", "physical", "electry"}, label)
        if type(pair["id"]) is not type(expected_id) or pair["id"] != expected_id:
            raise StudyError(f"{label}.id must be {expected_id!r}")
        result = {
            source: _descriptor(manifest_path, pair[source], f"{label}.{source}", wav=True)
            for source in ("physical", "electry")
        }
        if result["physical"]["frames"] != result["electry"]["frames"]:
            raise StudyError(f"{label} A/B stimuli must have equal frame counts")
        if (type(expected_id) is int
                and result["physical"]["frames"] != CORE_FRAMES[expected_id]):
            raise StudyError(
                f"{label} must contain exactly {CORE_FRAMES[expected_id]} frames")
        if result["physical"]["sha256"] == result["electry"]["sha256"]:
            raise StudyError(f"{label} A/B stimuli are byte-identical")
        for source, descriptor in result.items():
            assets.append({"pair": expected_id, "source": source, **descriptor})
        return result

    for index, value in enumerate(practice_values, 1):
        pair_id = f"practice-{index}"
        practice[pair_id] = read_pair(value, pair_id, f"practice[{index - 1}]")
    for index, value in enumerate(cell_values, 1):
        cells[index] = read_pair(value, index, f"cells[{index - 1}]")

    if comparison["study_id"] != study["study_id"]:
        raise StudyError("comparison manifest study_id does not match the blind study")
    if comparison["presentation_seed"] != seed:
        raise StudyError(
            "comparison manifest presentation_seed does not match the blind study")
    if comparison["participant_count"] != study["participant_count"]:
        raise StudyError(
            "comparison manifest participant_count does not match the blind study")
    for pair_id, study_pair in practice.items():
        comparison_pair = comparison["practice"][pair_id]
        if comparison_pair["frames"] != study_pair["physical"]["frames"]:
            raise StudyError(
                f"comparison manifest {pair_id} frame count does not match the blind study")
        for source in ("physical", "electry"):
            if comparison_pair[f"{source}_sha256"] != study_pair[source]["sha256"]:
                raise StudyError(
                    f"comparison manifest {pair_id} {source} SHA-256 does not match the blind study")
    for cell, study_pair in cells.items():
        comparison_pair = comparison["cells"][cell]
        for source in ("physical", "electry"):
            if comparison_pair[f"{source}_sha256"] != study_pair[source]["sha256"]:
                raise StudyError(
                    f"comparison manifest cell {cell} {source} SHA-256 does not match the blind study")

    paths = [item["path"] for item in assets]
    hashes = [item["sha256"] for item in assets]
    if len(set(paths)) != len(paths) or len(set(hashes)) != len(hashes):
        raise StudyError("every non-repeat practice/core stimulus must be unique")

    return {
        "study_id": study["study_id"],
        "seed": seed,
        "manifest_bytes": manifest_bytes,
        "manifest_sha256": hashlib.sha256(manifest_bytes).hexdigest(),
        "comparison": comparison,
        "practice": practice,
        "cells": cells,
        "assets": assets,
    }


def _write_json(path: Path, value: Any) -> None:
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n",
                    encoding="utf-8")


def prepare(manifest_path: Path, output_path: Path) -> str:
    manifest_path = manifest_path.resolve()
    output_path = output_path.resolve()
    if output_path.exists():
        raise StudyError(f"refusing to overwrite existing output: {output_path}")
    study = _validate_study(manifest_path)
    manifest_digest = study["manifest_sha256"]
    preparer_path = Path(__file__).resolve()
    runner_path = Path(__file__).with_name("BlindListening.html")
    if not runner_path.is_file():
        raise StudyError(f"missing browser runner: {runner_path}")
    scorer_path = Path(__file__).with_name("ScoreBlindListening.py")
    if not scorer_path.is_file():
        raise StudyError(f"missing frozen result scorer: {scorer_path}")
    server_path = Path(__file__).with_name("ServeBlindListening.py")
    if not server_path.is_file():
        raise StudyError(f"missing no-listing study server: {server_path}")
    try:
        preparer_bytes = preparer_path.read_bytes()
        runner_bytes = runner_path.read_bytes()
        scorer_bytes = scorer_path.read_bytes()
        server_bytes = server_path.read_bytes()
    except OSError as error:
        raise StudyError(f"could not snapshot study implementation: {error}") from error
    tool_digest = hashlib.sha256(preparer_bytes).hexdigest()
    runner_digest = hashlib.sha256(runner_bytes).hexdigest()
    scorer_digest = hashlib.sha256(scorer_bytes).hexdigest()
    server_digest = hashlib.sha256(server_bytes).hexdigest()
    if scorer_digest != study["comparison"]["listener_scorer_sha256"]:
        raise StudyError(
            "frozen comparison listener_scorer_sha256 does not match ScoreBlindListening.py")
    if tool_digest != study["comparison"]["selection_implementation_sha256"]:
        raise StudyError(
            "frozen selection implementation does not match archived PrepareBlindListening.py")
    fingerprint = hashlib.sha256(
        (f"{manifest_digest}:{tool_digest}:{runner_digest}:{scorer_digest}:"
         f"{server_digest}").encode("ascii")).hexdigest()

    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=f".{output_path.name}.",
                                      dir=output_path.parent))
    try:
        public = temporary / "public"
        sessions = public / "sessions"
        stimuli = public / "stimuli"
        private = temporary / "private"
        sessions.mkdir(parents=True)
        stimuli.mkdir()
        private.mkdir(mode=0o700)
        private.chmod(0o700)
        (public / "index.html").write_bytes(runner_bytes)
        (temporary / "score.py").write_bytes(scorer_bytes)
        (temporary / "serve.py").write_bytes(server_bytes)
        comparison_archive = private / "comparison-manifest.json"
        comparison_archive.write_bytes(study["comparison"]["bytes"])
        comparison_archive.chmod(0o600)
        blind_study_archive = private / "study-manifest.json"
        blind_study_archive.write_bytes(study["manifest_bytes"])
        blind_study_archive.chmod(0o600)
        preparer_archive = private / "prepare.py"
        preparer_archive.write_bytes(preparer_bytes)
        preparer_archive.chmod(0o600)
        capture_archive = private / "capture-manifests"
        capture_archive.mkdir(mode=0o700)
        capture_archive.chmod(0o700)
        for capture in study["comparison"]["capture_manifests"]:
            destination = private / capture["archive_name"]
            destination.write_bytes(capture["bytes"])
            destination.chmod(0o600)
        event_archive = private / "event-scores"
        event_archive.mkdir(mode=0o700)
        event_archive.chmod(0o700)
        for event in study["comparison"]["event_records"]:
            destination = private / event["archive_name"]
            destination.write_bytes(event["bytes"])
            destination.chmod(0o600)
        engineering_derivation = study["comparison"]["engineering_derivation"]
        engineering_receipt = engineering_derivation["receipt"]
        engineering_receipt_archive = private / engineering_receipt["archive_name"]
        engineering_receipt_archive.write_bytes(engineering_receipt["bytes"])
        engineering_receipt_archive.chmod(0o600)
        engineering_result = engineering_derivation["result"]
        engineering_result_archive = private / engineering_result["archive_name"]
        engineering_result_archive.write_bytes(engineering_result["bytes"])
        engineering_result_archive.chmod(0o600)
        train_analysis_archive = private / "engineering-train-analysis"
        train_analysis_archive.mkdir(mode=0o700)
        train_analysis_archive.chmod(0o700)
        for analysis_result in engineering_derivation["train_analysis"]:
            destination = private / analysis_result["archive_name"]
            destination.write_bytes(analysis_result["bytes"])
            destination.chmod(0o600)
        registry = study["comparison"]["artifact_registry"]
        registry_archive = private / registry["archive_name"]
        registry_archive.write_bytes(registry["bytes"])
        registry_archive.chmod(0o600)
        receipt = study["comparison"]["selection_receipt"]
        receipt_archive = private / receipt["archive_name"]
        receipt_archive.write_bytes(receipt["bytes"])
        receipt_archive.chmod(0o600)

        first_copy: dict[str, Path] = {}
        key_participants: list[dict[str, Any]] = []
        side_a = {
            pair_id: _physical_a_participants(study["seed"], pair_id)
            for pair_id in ["practice-1", "practice-2", *range(1, 11)]
        }
        private_links = ["participant_id\tlistener_stratum\tsession_token\tquery"]

        def emit_audio(source: dict[str, Any], participant: int,
                       section: str, position: int, side: str) -> str:
            name = _opaque(study["seed"], "audio", participant, section,
                           position, side) + ".wav"
            destination = stimuli / name
            source_hash = source["sha256"]
            if source_hash in first_copy:
                try:
                    os.link(first_copy[source_hash], destination)
                except OSError:
                    shutil.copyfile(first_copy[source_hash], destination)
            else:
                destination.write_bytes(source["bytes"])
                if _sha256(destination) != source_hash:
                    raise StudyError(f"could not preserve stimulus snapshot: {source['path']}")
                first_copy[source_hash] = destination
            return f"stimuli/{name}"

        for participant in range(1, PARTICIPANT_COUNT + 1):
            participant_id = f"p{participant:03d}"
            listener_stratum = ("extended_range_guitarist"
                                if participant <= 15 else "metal_producer")
            session_token = _opaque(study["seed"], "session", participant,
                                    length=32)
            public_practice: list[dict[str, Any]] = []
            public_trials: list[dict[str, Any]] = []
            key_practice: list[dict[str, Any]] = []
            key_trials: list[dict[str, Any]] = []

            practice_order = sorted(study["practice"], key=lambda pair_id: _opaque(
                study["seed"], "practice-order", participant, pair_id, length=64))
            for position, pair_id in enumerate(practice_order, 1):
                pair = study["practice"][pair_id]
                physical_on_a = participant in side_a[pair_id]
                a_source = "physical" if physical_on_a else "electry"
                b_source = "electry" if physical_on_a else "physical"
                trial_id = _opaque(study["seed"], "trial", participant,
                                   "practice", position, pair_id, length=20)
                public_record = {
                    "a": emit_audio(pair[a_source], participant, "practice", position, "a"),
                    "b": emit_audio(pair[b_source], participant, "practice", position, "b"),
                    "trial_id": trial_id,
                }
                public_practice.append(public_record)
                key_practice.append({
                    "a_sha256": pair[a_source]["sha256"],
                    "a_source": a_source,
                    "b_sha256": pair[b_source]["sha256"],
                    "b_source": b_source,
                    "pair": pair_id,
                    "position": position,
                    "trial_id": trial_id,
                })

            for position, (cell, repeated) in enumerate(
                    _scored_order(study["seed"], participant), 1):
                pair = study["cells"][cell]
                physical_on_a = participant in side_a[cell]
                if repeated:
                    physical_on_a = not physical_on_a
                a_source = "physical" if physical_on_a else "electry"
                b_source = "electry" if physical_on_a else "physical"
                variant = "repeat" if repeated else "original"
                trial_id = _opaque(study["seed"], "trial", participant,
                                   "scored", position, cell, variant, length=20)
                public_record = {
                    "a": emit_audio(pair[a_source], participant, "scored", position, "a"),
                    "b": emit_audio(pair[b_source], participant, "scored", position, "b"),
                    "trial_id": trial_id,
                }
                public_trials.append(public_record)
                key_trials.append({
                    "a_sha256": pair[a_source]["sha256"],
                    "a_source": a_source,
                    "b_sha256": pair[b_source]["sha256"],
                    "b_source": b_source,
                    "cell": cell,
                    "position": position,
                    "repeat_of": cell if repeated else None,
                    "trial_id": trial_id,
                })

            private_mapping = {
                "listener_stratum": listener_stratum,
                "participant_id": participant_id,
                "practice": key_practice,
                "session_token": session_token,
                "trials": key_trials,
            }
            mapping_commitment = _mapping_commitment(study["seed"], private_mapping)
            session = {
                "mapping_commitment": mapping_commitment,
                "max_replays": MAX_REPLAYS,
                "practice": public_practice,
                "schema": SESSION_SCHEMA,
                "session_token": session_token,
                "study_fingerprint": fingerprint,
                "study_id": study["study_id"],
                "trials": public_trials,
            }
            _write_json(sessions / f"{session_token}.json", session)
            key_participants.append({
                **private_mapping,
                "mapping_commitment": mapping_commitment,
            })
            private_links.append(
                f"{participant_id}\t{listener_stratum}\t{session_token}\t"
                f"?session={session_token}")

        links_bytes = ("\n".join(private_links) + "\n").encode("utf-8")
        key = {
            "algorithm": {
                "hidden_repeats": "cells 5, 7 and 9; later; at least three intervening trials; A/B reversed",
                "order": "SHA-256 rank with deterministic rejection",
                "side_balance": "per pair and stratum, seeded SHA-256 ranks IDs; odd pairs assign 8/7 physical=A and even pairs 7/8",
            },
            "analysis": {
                "bootstrap_replicates": BOOTSTRAP_REPLICATES,
                "bootstrap_seed_sha256": _bootstrap_seed(study["seed"]),
                "schema": ANALYSIS_SCHEMA,
            },
            "archives": {
                "artifact_registry": {
                    "path": f"private/{registry['archive_name']}",
                    "sha256": registry["sha256"],
                },
                "engineering_derivation_receipt": {
                    "path": f"private/{engineering_receipt['archive_name']}",
                    "sha256": engineering_receipt["sha256"],
                },
                "engineering_derivation_result": {
                    "path": f"private/{engineering_result['archive_name']}",
                    "sha256": engineering_result["sha256"],
                },
                "engineering_train_analysis": [
                    {
                        "cluster_id": analysis_result["cluster_id"],
                        "path": f"private/{analysis_result['archive_name']}",
                        "sha256": analysis_result["sha256"],
                    }
                    for analysis_result in engineering_derivation["train_analysis"]
                ],
                "capture_manifests": [
                    {
                        "path": f"private/{capture['archive_name']}",
                        "session_id": capture["session_id"],
                        "sha256": capture["sha256"],
                    }
                    for capture in sorted(
                        study["comparison"]["capture_manifests"],
                        key=lambda value: value["session_id"])
                ],
                "event_records": [
                    {
                        "path": f"private/{event['archive_name']}",
                        "sha256": event["sha256"],
                    }
                    for event in sorted(
                        study["comparison"]["event_records"],
                        key=lambda value: value["sha256"])
                ],
                "preparer": {
                    "path": "private/prepare.py",
                    "sha256": tool_digest,
                },
                "selection_receipt": {
                    "path": f"private/{receipt['archive_name']}",
                    "sha256": receipt["sha256"],
                },
            },
            "comparison_manifest": {
                "path": "private/comparison-manifest.json",
                "schema": study["comparison"]["schema"],
                "sha256": study["comparison"]["sha256"],
                "status": study["comparison"]["status"],
            },
            "implementation": {
                "preparer_sha256": tool_digest,
                "runner_sha256": runner_digest,
                "scorer_sha256": scorer_digest,
                "server_sha256": server_digest,
            },
            "participants": key_participants,
            "participant_links": {
                "path": "private/participant-links.tsv",
                "sha256": hashlib.sha256(links_bytes).hexdigest(),
            },
            "presentation_seed": study["seed"],
            "schema": KEY_SCHEMA,
            "source_manifest": {
                "path": "private/study-manifest.json",
                "sha256": manifest_digest,
            },
            "sources": [
                {
                    "frames": item["frames"],
                    "pair": item["pair"],
                    "path": str(item["path"]),
                    "sha256": item["sha256"],
                    "source": item["source"],
                }
                for item in study["assets"]
            ],
            "study_fingerprint": fingerprint,
            "study_id": study["study_id"],
        }
        key_path = private / "answer-key.json"
        _write_json(key_path, key)
        key_path.chmod(0o600)
        links_path = private / "participant-links.tsv"
        links_path.write_bytes(links_bytes)
        links_path.chmod(0o600)

        (public / "README.txt").write_text(
            "Run `python3 serve.py --expected-fingerprint "
            "<externally-recorded-64-hex-fingerprint>` from the pack root; "
            "do not use a directory-listing server.\n"
            "Open only the opaque URL assigned by the coordinator.\n"
            "Never expose the private directory or participant-links.tsv.\n",
            encoding="utf-8")
        os.replace(temporary, output_path)
    except Exception:
        shutil.rmtree(temporary, ignore_errors=True)
        raise
    return fingerprint


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--generate-selection-receipt", action="store_true",
        help="treat input as electry-blind-selection-input/v1 and write a receipt JSON")
    parser.add_argument("manifest", type=Path, help="private study or selection input JSON")
    parser.add_argument("output", type=Path, help="new pack directory or receipt JSON")
    arguments = parser.parse_args()
    try:
        if arguments.generate_selection_receipt:
            generate_selection_receipt(arguments.manifest, arguments.output)
            print(f"Wrote seeded selection plan and receipt to {arguments.output.resolve()}")
            return 0
        fingerprint = prepare(arguments.manifest, arguments.output)
    except StudyError as error:
        parser.error(str(error))
    print(f"Prepared blinded study pack at {arguments.output.resolve()}")
    print(f"STUDY_FINGERPRINT={fingerprint}")
    print("Record this fingerprint in an external append-only location before listening.")
    print("Run its serve.py with --expected-fingerprint set to that external value; "
          "keep private/answer-key.json and participant-links.tsv sealed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
