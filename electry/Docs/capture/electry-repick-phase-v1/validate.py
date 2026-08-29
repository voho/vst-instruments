#!/usr/bin/env python3
"""Validate Electry phase-repick templates or a filled freeze chain."""

from __future__ import annotations

import argparse
import array
import hashlib
import json
import math
import shutil
import struct
import subprocess
import sys
from collections import Counter
from pathlib import Path
from statistics import median


HERE = Path(__file__).resolve().parent
CELLS = {
    (string, direction, phase)
    for string in ("e1", "e2")
    for direction in ("down", "up")
    for phase in (0.0, 90.0, 180.0, 270.0)
}
PROFILES = {(string, direction) for string in ("e1", "e2") for direction in ("down", "up")}
RAIL_KEYS = (
    "fresh_note_identity",
    "energy_nonincrease",
    "lifecycle",
    "level",
    "tuning",
    "stability",
)
CANONICALIZATION = "UTF-8 Python json.dumps(value,sort_keys=True,separators=(',',':'),ensure_ascii=False,allow_nan=False)"
ANALYSIS_POINTERS = (
    "/sensors/relative_latency_and_phase_calibration/calibration_half_width_calculation",
    "/acquisition_analysis_freeze/model_render_contract",
    "/acquisition_analysis_freeze/engine_coordinate_mapping",
    "/acquisition_analysis_freeze/local_f0_estimator",
    "/acquisition_analysis_freeze/window_fit",
    "/acquisition_analysis_freeze/phase_assignment",
    "/acquisition_analysis_freeze/event_phase_uncertainty",
    "/acquisition_analysis_freeze/eligibility",
    "/acquisition_analysis_freeze/primary_response",
    "/acquisition_analysis_freeze/registered_statistics",
)
EXCLUSION_REASONS = (
    "ioi_outside_75_130_ms",
    "contact_onset_half_width_above_1_ms",
    "f0_interval_touches_search_boundary",
    "phase_interval_not_wholly_inside_one_quadrant",
    "pre_normal_motion_h1_snr_below_12_db",
    "pre_bridge_di_h1_h4_snr_below_12_db",
    "extra_transient",
    "clipping_or_nonfinite",
)
INSUFFICIENT = "insufficient_events"
INCONCLUSIVE = "inconclusive_missing_registered_cell"


class Invalid(ValueError):
    pass


def unique_object(pairs: list[tuple[str, object]]) -> dict:
    value = {}
    for key, item in pairs:
        if key in value:
            raise Invalid(f"duplicate JSON key: {key}")
        value[key] = item
    return value


def require(condition: bool, message: str) -> None:
    if not condition:
        raise Invalid(message)


def read_json(path: Path) -> dict:
    try:
        return json.loads(
            path.read_text(encoding="utf-8"),
            parse_constant=lambda value: (_ for _ in ()).throw(Invalid(f"non-finite JSON value {value}")),
            object_pairs_hook=unique_object,
        )
    except (OSError, json.JSONDecodeError) as exc:
        raise Invalid(f"{path}: {exc}") from exc


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def hex_digest(value, length: int) -> bool:
    return isinstance(value, str) and len(value) == length and all(character in "0123456789abcdef" for character in value)


def committed_file_sha256(commit: str, path: Path) -> str:
    git = shutil.which("git")
    require(git is not None, "git is required to verify the frozen protocol commit")
    root_result = subprocess.run([git, "-C", str(HERE), "rev-parse", "--show-toplevel"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    require(root_result.returncode == 0, "capture validator is not inside the protocol Git repository")
    root = Path(root_result.stdout.decode("utf-8").strip()).resolve()
    try:
        relative = path.resolve().relative_to(root).as_posix()
    except ValueError as exc:
        raise Invalid(f"protocol file outside Git repository: {path}") from exc
    blob = subprocess.run([git, "-C", str(root), "show", f"{commit}:{relative}"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    require(blob.returncode == 0, f"protocol commit {commit} does not contain {relative}")
    return hashlib.sha256(blob.stdout).hexdigest()


def resolve(owner: Path, value: str) -> Path:
    path = Path(value)
    return path if path.is_absolute() else (owner.parent / path).resolve()


def check_file_hash(owner: Path, file_value: str, expected: str, role: str) -> Path:
    path = resolve(owner, file_value)
    require(path.is_file(), f"{role}: missing file {path}")
    require(sha256(path) == expected, f"{role}: SHA-256 mismatch for {path}")
    return path


def scan_filled(value, where: str = "$") -> None:
    if value is None:
        raise Invalid(f"{where}: null placeholder remains")
    if isinstance(value, float) and not math.isfinite(value):
        raise Invalid(f"{where}: non-finite number")
    if isinstance(value, str):
        if "REPLACE" in value:
            raise Invalid(f"{where}: REPLACE placeholder remains")
        if len(value) in (40, 64) and set(value) == {"0"}:
            raise Invalid(f"{where}: zero hash placeholder remains")
    elif isinstance(value, list):
        for index, item in enumerate(value):
            scan_filled(item, f"{where}[{index}]")
    elif isinstance(value, dict):
        for key, item in value.items():
            if key == "instructions":
                continue
            scan_filled(item, f"{where}.{key}")


def template_placeholder(value) -> bool:
    return value is None or (
        isinstance(value, str)
        and ("REPLACE" in value or (len(value) in (40, 64) and set(value) == {"0"}))
    )


def validate_template_shape(value, template, where: str) -> None:
    if template_placeholder(template):
        return
    if isinstance(template, dict):
        require(isinstance(value, dict) and set(value) == set(template), f"{where}: keys differ from committed template")
        for key, item in template.items():
            validate_template_shape(value[key], item, f"{where}.{key}")
    elif isinstance(template, list):
        require(isinstance(value, list), f"{where}: expected array")
        if template:
            require(len(value) == len(template), f"{where}: array length differs from committed template")
            for index, item in enumerate(template):
                validate_template_shape(value[index], item, f"{where}[{index}]")
    else:
        require(value == template and type(value) is type(template), f"{where}: fixed value differs from committed template")


def canonical_hash(value) -> str:
    encoded = json.dumps(
        value,
        sort_keys=True,
        separators=(",", ":"),
        ensure_ascii=False,
        allow_nan=False,
    ).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def json_pointer(value, pointer: str):
    current = value
    for raw in pointer.split("/")[1:]:
        token = raw.replace("~1", "/").replace("~0", "~")
        current = current[int(token)] if isinstance(current, list) else current[token]
    return current


def analysis_contract(manifest: dict) -> dict:
    return {pointer: json_pointer(manifest, pointer) for pointer in ANALYSIS_POINTERS}


def wave_info(path: Path) -> tuple[int, int, int, int]:
    with path.open("rb") as stream:
        file_size = path.stat().st_size
        header = stream.read(12)
        require(len(header) == 12 and header[:4] == b"RIFF" and header[8:] == b"WAVE", f"{path}: not RIFF/WAVE")
        require(struct.unpack("<I", header[4:8])[0] + 8 == file_size, f"{path}: RIFF size/trailing data mismatch")
        fmt = None
        fmt_count = 0
        data_chunks = []
        while True:
            chunk_header = stream.read(8)
            if not chunk_header:
                break
            require(len(chunk_header) == 8, f"{path}: truncated chunk header")
            chunk_id, size = struct.unpack("<4sI", chunk_header)
            offset = stream.tell()
            require(offset + size + (size & 1) <= file_size, f"{path}: chunk payload/pad exceeds RIFF size")
            if chunk_id == b"fmt ":
                fmt_count += 1
                payload = stream.read(size)
                require(len(payload) == size and size >= 16, f"{path}: invalid fmt chunk")
                tag, channels, rate, byte_rate, block_align, bits = struct.unpack("<HHIIHH", payload[:16])
                if tag == 0xFFFE:
                    float_guid = bytes.fromhex("0300000000001000800000aa00389b71")
                    require(size >= 40 and struct.unpack("<H", payload[16:18])[0] >= 22 and struct.unpack("<H", payload[18:20])[0] == 32 and payload[24:40] == float_guid, f"{path}: extensible subtype is not 32-bit IEEE float")
                    tag = 3
                fmt = (tag, channels, rate, byte_rate, block_align, bits)
            elif chunk_id == b"data":
                data_chunks.append((offset, size))
                stream.seek(size, 1)
            else:
                stream.seek(size, 1)
            if size & 1:
                require(len(stream.read(1)) == 1, f"{path}: missing chunk pad byte")
        require(fmt is not None and fmt_count == 1 and len(data_chunks) == 1, f"{path}: require one fmt and one data chunk")
        tag, channels, rate, byte_rate, block_align, bits = fmt
        require((tag, channels, rate, bits) == (3, 4, 96000, 32), f"{path}: require float32, four-channel, 96 kHz")
        require(block_align == 16 and byte_rate == 1536000, f"{path}: invalid WAVE alignment/rate")
        offset, data_size = data_chunks[0]
        require(data_size % block_align == 0, f"{path}: partial frame")
        stream.seek(offset)
        remaining = data_size
        while remaining:
            payload = stream.read(min(1024 * 1024, remaining))
            require(payload and len(payload) % 4 == 0, f"{path}: truncated float data")
            samples = array.array("f")
            samples.frombytes(payload)
            if sys.byteorder != "little":
                samples.byteswap()
            require(all(math.isfinite(sample) for sample in samples), f"{path}: non-finite sample")
            remaining -= len(payload)
        return channels, rate, bits, data_size // block_align


def validate_trials(manifest: dict, source: str) -> None:
    performance = manifest["performance"]
    require(performance["pairs_per_string"] == 64, f"{source}: pairs_per_string")
    require(performance["pairs_per_target_phase"] == 16, f"{source}: pairs_per_target_phase")
    require(performance["second_strokes_per_phase"] == {"down": 8, "up": 8}, f"{source}: direction balance")
    trial_keys = set(performance["trial_order"]["required_item_schema"])
    for target in ("e1", "e2"):
        trials = performance["trial_order"][target]
        require(len(trials) == 64, f"{source}: {target} needs 64 trials")
        require(all(isinstance(item, dict) and set(item) == trial_keys for item in trials), f"{source}: {target} trial schema")
        require(all(isinstance(item["trial"], int) and not isinstance(item["trial"], bool) for item in trials), f"{source}: {target} trial number types")
        require(all(finite_number(item["target_phase_fraction"]) for item in trials), f"{source}: {target} cue phase types")
        require({item["trial"] for item in trials} == set(range(1, 65)), f"{source}: {target} trial numbers")
        phase_counts = Counter(item["target_phase_fraction"] for item in trials)
        require(phase_counts == Counter({0.0: 16, 0.25: 16, 0.5: 16, 0.75: 16}), f"{source}: {target} cue balance")
        for phase in (0.0, 0.25, 0.5, 0.75):
            directions = Counter(item["second_stroke"] for item in trials if item["target_phase_fraction"] == phase)
            require(directions == Counter({"down": 8, "up": 8}), f"{source}: {target}/{phase} direction balance")
        require(
            all(item["first_stroke"] != item["second_stroke"] and {item["first_stroke"], item["second_stroke"]} == {"down", "up"} for item in trials),
            f"{source}: first stroke must oppose second",
        )


def stable_setup(manifest: dict) -> dict:
    instrument = manifest["instrument"]
    performance = manifest["performance"]
    sensors = manifest["sensors"]
    return {
        "session": {key: manifest["session"][key] for key in ("player_id", "rig_id")},
        "instrument": {key: value for key, value in instrument.items() if key != "string_age_hours_at_start"},
        "pick": {key: performance[key] for key in (
            "articulation", "force_instruction", "force_repeatability_method", "pick_make_material",
            "pick_thickness_mm", "pick_tip_geometry", "pick_grip_exposure_mm", "pick_contact_distance_from_saddle_mm",
        )},
        "damping_geometry": {key: value for key, value in performance["non_target_string_damping"].items() if key != "residual_ring_verification"},
        "contact_reference": {key: value for key, value in sensors["contact_reference"].items() if key != "measured_onset_half_width_ms"},
        "sensor_geometry": {key: sensors[key] for key in ("plectrum_normal_motion", "tangential_motion")},
        "signal_chain": manifest["signal_chain"],
    }


def validate_manifest(path: Path, expected: dict, study_id: str) -> dict:
    manifest = read_json(path)
    validate_template_shape(manifest, read_json(HERE / "manifest.template.json"), str(path))
    scan_filled(manifest)
    require(manifest["schema"] == "electry-repick-phase-capture/v1", f"{path}: schema")
    require(manifest["session"]["study_id"] == study_id, f"{path}: study_id")
    require(manifest["session"]["cluster_id"] == expected["cluster_id"], f"{path}: cluster_id")
    require(manifest["session"]["split"] == expected["split"], f"{path}: split")
    require(manifest["acquisition_analysis_freeze"]["status"] == "frozen_before_train_decode", f"{path}: freeze status")
    require(manifest["audio_format"] == {
        "container": "WAVE", "encoding": "IEEE_FLOAT", "bits_per_sample": 32, "channels": 4,
        "sample_rate_hz": 96000,
        "channel_order": ["bridge_pickup_di", "contact_onset_reference", "plectrum_normal_motion", "tangential_motion"],
        "one_sample_clock": True, "normalization_applied": False, "processing": "none",
    }, f"{path}: audio format")
    rights = manifest["session"]["rights"]
    require(rights["commercial_model_calibration"] is True and rights["private_competitive_evaluation"] is True and isinstance(rights["redistribution_allowed"], bool), f"{path}: mandatory rights")
    onset_half_width = manifest["sensors"]["contact_reference"]["measured_onset_half_width_ms"]
    require(isinstance(onset_half_width, (int, float)) and not isinstance(onset_half_width, bool) and math.isfinite(onset_half_width) and 0.0 <= onset_half_width <= 1.0, f"{path}: contact-onset uncertainty")
    latency_calibration = manifest["sensors"]["relative_latency_and_phase_calibration"]
    require(all(manifest["sensors"][axis]["quantity"] in ("velocity", "displacement") for axis in ("plectrum_normal_motion", "tangential_motion")), f"{path}: motion sensor quantity")
    require(finite_number(manifest["sensors"]["contact_reference"]["added_mass_mg"]) and manifest["sensors"]["contact_reference"]["added_mass_mg"] >= 0.0, f"{path}: contact sensor added mass")
    require(all(finite_number(manifest["sensors"][axis]["reflective_target_mass_mg"]) and manifest["sensors"][axis]["reflective_target_mass_mg"] >= 0.0 and all(finite_number(value) and value > 0.0 for value in manifest["sensors"][axis]["pick_point_target_distance_from_saddle_mm"].values()) for axis in ("plectrum_normal_motion", "tangential_motion")), f"{path}: motion sensor geometry")
    require(all(finite_number(value) for value in latency_calibration["latency_samples_at_96khz_by_channel"]), f"{path}: calibrated channel latencies")
    require(all(finite_number(value) and value >= 0.0 for value in latency_calibration["latency_half_width_samples_by_channel"]), f"{path}: latency uncertainty")
    require(all(finite_number(value) and value >= 0.0 for row in latency_calibration["phase_response_half_width_degrees_by_channel_h1_h4"] for value in row), f"{path}: phase-response uncertainty")
    require(all(finite_number(latency_calibration["calibration_phase_half_width_degrees_by_string"][string]) and latency_calibration["calibration_phase_half_width_degrees_by_string"][string] >= 0.0 for string in ("e1", "e2")), f"{path}: combined calibration phase uncertainty")
    instrument = manifest["instrument"]
    require(finite_number(instrument["scale_length_inches"]) and instrument["scale_length_inches"] > 0.0 and finite_number(instrument["string_age_hours_at_start"]) and instrument["string_age_hours_at_start"] >= 0.0 and all(finite_number(value) and value > 0.0 for value in instrument["string_gauges_inches_low_to_high"]), f"{path}: instrument dimensions/string age")
    require(all(finite_number(instrument[field][string]) and instrument[field][string] >= 0.0 for field in ("bridge_pickup_sensing_centre_distance_from_saddle_mm", "bridge_pickup_open_string_gap_mm") for string in ("e1", "e2")), f"{path}: pickup geometry")
    performance = manifest["performance"]
    require(all(finite_number(performance[field]) and performance[field] > 0.0 for field in ("pick_thickness_mm", "pick_grip_exposure_mm")) and all(finite_number(performance["pick_contact_distance_from_saddle_mm"][string]) and performance["pick_contact_distance_from_saddle_mm"][string] > 0.0 for string in ("e1", "e2")), f"{path}: pick geometry")
    require(all(close(manifest["sensors"][axis]["pick_point_target_distance_from_saddle_mm"][string], performance["pick_contact_distance_from_saddle_mm"][string]) for axis in ("plectrum_normal_motion", "tangential_motion") for string in ("e1", "e2")), f"{path}: motion target is not at registered pick point")
    require(all(close(latency_calibration["calibration_phase_half_width_degrees_by_string"][string], phase_calibration_half_width(manifest, performance["session_f0_hz"][string], onset_half_width)) for string in ("e1", "e2")), f"{path}: combined calibration phase uncertainty calculation")
    for string, midi in (("e1", 28), ("e2", 40)):
        cue_f0 = performance["session_f0_hz"][string]
        nominal = 440.0 * 2.0 ** ((midi - 69) / 12.0)
        require(finite_number(cue_f0) and nominal * 2.0 ** (-50.0 / 1200.0) < cue_f0 < nominal * 2.0 ** (50.0 / 1200.0), f"{path}: {string} cue f0")
    damping_geometry = performance["non_target_string_damping"]
    require(all(finite_number(value) and value > 0.0 for key in ("contact_positions_from_saddle_mm_when_e1_target", "contact_positions_from_saddle_mm_when_e2_target") for value in damping_geometry[key]), f"{path}: damping contact positions")
    signal_chain = manifest["signal_chain"]
    require(finite_number(signal_chain["input_impedance_ohms"]) and signal_chain["input_impedance_ohms"] > 0.0 and all(finite_number(value) for value in signal_chain["interface_gain_db_by_channel"]), f"{path}: interface measurements")
    require(finite_number(signal_chain["guitar_output_cable_length_m"]) and signal_chain["guitar_output_cable_length_m"] > 0.0 and finite_number(signal_chain["guitar_output_cable_total_capacitance_pf"]) and signal_chain["guitar_output_cable_total_capacitance_pf"] > 0.0, f"{path}: cable measurements")
    validate_trials(manifest, str(path))
    damping = manifest["performance"]["non_target_string_damping"]["residual_ring_verification"]
    require(damping["method"] == "three_hard_calibration_strokes_per_string_bridge_di_rms", f"{path}: damping method")
    require(damping["window_ms"] == [50.0, 200.0] and damping["reference"] == "median_undamped_target_string_rms_same_force_instruction", f"{path}: damping window/reference")
    require(damping["maximum_each_damped_string_relative_db"] == -40.0, f"{path}: damping threshold")
    damping_keys = ("e1_target_results_db_physical_strings_7_to_1_each_three_strokes", "e2_target_results_db_physical_strings_8_7_5_4_3_2_1_each_three_strokes")
    require(all(len(damping[key]) == 7 and all(len(row) == 3 and all(finite_number(value) for value in row) for row in damping[key]) for key in damping_keys), f"{path}: damping needs three finite strokes for each of seven strings per target")
    require(damping["all_results_pass"] is True, f"{path}: non-target damping did not pass")
    require(all(value <= -40.0 for key in damping_keys for row in damping[key] for value in row), f"{path}: non-target residual stroke above -40 dB")
    check_file_hash(path, manifest["session"]["rights"]["agreement_file"], manifest["session"]["rights"]["agreement_sha256"], f"{path}: rights agreement")
    damping_path = check_file_hash(path, damping["receipt_file"], damping["receipt_sha256"], f"{path}: damping receipt")
    damping_receipt = read_json(damping_path)
    validate_template_shape(damping_receipt, read_json(HERE / "damping-receipt.template.json"), str(damping_path))
    scan_filled(damping_receipt)
    require(damping_receipt["schema"] == "electry-repick-phase-damping-result/v1" and damping_receipt["status"] == "sealed_before_scored_pairs", f"{path}: damping receipt schema/status")
    require(damping_receipt["study_id"] == manifest["session"]["study_id"] and damping_receipt["cluster_id"] == manifest["session"]["cluster_id"], f"{path}: damping receipt identity")
    for key in ("method", "window_ms", "reference", "maximum_each_damped_string_relative_db", *damping_keys, "all_results_pass"):
        require(damping_receipt[key] == damping[key], f"{path}: damping receipt differs at {key}")
    cue = manifest["performance"]["phase_locked_cue"]
    check_file_hash(path, cue["pre_session_pilot_file"], cue["pre_session_pilot_sha256"], f"{path}: cue pilot")
    takes = {take["target"]: take for take in manifest["takes"]}
    require(set(takes) == {"e1", "e2"}, f"{path}: take targets")
    for target, expected_name, string_number in (("e1", "e1-open-phase-repick.wav", 8), ("e2", "e2-open-phase-repick.wav", 6)):
        take = takes[target]
        require(take["file"] == expected_name and take["string_number"] == string_number and take["pairs"] == 64, f"{path}: {target} take metadata")
        wave = check_file_hash(path, take["file"], take["sha256"], f"{path}:{target}")
        _, _, _, frames = wave_info(wave)
        require(frames == take["frames"] and frames >= 8_140_800, f"{path}: {target} frame count/minimum protocol duration")
        require(take["sha256"] == expected[f"{target}_take_sha256"], f"{path}: {target} receipt hash")
    calibration = manifest["sensors"]["relative_latency_and_phase_calibration"]
    check_file_hash(path, calibration["calibration_file"], calibration["calibration_sha256"], f"{path}: sensor calibration")
    check_file_hash(path, calibration["phase_response_correction_file"], calibration["phase_response_correction_sha256"], f"{path}: phase correction")
    require(calibration["calibration_sha256"] == expected["sensor_calibration_sha256"], f"{path}: calibration hash")
    require(calibration["phase_response_correction_sha256"] == expected["phase_response_correction_sha256"], f"{path}: phase correction hash")
    require(manifest["acquisition_analysis_freeze"]["predecode_acquisition_integrity_receipt_sha256"] == expected["predecode_acquisition_integrity_sha256"], f"{path}: integrity receipt hash")
    check_file_hash(path, manifest["acquisition_analysis_freeze"]["predecode_acquisition_integrity_receipt_file"], manifest["acquisition_analysis_freeze"]["predecode_acquisition_integrity_receipt_sha256"], f"{path}: integrity receipt")
    mapping = manifest["acquisition_analysis_freeze"]["engine_coordinate_mapping"]
    require(mapping["recorded_down_maps_to_engine_stroke"] in ("down", "up") and mapping["recorded_up_maps_to_engine_stroke"] in ("down", "up") and mapping["recorded_down_maps_to_engine_stroke"] != mapping["recorded_up_maps_to_engine_stroke"], f"{path}: engine stroke-direction mapping")
    require(mapping["recorded_positive_normal_maps_to_engine_sign"] in (-1, 1) and not isinstance(mapping["recorded_positive_normal_maps_to_engine_sign"], bool), f"{path}: engine normal-coordinate sign")
    check_file_hash(path, mapping["calibration_file"], mapping["calibration_sha256"], f"{path}: engine coordinate mapping")
    return manifest


def artifact_pairs(owner: Path, container: dict, pairs: tuple[tuple[str, str], ...], prefix: str) -> None:
    for file_key, hash_key in pairs:
        check_file_hash(owner, container[file_key], container[hash_key], f"{prefix}.{file_key}")


def validate_rail_schedule(path: Path, driver_sha256: str) -> dict[str, set[str]]:
    schedule = read_json(path)
    validate_template_shape(schedule, read_json(HERE / "rail-test-schedule.template.json"), str(path))
    scan_filled(schedule)
    require(schedule["schema"] == "electry-repick-phase-rail-test-schedule/v1" and schedule["rail_test_driver_sha256"] == driver_sha256, f"{path}: rail schedule identity/driver")
    cases = schedule["cases"]
    require(all(isinstance(item, dict) and set(item) == set(schedule["required_case_schema"]) for item in cases), f"{path}: rail schedule case schema")
    identities = [(item["rail"], item["case_id"]) for item in cases]
    require(len(identities) == len(set(identities)), f"{path}: duplicate rail schedule case")
    by_rail = {name: set() for name in RAIL_KEYS}
    for rail, case_id in identities:
        require(rail in by_rail and isinstance(case_id, str) and 0 < len(case_id) <= 256, f"{path}: rail schedule case identity")
        by_rail[rail].add(case_id)
    require(all(by_rail.values()), f"{path}: every registered rail needs scheduled cases")
    return by_rail


def validate_build_receipt(
    path: Path,
    role: str,
    source_tree_receipt_sha256: str,
    model_source_commit: str,
    output_executable_sha256: str,
    coefficient_value,
) -> dict:
    receipt = read_json(path)
    validate_template_shape(receipt, read_json(HERE / "build-receipt.template.json"), str(path))
    scan_filled(receipt)
    require(receipt["schema"] == "electry-repick-phase-build-receipt/v1" and receipt["status"] == "built_from_frozen_source" and receipt["role"] == role, f"{path}: build receipt schema/status/role")
    artifact_pairs(path, receipt, (
        ("source_tree_receipt_file", "source_tree_receipt_sha256"),
        ("output_executable_file", "output_executable_sha256"),
        ("build_log_file", "build_log_sha256"),
    ), "build_receipt")
    common = receipt["common_build_contract"]
    artifact_pairs(path, common, (
        ("compiler_executable_file", "compiler_executable_sha256"),
        ("compiler_version_output_file", "compiler_version_output_sha256"),
        ("build_system_file", "build_system_sha256"),
    ), "build_receipt.common_build_contract")
    require(receipt["source_tree_receipt_sha256"] == source_tree_receipt_sha256 and receipt["model_source_commit"] == model_source_commit and receipt["output_executable_sha256"] == output_executable_sha256, f"{path}: build source/commit/output binding")
    require(hex_digest(receipt["model_source_commit"], 40), f"{path}: build source commit")
    require(all(isinstance(common[key], str) and common[key] for key in ("target", "compiler_identity", "generator")), f"{path}: build identity strings")
    for key in ("compile_flags", "link_flags", "non_variant_definitions"):
        require(isinstance(common[key], list) and all(isinstance(item, str) and item for item in common[key]), f"{path}: build {key}")
    require(common["compile_flags"] and len(common["non_variant_definitions"]) == len(set(common["non_variant_definitions"])), f"{path}: build flags/definition set")
    variant = receipt["candidate_contact_variant"]
    require(isinstance(variant["enabled"], bool), f"{path}: candidate-contact build flag type")
    if role == "shipping":
        require(variant["enabled"] is False and variant["compile_definition"] == "ELECTRY_REPICK_PHASE_CANDIDATE=0" and variant["coefficient_value_or_not_applicable"] == "not_applicable_shipping", f"{path}: shipping candidate-contact variant")
    else:
        require(role == "candidate" and variant["enabled"] is True and variant["compile_definition"] == "ELECTRY_REPICK_PHASE_CANDIDATE=1" and finite_number(variant["coefficient_value_or_not_applicable"]) and variant["coefficient_value_or_not_applicable"] == coefficient_value, f"{path}: candidate contact/coefficient variant")
    return {key: value for key, value in common.items() if not key.endswith("_file")}


def validate_build_pair(
    owner: Path,
    evidence: dict,
    acquisition: dict,
    holdout: dict,
    shipping_output_sha256: str,
    candidate_output_sha256: str,
) -> None:
    shipping_common = validate_build_receipt(
        resolve(owner, evidence["shipping_build_receipt_file"]),
        "shipping",
        acquisition["shipping_baseline"]["renderer_source_tree_receipt_sha256"],
        acquisition["shipping_baseline"]["model_source_commit"],
        shipping_output_sha256,
        None,
    )
    candidate_common = validate_build_receipt(
        resolve(owner, evidence["candidate_build_receipt_file"]),
        "candidate",
        holdout["candidate"]["source_tree_receipt_sha256"],
        holdout["candidate"]["model_source_commit"],
        candidate_output_sha256,
        holdout["candidate"]["coefficient"]["value"],
    )
    require(shipping_common == candidate_common, f"{owner}: shipping/candidate common build contracts differ")


def verify_detached_anchor(
    receipt: Path,
    anchor: dict,
    allowed_signers: Path,
    signer_identity: str,
    attestation_key: str,
    stage: str,
) -> None:
    sha_file = resolve(receipt, anchor["sha256_file"])
    signature = resolve(receipt, anchor["custodian_signature_file"])
    require(anchor["signature_method"] == "openssh_sshsig" and anchor["signature_namespace"] == "electry-repick-phase-v1", f"{receipt}: {stage} signature contract")
    require(anchor["signer_identity"] == signer_identity, f"{receipt}: {stage} signer identity differs from pretrusted command-line identity")
    require(allowed_signers.is_file(), f"pretrusted allowed-signers file missing: {allowed_signers}")
    require(anchor["pretrusted_allowed_signers_sha256"] == sha256(allowed_signers), f"{receipt}: {stage} pretrusted allowed-signers SHA-256 mismatch")
    require(anchor[attestation_key] is True, f"{receipt}: missing signed {stage} chronology attestation")
    require(sha_file.is_file() and signature.is_file() and signature.stat().st_size > 0, f"{receipt}: detached {stage} anchor missing")
    require(sha_file.read_text(encoding="utf-8").split()[0] == sha256(receipt), f"{receipt}: detached SHA does not anchor {stage} receipt")
    ssh_keygen = shutil.which("ssh-keygen")
    require(ssh_keygen is not None, f"{receipt}: ssh-keygen is required to verify custodian signature")
    verification = subprocess.run(
        [ssh_keygen, "-Y", "verify", "-f", str(allowed_signers), "-I", signer_identity, "-n", anchor["signature_namespace"], "-s", str(signature)],
        input=sha_file.read_bytes(),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    require(verification.returncode == 0, f"{receipt}: invalid {stage} custodian signature: {verification.stderr.decode('utf-8', 'replace').strip()}")


def validate_acquisition(
    path: Path, allowed_signers: Path, signer_identity: str
) -> tuple[dict, dict[str, tuple[Path, dict, dict]]]:
    acquisition = read_json(path)
    validate_template_shape(acquisition, read_json(HERE / "acquisition-freeze.template.json"), str(path))
    scan_filled(acquisition)
    require(acquisition["schema"] == "electry-repick-phase-acquisition-freeze/v1", f"{path}: schema")
    require(acquisition["status"] == "frozen_before_train_decode", f"{path}: status")
    clusters = acquisition["clusters"]
    require(len(clusters) == 5 and Counter(item["split"] for item in clusters) == Counter({"train": 3, "holdout": 2}), f"{path}: split counts")
    require(len({item["cluster_id"] for item in clusters}) == 5, f"{path}: duplicate cluster_id")
    versioned = acquisition["versioned_contract"]
    require(versioned["model_render_contract_canonicalization"] == CANONICALIZATION, f"{path}: canonicalization")
    require(versioned["model_render_contract_source_json_pointer"] == "/acquisition_analysis_freeze/model_render_contract", f"{path}: model contract pointer")
    require(tuple(versioned["analysis_contract_source_json_pointers"]) == ANALYSIS_POINTERS, f"{path}: analysis contract pointers")
    require(versioned["analysis_contract_value_construction"] == "object_mapping_each_listed_pointer_string_to_its_pointed_JSON_value", f"{path}: analysis contract construction")
    require(versioned["analysis_contract_canonicalization"] == CANONICALIZATION, f"{path}: analysis contract canonicalization")
    contract_files = {
        "capture_readme_sha256": "README.md",
        "manifest_template_sha256": "manifest.template.json",
        "validator_sha256": "validate.py",
        "acquisition_freeze_template_sha256": "acquisition-freeze.template.json",
        "holdout_freeze_template_sha256": "holdout-freeze.template.json",
        "result_receipt_template_sha256": "result-receipt.template.json",
        "event_detail_template_sha256": "event-detail.template.json",
        "damping_receipt_template_sha256": "damping-receipt.template.json",
        "decode_ledger_template_sha256": "decode-ledger.template.json",
        "coefficient_receipt_template_sha256": "coefficient-receipt.template.json",
        "build_receipt_template_sha256": "build-receipt.template.json",
        "cpu_timing_template_sha256": "cpu-timing.template.json",
        "rail_test_schedule_template_sha256": "rail-test-schedule.template.json",
        "rail_test_result_template_sha256": "rail-test-result.template.json",
        "rails_receipt_template_sha256": "rails-receipt.template.json",
        "final_receipt_template_sha256": "final-receipt.template.json",
    }
    protocol_commit = versioned["protocol_git_commit"]
    require(hex_digest(protocol_commit, 40), f"{path}: protocol Git commit")
    for hash_key, filename in contract_files.items():
        protocol_file = HERE / filename
        require(hex_digest(versioned[hash_key], 64) and versioned[hash_key] == sha256(protocol_file), f"{path}: frozen protocol file hash {filename}")
        require(versioned[hash_key] == committed_file_sha256(protocol_commit, protocol_file), f"{path}: {filename} differs from recorded protocol commit")
    artifact_pairs(path, versioned, (
        ("analysis_preregistration_file", "analysis_preregistration_sha256"),
        ("analyzer_file", "analyzer_sha256"),
        ("snr_control_band_definition_file", "snr_control_band_definition_sha256"),
        ("synthetic_fixture_inputs_file", "synthetic_fixture_inputs_sha256"),
        ("synthetic_fixture_expected_results_file", "synthetic_fixture_expected_results_sha256"),
        ("cpu_benchmark_driver_file", "cpu_benchmark_driver_sha256"),
        ("rail_test_driver_file", "rail_test_driver_sha256"),
        ("rail_test_schedule_file", "rail_test_schedule_sha256"),
    ), "versioned_contract")
    validate_rail_schedule(resolve(path, versioned["rail_test_schedule_file"]), versioned["rail_test_driver_sha256"])
    check_file_hash(path, acquisition["holdout_blinding"]["blinded_qc_program_file"], acquisition["holdout_blinding"]["blinded_qc_program_sha256"], "holdout_blinding.blinded_qc_program_file")
    artifact_pairs(path, acquisition["shipping_baseline"], (
        ("renderer_executable_file", "renderer_executable_sha256"),
        ("renderer_source_tree_receipt_file", "renderer_source_tree_receipt_sha256"),
        ("model_preset_file", "model_preset_sha256"),
    ), "shipping_baseline")
    require(hex_digest(acquisition["shipping_baseline"]["model_source_commit"], 40), f"{path}: shipping model source commit")
    manifests = {}
    setup = None
    contract_hash = versioned["model_render_contract_canonical_json_sha256"]
    registered_analysis_hash = versioned["analysis_contract_canonical_json_sha256"]
    for entry in clusters:
        manifest_path = check_file_hash(path, entry["manifest_file"], entry["manifest_sha256"], f"cluster {entry['cluster_id']} manifest")
        manifest = validate_manifest(manifest_path, entry, acquisition["study_id"])
        frozen = manifest["acquisition_analysis_freeze"]
        require(frozen["protocol_git_commit"] == versioned["protocol_git_commit"] and frozen["analysis_preregistration_sha256"] == versioned["analysis_preregistration_sha256"] and frozen["analyzer_sha256"] == versioned["analyzer_sha256"], f"{manifest_path}: protocol/analyzer binding")
        require(frozen["synthetic_joint_fit_fixtures"]["inputs_sha256"] == versioned["synthetic_fixture_inputs_sha256"] and frozen["synthetic_joint_fit_fixtures"]["expected_results_sha256"] == versioned["synthetic_fixture_expected_results_sha256"], f"{manifest_path}: synthetic fixture binding")
        require(frozen["eligibility"]["snr_control_band_definition_sha256"] == versioned["snr_control_band_definition_sha256"], f"{manifest_path}: SNR definition binding")
        require(frozen["holdout_blinding"]["blinded_qc_program_sha256"] == acquisition["holdout_blinding"]["blinded_qc_program_sha256"], f"{manifest_path}: blinded QC binding")
        baseline = frozen["shipping_baseline"]
        acquisition_baseline = acquisition["shipping_baseline"]
        require(
            baseline == {
                "renderer_executable_sha256": acquisition_baseline["renderer_executable_sha256"],
                "renderer_source_tree_receipt_sha256": acquisition_baseline["renderer_source_tree_receipt_sha256"],
                "model_source_commit": acquisition_baseline["model_source_commit"],
                "model_preset_sha256": acquisition_baseline["model_preset_sha256"],
                "model_seed": acquisition_baseline["model_seed"],
            },
            f"{manifest_path}: shipping baseline binding",
        )
        sensor_binding = frozen["sensor_calibration_binding"]
        sensor_calibration = manifest["sensors"]["relative_latency_and_phase_calibration"]
        require(sensor_binding["calibration_sha256"] == sensor_calibration["calibration_sha256"] and sensor_binding["phase_response_correction_sha256"] == sensor_calibration["phase_response_correction_sha256"], f"{manifest_path}: sensor calibration binding")
        require(canonical_hash(frozen["model_render_contract"]) == contract_hash, f"{manifest_path}: model render contract hash")
        require(canonical_hash(analysis_contract(manifest)) == registered_analysis_hash, f"{manifest_path}: registered analysis contract changed")
        current_setup = stable_setup(manifest)
        if setup is None:
            setup = current_setup
        else:
            require(current_setup == setup, f"{manifest_path}: fixed rig/player setup changed")
        manifests[entry["cluster_id"]] = (manifest_path, manifest, entry)
    completed_manifests = [manifest for _, manifest, _ in manifests.values()]
    require(len({manifest["session"]["session_id"] for manifest in completed_manifests}) == 5, f"{path}: performance clusters must have distinct session IDs")
    require(len({take["sha256"] for manifest in completed_manifests for take in manifest["takes"]}) == 10, f"{path}: raw takes reused across performance clusters/strings")
    verify_detached_anchor(
        path,
        acquisition["pretrain_detached_anchor"],
        allowed_signers,
        signer_identity,
        "custodian_attests_signature_completed_before_train_decode",
        "pre-TRAIN",
    )
    return acquisition, manifests


def result_cells(result: dict) -> set[tuple[str, str, float]]:
    return {(item["string"], item["second_stroke_direction"], float(item["quadrant_centre_degrees"])) for item in result["real_cells"]}


def close(left: float, right: float) -> bool:
    return math.isclose(float(left), float(right), rel_tol=1e-9, abs_tol=1e-9)


def finite_number(value) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def phase_calibration_half_width(manifest: dict, f0_hz: float, contact_onset_half_width_ms: float) -> float:
    calibration = manifest["sensors"]["relative_latency_and_phase_calibration"]
    latency = calibration["latency_half_width_samples_by_channel"]
    phase = calibration["phase_response_half_width_degrees_by_channel_h1_h4"]
    return (
        360.0 * float(f0_hz)
        * (float(contact_onset_half_width_ms) / 1000.0 + (float(latency[1]) + float(latency[2])) / 96000.0)
        + float(phase[1][0]) + float(phase[2][0])
    )


def same_scalar(left, right) -> bool:
    return close(left, right) if finite_number(left) and finite_number(right) else left == right


def quadrant_for_interval(phase: float, half_width: float) -> float | None:
    if not (0.0 <= phase < 360.0 and 0.0 <= half_width < 45.0):
        return None
    matches = [
        centre
        for centre in (0.0, 90.0, 180.0, 270.0)
        if abs((phase - centre + 180.0) % 360.0 - 180.0) + half_width < 45.0
    ]
    return matches[0] if len(matches) == 1 else None


def validate_result(
    path: Path,
    acquisition_path: Path,
    allowed_signers: Path,
    signer_identity: str,
    holdout_path: Path | None = None,
) -> dict:
    result = read_json(path)
    validate_template_shape(result, read_json(HERE / "result-receipt.template.json"), str(path))
    scan_filled(result)
    require(result["schema"] == "electry-repick-phase-analysis-result/v1", f"{path}: schema")
    require(result["status"] == "sealed_before_listening", f"{path}: status")
    require(result["decode"]["attempt"] == 1 and result["decode"]["previous_attempts"] == 0, f"{path}: exact-once decode")
    require(result_cells(result) == CELLS and len(result["real_cells"]) == 16, f"{path}: real cells")
    profiles = {(item["string"], item["second_stroke_direction"]) for item in result["real_profiles"]}
    require(profiles == PROFILES and len(result["real_profiles"]) == 4, f"{path}: profiles")
    manifest_path = check_file_hash(path, result["manifest"]["file"], result["manifest"]["sha256"], f"{path}: manifest")
    manifest = read_json(manifest_path)
    require(manifest["session"]["study_id"] == result["study_id"] and manifest["session"]["cluster_id"] == result["cluster_id"] and manifest["session"]["split"] == result["split"], f"{path}: manifest identity")
    anchored_acquisition = check_file_hash(path, result["acquisition_freeze"]["file"], result["acquisition_freeze"]["sha256"], f"{path}: acquisition freeze")
    require(anchored_acquisition == acquisition_path.resolve(), f"{path}: wrong acquisition freeze")
    acquisition = read_json(anchored_acquisition)
    registered_clusters = [entry for entry in acquisition["clusters"] if entry["cluster_id"] == result["cluster_id"] and entry["split"] == result["split"]]
    require(len(registered_clusters) == 1 and registered_clusters[0]["manifest_sha256"] == result["manifest"]["sha256"] and manifest_path == resolve(acquisition_path, registered_clusters[0]["manifest_file"]), f"{path}: result cluster/manifest not registered by acquisition freeze")
    require(result["analyzer"]["sha256"] == acquisition["versioned_contract"]["analyzer_sha256"], f"{path}: analyzer differs from acquisition freeze")
    require(result["analyzer"]["analysis_preregistration_sha256"] == acquisition["versioned_contract"]["analysis_preregistration_sha256"], f"{path}: preregistration differs from acquisition freeze")
    require(result["analyzer"]["model_render_contract_canonical_json_sha256"] == acquisition["versioned_contract"]["model_render_contract_canonical_json_sha256"], f"{path}: model contract differs from acquisition freeze")
    require(result["analyzer"]["analysis_contract_canonical_json_sha256"] == acquisition["versioned_contract"]["analysis_contract_canonical_json_sha256"], f"{path}: analysis contract differs from acquisition freeze")
    detached = resolve(path, result["acquisition_freeze"]["detached_sha256_file"])
    signature = resolve(path, result["acquisition_freeze"]["custodian_signature_file"])
    require(detached.is_file() and signature.is_file() and detached.read_text(encoding="utf-8").split()[0] == result["acquisition_freeze"]["sha256"], f"{path}: detached acquisition anchor")
    artifact_pairs(path, result["analyzer"], (("file", "sha256"), ("analysis_preregistration_file", "analysis_preregistration_sha256")), "analyzer")
    detail_path = check_file_hash(path, result["event_detail"]["file"], result["event_detail"]["sha256"], f"{path}: event detail")
    detail = read_json(detail_path)
    validate_template_shape(detail, read_json(HERE / "event-detail.template.json"), str(detail_path))
    scan_filled(detail)
    require(detail["schema"] == "electry-repick-phase-event-detail/v1", f"{path}: event-detail schema")
    require((detail["study_id"], detail["cluster_id"], detail["split"]) == (result["study_id"], result["cluster_id"], result["split"]), f"{path}: event-detail identity")
    require(detail["manifest_sha256"] == result["manifest"]["sha256"] and detail["authority_freeze_sha256"] == result["authority_freeze"]["sha256"] and detail["analyzer_sha256"] == result["analyzer"]["sha256"], f"{path}: event-detail authority")
    require(detail["analysis_contract_canonical_json_sha256"] == result["analyzer"]["analysis_contract_canonical_json_sha256"], f"{path}: event-detail analysis contract")
    ledger_path = check_file_hash(path, result["decode"]["ledger_file"], result["decode"]["ledger_sha256"], f"{path}: decode ledger")
    ledger = read_json(ledger_path)
    validate_template_shape(ledger, read_json(HERE / "decode-ledger.template.json"), str(ledger_path))
    scan_filled(ledger)
    require(ledger["schema"] == "electry-repick-phase-decode-ledger/v1" and ledger["status"] == "sealed_after_only_decode_before_listening", f"{path}: decode ledger schema/status")
    require((ledger["study_id"], ledger["cluster_id"], ledger["split"]) == (result["study_id"], result["cluster_id"], result["split"]), f"{path}: decode ledger identity")
    require(ledger["authority_freeze_sha256"] == result["authority_freeze"]["sha256"] and ledger["manifest_sha256"] == result["manifest"]["sha256"] and ledger["analyzer_sha256"] == result["analyzer"]["sha256"] and ledger["analysis_contract_canonical_json_sha256"] == result["analyzer"]["analysis_contract_canonical_json_sha256"], f"{path}: decode ledger authority")
    require(ledger["input_takes"] == result["input_takes"], f"{path}: decode ledger input takes")
    require(ledger["attempt_count"] == 1 and ledger["attempt"]["ordinal"] == 1 and ledger["attempt"]["automatic_before_listening"] is True and ledger["attempt"]["completed_successfully"] is True, f"{path}: decode ledger attempt")
    require(check_file_hash(ledger_path, ledger["attempt"]["event_detail_file"], ledger["attempt"]["event_detail_sha256"], f"{path}: ledger event detail") == detail_path and ledger["attempt"]["event_detail_sha256"] == result["event_detail"]["sha256"], f"{path}: decode ledger event-detail linkage")
    verify_detached_anchor(
        ledger_path,
        ledger["custodian_attestation"],
        allowed_signers,
        signer_identity,
        "custodian_attests_ledger_sealed_before_any_listening",
        "decode-ledger",
    )
    take_map = {item["target"]: item for item in manifest["takes"]}
    require(result["input_takes"]["e1_sha256"] == take_map["e1"]["sha256"] and result["input_takes"]["e2_sha256"] == take_map["e2"]["sha256"], f"{path}: input take hashes")
    trial_map = {
        (string, item["trial"]): item
        for string in ("e1", "e2")
        for item in manifest["performance"]["trial_order"][string]
    }
    real_events = detail["real_events"]
    require(tuple(detail["registered_exclusion_reasons"]) == EXCLUSION_REASONS, f"{path}: registered exclusion reasons")
    require(len(real_events) == 128 and all(isinstance(item, dict) and set(item) == set(detail["required_real_event_schema"]) for item in real_events), f"{path}: real event schema/count")
    require(all(item["string"] in ("e1", "e2") and isinstance(item["trial"], int) and not isinstance(item["trial"], bool) and 1 <= item["trial"] <= 64 for item in real_events), f"{path}: real event source")
    require({(item["string"], item["trial"]) for item in real_events} == set(trial_map), f"{path}: retain all 128 real attempts")
    real_by_source = {(item["string"], item["trial"]): item for item in real_events}
    real_groups = {key: [] for key in CELLS}
    eligible_sources = set()
    nominal_midi = {"e1": 28, "e2": 40}
    for event in real_events:
        source = (event["string"], event["trial"])
        scheduled = trial_map[source]
        require(event["second_stroke_direction"] == scheduled["second_stroke"], f"{path}: event stroke mismatch {source}")
        require(isinstance(event["eligible"], bool) and isinstance(event["exclusion_reasons"], list), f"{path}: event eligibility/reasons {source}")
        numeric_keys = (
            "measured_ioi_ms", "contact_onset_half_width_ms", "measured_phase_degrees",
            "calibration_phase_half_width_degrees", "fit_phase_half_width_degrees",
            "f0_phase_half_width_degrees", "phase_interval_half_width_degrees",
            "f0_hz", "pre_normal_motion_h1_snr_db",
            "pre_bridge_di_h1_h4_snr_db",
        )
        require(all(finite_number(event[key]) for key in numeric_keys), f"{path}: event numeric fields {source}")
        require(event["measured_ioi_ms"] >= 0.0 and event["contact_onset_half_width_ms"] >= 0.0 and event["f0_hz"] > 0.0 and 0.0 <= event["measured_phase_degrees"] < 360.0 and all(event[key] >= 0.0 for key in ("calibration_phase_half_width_degrees", "fit_phase_half_width_degrees", "f0_phase_half_width_degrees", "phase_interval_half_width_degrees")), f"{path}: event physical ranges {source}")
        require(event["contact_onset_half_width_ms"] >= manifest["sensors"]["contact_reference"]["measured_onset_half_width_ms"], f"{path}: event contact uncertainty understates calibration {source}")
        require(isinstance(event["extra_transient_detected"], bool) and isinstance(event["clipping_or_nonfinite_detected"], bool), f"{path}: event quality flags {source}")
        interval = event["f0_interval_hz"]
        require(len(interval) == 2 and all(finite_number(value) and value > 0.0 for value in interval) and interval[0] <= event["f0_hz"] <= interval[1], f"{path}: f0 interval {source}")
        calibration_half_width = phase_calibration_half_width(manifest, event["f0_hz"], event["contact_onset_half_width_ms"])
        f0_half_width = 360.0 * 0.031 * max(abs(interval[0] - event["f0_hz"]), abs(interval[1] - event["f0_hz"]))
        total_phase_half_width = calibration_half_width + event["fit_phase_half_width_degrees"] + f0_half_width
        require(close(event["calibration_phase_half_width_degrees"], calibration_half_width) and close(event["f0_phase_half_width_degrees"], f0_half_width), f"{path}: event uncertainty components {source}")
        require(close(event["phase_interval_half_width_degrees"], total_phase_half_width), f"{path}: event total phase uncertainty {source}")
        nominal = 440.0 * 2.0 ** ((nominal_midi[event["string"]] - 69) / 12.0)
        f0_inside = interval[0] > nominal * 2.0 ** (-50.0 / 1200.0) and interval[1] < nominal * 2.0 ** (50.0 / 1200.0)
        assigned_quadrant = quadrant_for_interval(float(event["measured_phase_degrees"]), total_phase_half_width)
        failures = []
        if not 75.0 <= event["measured_ioi_ms"] <= 130.0:
            failures.append(EXCLUSION_REASONS[0])
        if event["contact_onset_half_width_ms"] > 1.0:
            failures.append(EXCLUSION_REASONS[1])
        if not f0_inside:
            failures.append(EXCLUSION_REASONS[2])
        if assigned_quadrant is None:
            failures.append(EXCLUSION_REASONS[3])
        if event["pre_normal_motion_h1_snr_db"] < 12.0:
            failures.append(EXCLUSION_REASONS[4])
        if event["pre_bridge_di_h1_h4_snr_db"] < 12.0:
            failures.append(EXCLUSION_REASONS[5])
        if event["extra_transient_detected"]:
            failures.append(EXCLUSION_REASONS[6])
        if event["clipping_or_nonfinite_detected"]:
            failures.append(EXCLUSION_REASONS[7])
        require(event["eligible"] == (not failures) and event["exclusion_reasons"] == failures, f"{path}: event eligibility derivation {source}")
        if event["eligible"] is True:
            phase = float(event["quadrant_centre_degrees"])
            key = (event["string"], event["second_stroke_direction"], phase)
            require(key in CELLS and isinstance(event["G_db"], (int, float)) and not isinstance(event["G_db"], bool) and math.isfinite(event["G_db"]), f"{path}: eligible event cell/G {source}")
            require(phase == assigned_quadrant, f"{path}: measured phase interval does not match quadrant {source}")
            real_groups[key].append(float(event["G_db"]))
            eligible_sources.add(source)
        else:
            require(event["eligible"] is False and event["quadrant_centre_degrees"] == "ineligible" and event["G_db"] == "ineligible" and event["exclusion_reasons"], f"{path}: rejected event sentinel/reason {source}")
    result_real = {(item["string"], item["second_stroke_direction"], float(item["quadrant_centre_degrees"])): item for item in result["real_cells"]}
    computed_real_values = {}
    for key, values in real_groups.items():
        require(isinstance(result_real[key]["eligible_events"], int) and not isinstance(result_real[key]["eligible_events"], bool) and result_real[key]["eligible_events"] == len(values), f"{path}: real cell count {key}")
        if values:
            computed_real_values[key] = median(values)
            require(finite_number(result_real[key]["real_G_median_db"]) and close(result_real[key]["real_G_median_db"], computed_real_values[key]), f"{path}: real cell median {key}")
        else:
            computed_real_values[key] = INSUFFICIENT
            require(result_real[key]["real_G_median_db"] == INSUFFICIENT, f"{path}: empty real cell sentinel {key}")
    computed_profiles = {}
    for profile in result["real_profiles"]:
        key = (profile["string"], profile["second_stroke_direction"])
        require(len(profile["uncentred_G_db"]) == 4 and len(profile["centred_G_db"]) == 4 and isinstance(profile["resolved"], bool), f"{path}: profile shape {key}")
        cell_keys = [(key[0], key[1], phase) for phase in (0.0, 90.0, 180.0, 270.0)]
        values = [computed_real_values[cell_key] for cell_key in cell_keys]
        complete = all(len(real_groups[cell_key]) >= 3 for cell_key in cell_keys)
        require(all(same_scalar(left, right) for left, right in zip(profile["uncentred_G_db"], values)), f"{path}: uncentred profile {key}")
        if not complete:
            require(profile["centred_G_db"] == [INCONCLUSIVE] * 4 and profile["span_db"] == INCONCLUSIVE and profile["pooled_residual_mad_db"] == INCONCLUSIVE and profile["resolved"] is False, f"{path}: incomplete profile sentinel {key}")
            computed_profiles[key] = {"centred_G_db": None, "resolved": False}
            continue
        require(all(finite_number(value) for value in values) and all(finite_number(value) for value in profile["centred_G_db"]) and finite_number(profile["span_db"]) and finite_number(profile["pooled_residual_mad_db"]) and profile["pooled_residual_mad_db"] >= 0.0, f"{path}: complete profile numerics {key}")
        values = [float(value) for value in values]
        centred = [value - sum(values) / 4.0 for value in values]
        span = max(values) - min(values)
        residuals = [
            abs(event_value - cell_median)
            for phase, cell_median in zip((0.0, 90.0, 180.0, 270.0), values)
            for event_value in real_groups[(key[0], key[1], phase)]
        ]
        pooled_mad = median(residuals)
        require(all(close(left, right) for left, right in zip(profile["centred_G_db"], centred)), f"{path}: centred profile {key}")
        require(close(profile["pooled_residual_mad_db"], pooled_mad), f"{path}: pooled residual MAD {key}")
        computed_resolved = span > pooled_mad
        require(close(profile["span_db"], span) and profile["resolved"] == computed_resolved, f"{path}: profile gate {key}")
        computed_profiles[key] = {"centred_G_db": centred, "resolved": computed_resolved}
    require(set(computed_profiles) == PROFILES, f"{path}: computed profile coverage")
    result["_computed_real_profiles"] = computed_profiles
    profiles_resolved = all(item["resolved"] is True for item in computed_profiles.values())
    require(isinstance(result["real_effect"]["all_four_profiles_resolved"], bool) and result["real_effect"]["all_four_profiles_resolved"] == profiles_resolved and result["real_effect"]["status"] == ("pass" if profiles_resolved else "inconclusive"), f"{path}: real-effect aggregate")
    if result["split"] == "train":
        require(result["authority_freeze"]["kind"] == "acquisition", f"{path}: TRAIN authority")
        require(result["authority_freeze"]["sha256"] == result["acquisition_freeze"]["sha256"], f"{path}: TRAIN authority hash")
        require(check_file_hash(path, result["authority_freeze"]["file"], result["authority_freeze"]["sha256"], f"{path}: TRAIN authority") == acquisition_path.resolve(), f"{path}: TRAIN authority path")
        require(result["model_comparison"]["status"] == "not_applicable_train" and result["model_comparison"]["cells"] == [] and result["model_comparison"]["summary"] == {}, f"{path}: TRAIN model comparison")
        require(detail["model_events"] == [], f"{path}: TRAIN must not contain model events")
    else:
        require(result["split"] == "holdout" and holdout_path is not None, f"{path}: HOLDOUT authority missing")
        require(result["authority_freeze"]["kind"] == "holdout", f"{path}: HOLDOUT authority")
        require(check_file_hash(path, result["authority_freeze"]["file"], result["authority_freeze"]["sha256"], f"{path}: holdout freeze") == holdout_path.resolve(), f"{path}: wrong HOLDOUT freeze")
        cells = result["model_comparison"]["cells"]
        require(all(isinstance(item, dict) and set(item) == set(result["model_comparison"]["required_holdout_cell_schema"]) for item in cells), f"{path}: HOLDOUT model cell schema")
        keys = {(item["string"], item["second_stroke_direction"], float(item["quadrant_centre_degrees"])) for item in cells}
        require(len(cells) == 16 and keys == CELLS, f"{path}: HOLDOUT model cells")
        model_events = detail["model_events"]
        expected_model_sources = {(variant, string, trial) for string, trial in eligible_sources for variant in ("shipping", "candidate")}
        require(len(model_events) == len(expected_model_sources) and all(isinstance(item, dict) and set(item) == set(detail["required_model_event_schema"]) for item in model_events), f"{path}: HOLDOUT model event schema/count")
        model_groups = {(variant, *key): [] for variant in ("shipping", "candidate") for key in CELLS}
        model_by_source = {}
        seen_model_sources = set()
        for event in model_events:
            source = (event["source_real_string"], event["source_real_trial"])
            require(source in eligible_sources and event["variant"] in ("shipping", "candidate"), f"{path}: model source/variant {source}")
            scheduled = trial_map[source]
            note_and_string = {"e1": (28, 8), "e2": (40, 6)}[source[0]]
            require((event["midi_note"], event["physical_string_number"]) == note_and_string and event["first_stroke_direction"] == scheduled["first_stroke"] and event["second_stroke_direction"] == scheduled["second_stroke"], f"{path}: model note/string/stroke mismatch {source}")
            require(finite_number(event["first_velocity"]) and finite_number(event["second_velocity"]) and event["first_velocity"] == 0.90 and event["second_velocity"] == 0.90, f"{path}: model velocities {source}")
            diagnostic_keys = ("diagnostic_first_contact_sample", "diagnostic_second_contact_sample", "diagnostic_note_off_sample", "render_end_sample")
            require(all(isinstance(event[key], int) and not isinstance(event[key], bool) for key in diagnostic_keys), f"{path}: model diagnostic sample types {source}")
            second_sample = 96000 + math.floor(real_by_source[source]["measured_ioi_ms"] * 96.0 + 0.5)
            require(event["diagnostic_first_contact_sample"] == 96000 and event["diagnostic_second_contact_sample"] == second_sample and event["diagnostic_note_off_sample"] == second_sample + 24000 and event["render_end_sample"] >= second_sample + 48000, f"{path}: model diagnostic schedule {source}")
            require(hex_digest(event["precontact_identity_sha256"], 64), f"{path}: model precontact identity hash {source}")
            require(isinstance(event["eligible"], bool) and isinstance(event["exclusion_reasons"], list) and all(finite_number(event[key]) for key in ("f0_hz", "measured_phase_degrees", "fit_phase_half_width_degrees", "f0_phase_half_width_degrees", "phase_interval_half_width_degrees")), f"{path}: model event eligibility/numerics {source}")
            model_f0_interval = event["f0_interval_hz"]
            require(len(model_f0_interval) == 2 and all(finite_number(value) and value > 0.0 for value in model_f0_interval) and model_f0_interval[0] <= event["f0_hz"] <= model_f0_interval[1], f"{path}: model f0 interval {source}")
            require(event["f0_hz"] > 0.0 and 0.0 <= event["measured_phase_degrees"] < 360.0 and all(event[key] >= 0.0 for key in ("fit_phase_half_width_degrees", "f0_phase_half_width_degrees", "phase_interval_half_width_degrees")), f"{path}: model phase/f0 ranges {source}")
            model_f0_half_width = 360.0 * 0.031 * max(abs(model_f0_interval[0] - event["f0_hz"]), abs(model_f0_interval[1] - event["f0_hz"]))
            model_total_phase_half_width = event["fit_phase_half_width_degrees"] + model_f0_half_width
            require(close(event["f0_phase_half_width_degrees"], model_f0_half_width) and close(event["phase_interval_half_width_degrees"], model_total_phase_half_width), f"{path}: model phase uncertainty calculation {source}")
            nominal = 440.0 * 2.0 ** ((nominal_midi[source[0]] - 69) / 12.0)
            model_f0_inside = model_f0_interval[0] > nominal * 2.0 ** (-50.0 / 1200.0) and model_f0_interval[1] < nominal * 2.0 ** (50.0 / 1200.0)
            model_quadrant = quadrant_for_interval(float(event["measured_phase_degrees"]), model_total_phase_half_width)
            model_failures = []
            if not model_f0_inside:
                model_failures.append(EXCLUSION_REASONS[2])
            if model_quadrant is None:
                model_failures.append(EXCLUSION_REASONS[3])
            require(event["eligible"] == (not model_failures) and event["exclusion_reasons"] == model_failures, f"{path}: model event eligibility derivation {source}")
            if event["eligible"]:
                require(event["exclusion_reasons"] == [] and finite_number(event["G_db"]) and float(event["quadrant_centre_degrees"]) == model_quadrant, f"{path}: eligible model phase/G {source}")
                key = (event["variant"], event["source_real_string"], event["second_stroke_direction"], float(event["quadrant_centre_degrees"]))
                require(key in model_groups, f"{path}: model event cell {source}")
                model_groups[key].append(float(event["G_db"]))
            else:
                require(event["quadrant_centre_degrees"] == "ineligible" and event["G_db"] == "ineligible" and event["exclusion_reasons"], f"{path}: ineligible model sentinel/reason {source}")
            model_source = (event["variant"], *source)
            seen_model_sources.add(model_source)
            model_by_source[model_source] = event
        require(seen_model_sources == expected_model_sources, f"{path}: HOLDOUT model schedule")
        paired_precontact_fields = (
            "midi_note", "physical_string_number", "first_stroke_direction",
            "second_stroke_direction", "first_velocity", "second_velocity",
            "diagnostic_first_contact_sample", "diagnostic_second_contact_sample",
            "diagnostic_note_off_sample", "render_end_sample",
            "precontact_identity_sha256", "f0_hz", "f0_interval_hz",
            "measured_phase_degrees", "fit_phase_half_width_degrees",
            "f0_phase_half_width_degrees", "phase_interval_half_width_degrees",
            "eligible", "quadrant_centre_degrees", "exclusion_reasons",
        )
        for source in eligible_sources:
            shipping_event = model_by_source[("shipping", *source)]
            candidate_event = model_by_source[("candidate", *source)]
            require(
                all(shipping_event[field] == candidate_event[field] for field in paired_precontact_fields),
                f"{path}: shipping/candidate precontact state or bin differs {source}",
            )
        comparison_complete = all(len(real_groups[key]) >= 3 and len(model_groups[("shipping", *key)]) >= 3 and len(model_groups[("candidate", *key)]) >= 3 for key in CELLS)
        shipping_errors = []
        candidate_errors = []
        for item in cells:
            key = (item["string"], item["second_stroke_direction"], float(item["quadrant_centre_degrees"]))
            shipping_values = model_groups[("shipping", *key)]
            candidate_values = model_groups[("candidate", *key)]
            require(isinstance(item["shipping_model_events"], int) and not isinstance(item["shipping_model_events"], bool) and isinstance(item["candidate_model_events"], int) and not isinstance(item["candidate_model_events"], bool), f"{path}: model event count types {key}")
            require(item["shipping_model_events"] == len(shipping_values) and item["candidate_model_events"] == len(candidate_values), f"{path}: model event count {key}")
            cell_complete = len(real_groups[key]) >= 3 and len(shipping_values) >= 3 and len(candidate_values) >= 3
            metric_fields = ("shipping_model_G_median_db", "candidate_model_G_median_db", "shipping_absolute_error_db", "candidate_absolute_error_db")
            if not cell_complete:
                require(all(item[field] == INCONCLUSIVE for field in metric_fields), f"{path}: incomplete model cell sentinel {key}")
                continue
            require(all(finite_number(item[field]) for field in metric_fields), f"{path}: model cell numerics {key}")
            shipping_median_value = median(shipping_values)
            candidate_median_value = median(candidate_values)
            require(close(item["shipping_model_G_median_db"], shipping_median_value) and close(item["candidate_model_G_median_db"], candidate_median_value), f"{path}: model cell median {key}")
            real_value = float(computed_real_values[key])
            shipping = abs(shipping_median_value - real_value)
            candidate = abs(candidate_median_value - real_value)
            require(close(shipping, item["shipping_absolute_error_db"]) and close(candidate, item["candidate_absolute_error_db"]), f"{path}: model cell error")
            shipping_errors.append(shipping)
            candidate_errors.append(candidate)
        if not comparison_complete:
            require(result["model_comparison"]["status"] == INCONCLUSIVE and result["model_comparison"]["summary"] == {}, f"{path}: incomplete HOLDOUT comparison status")
            return result
        require(result["model_comparison"]["status"] == "scored_holdout" and result["model_comparison"]["summary"], f"{path}: HOLDOUT comparison status")
        require(set(result["model_comparison"]["summary"]) == {
            "shipping_median_absolute_error_db", "candidate_median_absolute_error_db",
            "median_error_reduction_fraction_or_rule", "shipping_maximum_absolute_error_db",
            "candidate_maximum_absolute_error_db", "median_reduction_at_least_0_50",
            "maximum_error_not_worse",
        }, f"{path}: HOLDOUT summary fields")
        summary = result["model_comparison"]["summary"]
        require(all(finite_number(summary[field]) for field in ("shipping_median_absolute_error_db", "candidate_median_absolute_error_db", "shipping_maximum_absolute_error_db", "candidate_maximum_absolute_error_db")) and isinstance(summary["median_reduction_at_least_0_50"], bool) and isinstance(summary["maximum_error_not_worse"], bool), f"{path}: HOLDOUT summary types")
        shipping_median = median(shipping_errors)
        candidate_median = median(candidate_errors)
        require(close(summary["shipping_median_absolute_error_db"], shipping_median) and close(summary["candidate_median_absolute_error_db"], candidate_median), f"{path}: median error")
        require(close(summary["shipping_maximum_absolute_error_db"], max(shipping_errors)) and close(summary["candidate_maximum_absolute_error_db"], max(candidate_errors)), f"{path}: maximum error")
        if shipping_median == 0.0:
            require(summary["median_error_reduction_fraction_or_rule"] == "promotion_impossible_zero_shipping_error" and summary["median_reduction_at_least_0_50"] is False, f"{path}: zero shipping error rule")
        else:
            reduction = 1.0 - candidate_median / shipping_median
            require(finite_number(summary["median_error_reduction_fraction_or_rule"]) and close(summary["median_error_reduction_fraction_or_rule"], reduction) and summary["median_reduction_at_least_0_50"] == (reduction >= 0.5), f"{path}: median reduction")
        require(summary["maximum_error_not_worse"] == (max(candidate_errors) <= max(shipping_errors)), f"{path}: maximum error gate")
    return result


def validate_holdout(
    path: Path,
    acquisition_path: Path,
    acquisition: dict,
    manifests: dict,
    allowed_signers: Path,
    signer_identity: str,
) -> dict:
    holdout = read_json(path)
    validate_template_shape(holdout, read_json(HERE / "holdout-freeze.template.json"), str(path))
    scan_filled(holdout)
    require(holdout["schema"] == "electry-repick-phase-holdout-freeze/v1", f"{path}: schema")
    require(holdout["status"] == "frozen_before_holdout_decode", f"{path}: status")
    require(holdout["study_id"] == acquisition["study_id"], f"{path}: study_id")
    verify_detached_anchor(
        path,
        holdout["preholdout_detached_anchor"],
        allowed_signers,
        signer_identity,
        "custodian_attests_signature_completed_before_holdout_decode",
        "pre-HOLDOUT",
    )
    freeze = holdout["acquisition_analysis_freeze"]
    require(check_file_hash(path, freeze["receipt_file"], freeze["receipt_sha256"], f"{path}: acquisition receipt") == acquisition_path.resolve(), f"{path}: acquisition receipt path")
    require(freeze["model_render_contract_canonicalization"] == CANONICALIZATION, f"{path}: canonicalization")
    require(freeze["protocol_git_commit"] == acquisition["versioned_contract"]["protocol_git_commit"], f"{path}: protocol commit")
    require(freeze["analyzer_sha256"] == acquisition["versioned_contract"]["analyzer_sha256"], f"{path}: analyzer hash")
    require(freeze["model_render_contract_canonical_json_sha256"] == acquisition["versioned_contract"]["model_render_contract_canonical_json_sha256"], f"{path}: model contract hash")
    require(freeze["analysis_contract_canonical_json_sha256"] == acquisition["versioned_contract"]["analysis_contract_canonical_json_sha256"], f"{path}: analysis contract hash")
    train_ids = {cluster_id for cluster_id, (_, _, entry) in manifests.items() if entry["split"] == "train"}
    require({item["cluster_id"] for item in holdout["train_evidence"]} == train_ids, f"{path}: TRAIN evidence clusters")
    train_results = {}
    for item in holdout["train_evidence"]:
        manifest_path, _, entry = manifests[item["cluster_id"]]
        require(item["manifest_sha256"] == entry["manifest_sha256"], f"{path}: TRAIN manifest hash")
        receipt_path = check_file_hash(path, item["result_receipt_file"], item["result_receipt_sha256"], f"{path}: TRAIN result")
        result = validate_result(receipt_path, acquisition_path, allowed_signers, signer_identity)
        require(result["cluster_id"] == item["cluster_id"] and result["manifest"]["sha256"] == sha256(manifest_path), f"{path}: TRAIN result linkage")
        require(result["real_effect"]["all_four_profiles_resolved"] is True, f"{path}: TRAIN profile resolution")
        train_results[item["cluster_id"]] = result
    centred = {
        cluster_id: {
            profile: values["centred_G_db"]
            for profile, values in result["_computed_real_profiles"].items()
        }
        for cluster_id, result in train_results.items()
    }
    train_cluster_ids = sorted(train_results)
    pairwise_dots = [
        sum(float(left) * float(right) for left, right in zip(centred[train_cluster_ids[first]][profile], centred[train_cluster_ids[second]][profile]))
        for profile in sorted(PROFILES)
        for first in range(3)
        for second in range(first + 1, 3)
    ]
    require(len(pairwise_dots) == 12 and all(dot > 0.0 for dot in pairwise_dots), f"{path}: TRAIN corresponding profile pairwise dot product")
    holdout_ids = {cluster_id for cluster_id, (_, _, entry) in manifests.items() if entry["split"] == "holdout"}
    require({item["cluster_id"] for item in holdout["holdouts"]} == holdout_ids, f"{path}: HOLDOUT clusters")
    for item in holdout["holdouts"]:
        manifest_path, _, entry = manifests[item["cluster_id"]]
        require(item["manifest_sha256"] == entry["manifest_sha256"] and check_file_hash(path, item["manifest_file"], item["manifest_sha256"], f"{path}: HOLDOUT manifest") == manifest_path and item["allowed_decode_count"] == 1, f"{path}: HOLDOUT manifest/decode")
    candidate = holdout["candidate"]
    artifact_pairs(path, candidate, (
        ("source_tree_receipt_file", "source_tree_receipt_sha256"),
        ("renderer_executable_file", "renderer_executable_sha256"),
        ("renderer_source_file", "renderer_source_sha256"),
        ("model_preset_file", "model_preset_sha256"),
        ("code_freeze_receipt_file", "code_freeze_receipt_sha256"),
    ), "candidate")
    derivation_path = check_file_hash(path, candidate["coefficient"]["derivation_receipt_file"], candidate["coefficient"]["derivation_receipt_sha256"], "candidate coefficient")
    derivation = read_json(derivation_path)
    validate_template_shape(derivation, read_json(HERE / "coefficient-receipt.template.json"), str(derivation_path))
    scan_filled(derivation)
    require(derivation["schema"] == "electry-repick-phase-coefficient-derivation/v1" and derivation["status"] == "sealed_before_candidate_holdout_freeze" and derivation["study_id"] == holdout["study_id"], f"{path}: coefficient receipt identity/status")
    require(derivation["analysis_preregistration_sha256"] == acquisition["versioned_contract"]["analysis_preregistration_sha256"], f"{path}: coefficient preregistration binding")
    expected_train_results = {item["cluster_id"]: item["result_receipt_sha256"] for item in holdout["train_evidence"]}
    reported_train_results = {item["cluster_id"]: item["result_receipt_sha256"] for item in derivation["train_results"]}
    require(len(derivation["train_results"]) == 3 and reported_train_results == expected_train_results, f"{path}: coefficient TRAIN-result binding")
    require(derivation["coefficient"] == {key: candidate["coefficient"][key] for key in ("name", "value", "units", "registered_contractive_range_inclusive", "sample_rate_semantics")}, f"{path}: coefficient receipt value/semantics")
    require(candidate["coefficient"]["sample_rate_semantics"] == "one_shot_total_applied_once_at_second_diagnostic_contact_independent_of_sample_rate_and_contact_duration", f"{path}: candidate sample-rate semantics")
    require(candidate["model_preset_sha256"] == acquisition["shipping_baseline"]["model_preset_sha256"], f"{path}: candidate preset changed")
    require(candidate["model_seed"] == acquisition["shipping_baseline"]["model_seed"], f"{path}: candidate seed changed")
    require(hex_digest(candidate["model_source_commit"], 40), f"{path}: candidate model source commit")
    require(finite_number(candidate["coefficient"]["value"]) and 0.0 <= candidate["coefficient"]["value"] <= 2.0, f"{path}: candidate coefficient outside registered contractive range")
    require(candidate["one_fitted_parameter_only"] is True and candidate["derived_from_train_only"] is True and candidate["shipping_default_off"] is True, f"{path}: candidate scope")
    train_gate = holdout["train_gate"]
    require(train_gate == {
        "all_three_clusters_complete": True,
        "all_string_direction_profiles_resolve_phase": True,
        "all_corresponding_centred_profile_pairwise_dot_products_positive": True,
        "coefficient_derived_only_after_gate_passed": True,
        "status": "passed_all_registered_train_gates",
    }, f"{path}: TRAIN gate")
    return holdout


def validate_rail_evidence(
    rails_path: Path,
    rails_receipt: dict,
    acquisition: dict,
    holdout: dict,
) -> dict[str, bool]:
    evidence = rails_receipt["rail_evidence"]
    artifact_pairs(rails_path, evidence, (
        ("rail_test_driver_file", "rail_test_driver_sha256"),
        ("rail_test_schedule_file", "rail_test_schedule_sha256"),
        ("shipping_test_executable_file", "shipping_test_executable_sha256"),
        ("candidate_test_executable_file", "candidate_test_executable_sha256"),
        ("shipping_source_tree_receipt_file", "shipping_source_tree_receipt_sha256"),
        ("candidate_source_tree_receipt_file", "candidate_source_tree_receipt_sha256"),
        ("shipping_build_receipt_file", "shipping_build_receipt_sha256"),
        ("candidate_build_receipt_file", "candidate_build_receipt_sha256"),
        ("raw_result_file", "raw_result_sha256"),
        ("raw_log_file", "raw_log_sha256"),
    ), "rail_evidence")
    require(evidence["rail_test_driver_sha256"] == acquisition["versioned_contract"]["rail_test_driver_sha256"], f"{rails_path}: rail driver differs from acquisition freeze")
    require(evidence["rail_test_schedule_sha256"] == acquisition["versioned_contract"]["rail_test_schedule_sha256"], f"{rails_path}: rail schedule differs from acquisition freeze")
    require(evidence["shipping_source_tree_receipt_sha256"] == acquisition["shipping_baseline"]["renderer_source_tree_receipt_sha256"], f"{rails_path}: rail shipping source differs from acquisition freeze")
    require(evidence["candidate_source_tree_receipt_sha256"] == holdout["candidate"]["source_tree_receipt_sha256"], f"{rails_path}: rail candidate source differs from HOLDOUT freeze")
    validate_build_pair(rails_path, evidence, acquisition, holdout, evidence["shipping_test_executable_sha256"], evidence["candidate_test_executable_sha256"])
    scheduled = validate_rail_schedule(resolve(rails_path, evidence["rail_test_schedule_file"]), evidence["rail_test_driver_sha256"])

    raw_path = resolve(rails_path, evidence["raw_result_file"])
    raw = read_json(raw_path)
    validate_template_shape(raw, read_json(HERE / "rail-test-result.template.json"), str(raw_path))
    scan_filled(raw)
    require(raw["schema"] == "electry-repick-phase-rail-test-result/v1" and raw["study_id"] == rails_receipt["study_id"], f"{raw_path}: rail result identity")
    require(raw["compatibility_rail_contract_canonical_json_sha256"] == canonical_hash(rails_receipt["compatibility_rail_contract"]), f"{raw_path}: rail contract hash")
    require(raw["rail_test_driver_sha256"] == evidence["rail_test_driver_sha256"], f"{raw_path}: rail driver hash")
    require(raw["rail_test_schedule_sha256"] == evidence["rail_test_schedule_sha256"], f"{raw_path}: rail schedule hash")
    require(raw["shipping_test_executable_sha256"] == evidence["shipping_test_executable_sha256"] and raw["candidate_test_executable_sha256"] == evidence["candidate_test_executable_sha256"], f"{raw_path}: rail test executable hashes")
    require(raw["shipping_source_tree_receipt_sha256"] == evidence["shipping_source_tree_receipt_sha256"] and raw["candidate_source_tree_receipt_sha256"] == evidence["candidate_source_tree_receipt_sha256"], f"{raw_path}: rail source receipt hashes")
    require(raw["raw_log_sha256"] == evidence["raw_log_sha256"], f"{raw_path}: rail raw log hash")
    require(isinstance(raw["process_exit_code"], int) and not isinstance(raw["process_exit_code"], bool) and 0 <= raw["process_exit_code"] <= 255, f"{raw_path}: process exit code")
    rows = raw["rail_results"]
    require(len(rows) == len(RAIL_KEYS) and all(isinstance(row, dict) and set(row) == set(raw["required_rail_result_schema"]) for row in rows), f"{raw_path}: rail result schema/count")
    by_name = {row["rail"]: row for row in rows}
    require(set(by_name) == set(RAIL_KEYS), f"{raw_path}: registered rail names")
    derived = {}
    for name in RAIL_KEYS:
        row = by_name[name]
        require(row["registered_test_id"] == name, f"{raw_path}: registered test id {name}")
        count_fields = ("scheduled_case_count", "completed_case_count", "failed_case_count")
        completed_ids = row["completed_case_ids"]
        failed_ids = row["failed_case_ids"]
        require(all(isinstance(row[key], int) and not isinstance(row[key], bool) for key in count_fields) and isinstance(completed_ids, list) and isinstance(failed_ids, list) and isinstance(row["passed"], bool), f"{raw_path}: rail result types {name}")
        require(all(isinstance(case_id, str) for case_id in completed_ids + failed_ids) and len(completed_ids) == len(set(completed_ids)) and len(failed_ids) == len(set(failed_ids)), f"{raw_path}: rail result case ID types/duplicates {name}")
        completed_set = set(completed_ids)
        failed_set = set(failed_ids)
        require(row["scheduled_case_count"] == len(scheduled[name]) and row["completed_case_count"] == len(completed_ids) and row["failed_case_count"] == len(failed_ids), f"{raw_path}: rail result counts {name}")
        require(completed_set <= scheduled[name] and failed_set <= completed_set, f"{raw_path}: unknown completed/failed rail case {name}")
        passed = raw["process_exit_code"] == 0 and completed_set == scheduled[name] and not failed_set
        require(row["passed"] == passed, f"{raw_path}: derived rail result {name}")
        derived[name] = passed
    return derived


def validate_cpu_evidence(
    rails_path: Path,
    rails_receipt: dict,
    acquisition: dict,
    holdout: dict,
) -> tuple[float, float]:
    evidence = rails_receipt["cpu_evidence"]
    artifact_pairs(rails_path, evidence, (
        ("benchmark_driver_file", "benchmark_driver_sha256"),
        ("shipping_benchmark_executable_file", "shipping_benchmark_executable_sha256"),
        ("candidate_benchmark_executable_file", "candidate_benchmark_executable_sha256"),
        ("shipping_build_receipt_file", "shipping_build_receipt_sha256"),
        ("candidate_build_receipt_file", "candidate_build_receipt_sha256"),
        ("host_fingerprint_file", "host_fingerprint_sha256"),
        ("raw_timing_file", "raw_timing_sha256"),
    ), "cpu_evidence")
    require(evidence["benchmark_driver_sha256"] == acquisition["versioned_contract"]["cpu_benchmark_driver_sha256"], f"{rails_path}: CPU driver differs from acquisition freeze")
    require(evidence["shipping_benchmark_executable_sha256"] == acquisition["shipping_baseline"]["renderer_executable_sha256"], f"{rails_path}: CPU shipping executable differs from acquisition freeze")
    require(evidence["candidate_benchmark_executable_sha256"] == holdout["candidate"]["renderer_executable_sha256"], f"{rails_path}: CPU candidate executable differs from HOLDOUT freeze")
    validate_build_pair(rails_path, evidence, acquisition, holdout, evidence["shipping_benchmark_executable_sha256"], evidence["candidate_benchmark_executable_sha256"])

    raw_path = resolve(rails_path, evidence["raw_timing_file"])
    raw = read_json(raw_path)
    validate_template_shape(raw, read_json(HERE / "cpu-timing.template.json"), str(raw_path))
    scan_filled(raw)
    require(raw["schema"] == "electry-repick-phase-cpu-timing/v1" and raw["study_id"] == rails_receipt["study_id"], f"{raw_path}: CPU timing identity")
    require(raw["benchmark_contract_canonical_json_sha256"] == canonical_hash(rails_receipt["cpu_benchmark_contract"]), f"{raw_path}: CPU benchmark contract hash")
    require(raw["benchmark_driver_sha256"] == evidence["benchmark_driver_sha256"], f"{raw_path}: CPU driver hash")
    require(raw["shipping_benchmark_executable_sha256"] == evidence["shipping_benchmark_executable_sha256"] and raw["candidate_benchmark_executable_sha256"] == evidence["candidate_benchmark_executable_sha256"], f"{raw_path}: CPU executable hashes")
    require(raw["host_fingerprint_sha256"] == evidence["host_fingerprint_sha256"], f"{raw_path}: CPU host fingerprint hash")

    expected = {
        (sample_rate, block_size, repicks)
        for sample_rate in (96000, 384000)
        for block_size in (16, 64, 512)
        for repicks in (4, 12, 20)
    }
    scenarios = raw["scenarios"]
    require(len(scenarios) == len(expected) and all(isinstance(item, dict) and set(item) == set(raw["required_scenario_schema"]) for item in scenarios), f"{raw_path}: CPU scenario schema/count")
    scenario_medians = {96000: [], 384000: []}
    seen = set()
    for scenario in scenarios:
        keys = (scenario["sample_rate_hz"], scenario["block_size_samples"], scenario["repicks_per_second_per_string"])
        require(all(isinstance(value, int) and not isinstance(value, bool) for value in keys) and keys in expected and keys not in seen, f"{raw_path}: CPU scenario identity {keys}")
        seen.add(keys)
        rounds = scenario["rounds"]
        require(len(rounds) == 20 and all(isinstance(item, dict) and set(item) == set(raw["required_round_schema"]) for item in rounds), f"{raw_path}: CPU rounds {keys}")
        overheads = []
        for index, round_result in enumerate(rounds):
            expected_order = "ABBA" if index % 2 == 0 else "BAAB"
            require(round_result["order"] == expected_order, f"{raw_path}: CPU round order {keys}/{index}")
            shipping = round_result["shipping_render_seconds"]
            candidate = round_result["candidate_render_seconds"]
            require(isinstance(shipping, list) and isinstance(candidate, list) and len(shipping) == 2 and len(candidate) == 2 and all(finite_number(value) and value > 0.0 for value in shipping + candidate), f"{raw_path}: CPU round timings {keys}/{index}")
            require(finite_number(round_result["paired_overhead_percent"]), f"{raw_path}: CPU round overhead type {keys}/{index}")
            overhead = 100.0 * (median(candidate) / median(shipping) - 1.0)
            require(math.isfinite(overhead) and close(round_result["paired_overhead_percent"], overhead), f"{raw_path}: CPU round overhead {keys}/{index}")
            overheads.append(overhead)
        scenario_median = median(overheads)
        require(finite_number(scenario["scenario_median_paired_overhead_percent"]) and close(scenario["scenario_median_paired_overhead_percent"], scenario_median), f"{raw_path}: CPU scenario median {keys}")
        scenario_medians[keys[0]].append(scenario_median)
    require(seen == expected and all(len(values) == 9 for values in scenario_medians.values()), f"{raw_path}: CPU scenario coverage")
    return median(scenario_medians[96000]), max(scenario_medians[384000])


def validate_final(
    path: Path,
    holdout_path: Path,
    holdout: dict,
    acquisition_path: Path,
    allowed_signers: Path,
    signer_identity: str,
) -> None:
    final = read_json(path)
    acquisition = read_json(acquisition_path)
    validate_template_shape(final, read_json(HERE / "final-receipt.template.json"), str(path))
    scan_filled(final)
    require(final["schema"] == "electry-repick-phase-final-result/v1", f"{path}: schema")
    require(final["status"] == "sealed_before_promotion_decision", f"{path}: status")
    require(final["study_id"] == holdout["study_id"], f"{path}: study_id")
    require(resolve(holdout_path, holdout["post_decode_receipt"]["file"]) == path.resolve(), f"{path}: final receipt path differs from HOLDOUT freeze")
    require(final["decision"] in ("promote", "do_not_promote", "inconclusive"), f"{path}: decision")
    require(check_file_hash(path, final["holdout_freeze"]["file"], final["holdout_freeze"]["sha256"], f"{path}: HOLDOUT freeze") == holdout_path.resolve(), f"{path}: wrong HOLDOUT freeze")
    require(len(final["holdout_results"]) == 2 and len({item["cluster_id"] for item in final["holdout_results"]}) == 2, f"{path}: HOLDOUT result count")
    frozen_holdouts = {item["cluster_id"]: item for item in holdout["holdouts"]}
    results = {}
    for item in final["holdout_results"]:
        require(item["cluster_id"] in frozen_holdouts, f"{path}: unknown HOLDOUT result cluster")
        result_path = check_file_hash(path, item["result_receipt_file"], item["result_receipt_sha256"], f"{path}: HOLDOUT result")
        require(result_path == resolve(holdout_path, frozen_holdouts[item["cluster_id"]]["result_receipt_file"]), f"{path}: result path differs from HOLDOUT freeze")
        result = validate_result(result_path, acquisition_path, allowed_signers, signer_identity, holdout_path)
        require(result["cluster_id"] == item["cluster_id"] and result["manifest"]["sha256"] == frozen_holdouts[item["cluster_id"]]["manifest_sha256"], f"{path}: HOLDOUT result linkage")
        results[item["cluster_id"]] = result
    first, second = [results[item["cluster_id"]] for item in final["holdout_results"]]
    first_profiles = first["_computed_real_profiles"]
    second_profiles = second["_computed_real_profiles"]
    dot_names = {("e1", "down"): "e1_down", ("e1", "up"): "e1_up", ("e2", "down"): "e2_down", ("e2", "up"): "e2_up"}
    dots = {}
    for key, name in dot_names.items():
        if first_profiles[key]["resolved"] and second_profiles[key]["resolved"]:
            dot = sum(float(left) * float(right) for left, right in zip(first_profiles[key]["centred_G_db"], second_profiles[key]["centred_G_db"]))
            require(finite_number(final["real_effect_replication"]["corresponding_profile_dot_products"][name]) and close(final["real_effect_replication"]["corresponding_profile_dot_products"][name], dot), f"{path}: profile dot product {name}")
            dots[name] = dot
        else:
            require(final["real_effect_replication"]["corresponding_profile_dot_products"][name] == INCONCLUSIVE, f"{path}: inconclusive profile dot product {name}")
    resolved = all(item["resolved"] is True for profiles in (first_profiles, second_profiles) for item in profiles.values())
    all_positive = resolved and len(dots) == 4 and all(value > 0.0 for value in dots.values())
    replicated = resolved and all_positive
    require(final["real_effect_replication"]["all_positive"] == all_positive, f"{path}: dot-product gate")
    require(all(isinstance(final["real_effect_replication"][key], bool) for key in ("all_positive", "both_holdouts_all_profiles_resolved", "passed")), f"{path}: real replication booleans")
    require(final["real_effect_replication"]["both_holdouts_all_profiles_resolved"] == resolved and final["real_effect_replication"]["passed"] == replicated, f"{path}: real replication gate")
    promotion = {item["cluster_id"]: item for item in final["promotion_by_holdout"]}
    require(set(promotion) == set(results), f"{path}: promotion clusters")
    for cluster_id, result in results.items():
        summary = result["model_comparison"]["summary"]
        item = promotion[cluster_id]
        require(isinstance(item["median_reduction_at_least_0_50"], bool) and isinstance(item["maximum_error_not_worse"], bool) and isinstance(item["passed"], bool), f"{path}: {cluster_id} promotion booleans")
        if result["model_comparison"]["status"] == INCONCLUSIVE:
            metric_fields = (
                "shipping_median_absolute_error_db", "candidate_median_absolute_error_db",
                "median_error_reduction_fraction_or_rule", "shipping_maximum_absolute_error_db",
                "candidate_maximum_absolute_error_db",
            )
            require(summary == {} and all(item[key] == INCONCLUSIVE for key in metric_fields) and item["median_reduction_at_least_0_50"] is False and item["maximum_error_not_worse"] is False and item["passed"] is False, f"{path}: {cluster_id} inconclusive promotion receipt")
            continue
        for key in (
            "shipping_median_absolute_error_db", "candidate_median_absolute_error_db",
            "median_error_reduction_fraction_or_rule", "shipping_maximum_absolute_error_db",
            "candidate_maximum_absolute_error_db", "median_reduction_at_least_0_50",
            "maximum_error_not_worse",
        ):
            if isinstance(summary[key], (int, float)) and not isinstance(summary[key], bool):
                require(close(item[key], summary[key]), f"{path}: {cluster_id} {key}")
            else:
                require(item[key] == summary[key], f"{path}: {cluster_id} {key}")
        require(item["passed"] == (item["median_reduction_at_least_0_50"] and item["maximum_error_not_worse"]), f"{path}: {cluster_id} promotion gate")
    rails = final["existing_rails"]
    rails_path = check_file_hash(path, rails["receipt_file"], rails["receipt_sha256"], f"{path}: rails")
    rails_receipt = read_json(rails_path)
    validate_template_shape(rails_receipt, read_json(HERE / "rails-receipt.template.json"), str(rails_path))
    scan_filled(rails_receipt)
    require(rails_receipt["schema"] == "electry-repick-phase-rails-result/v1" and rails_receipt["status"] == "sealed_before_promotion_decision", f"{path}: rails receipt schema/status")
    require(rails_receipt["study_id"] == final["study_id"] and rails_receipt["holdout_freeze_sha256"] == final["holdout_freeze"]["sha256"], f"{path}: rails receipt study/HOLDOUT linkage")
    require(rails_receipt["candidate_source_tree_receipt_sha256"] == holdout["candidate"]["source_tree_receipt_sha256"] and rails_receipt["candidate_renderer_executable_sha256"] == holdout["candidate"]["renderer_executable_sha256"], f"{path}: rails receipt candidate linkage")
    copied_rails = {key: value for key, value in rails.items() if key not in ("receipt_file", "receipt_sha256")}
    receipt_rails = {key: value for key, value in rails_receipt.items() if key in copied_rails}
    require(receipt_rails == copied_rails, f"{path}: copied rails differ from hashed receipt")
    derived_rails = validate_rail_evidence(rails_path, rails_receipt, acquisition, holdout)
    require(all(rails[name] == derived_rails[name] for name in RAIL_KEYS), f"{path}: compatibility rails differ from raw frozen test result")
    cpu_96, cpu_384 = validate_cpu_evidence(rails_path, rails_receipt, acquisition, holdout)
    require(finite_number(rails["cpu_median_overhead_at_96khz_percent"]) and finite_number(rails["cpu_worst_case_overhead_at_384khz_percent"]), f"{path}: CPU rail measurements")
    require(close(rails["cpu_median_overhead_at_96khz_percent"], cpu_96) and close(rails["cpu_worst_case_overhead_at_384khz_percent"], cpu_384), f"{path}: CPU rails differ from raw timing result")
    require(all(isinstance(rails[key], bool) for key in RAIL_KEYS) and isinstance(rails["all_passed"], bool), f"{path}: rail booleans")
    rails_pass = all(rails[key] is True for key in RAIL_KEYS) and cpu_96 <= 1.0 and cpu_384 <= 3.0
    require(rails["all_passed"] == rails_pass, f"{path}: rail aggregate")
    all_passed = replicated and rails_pass and all(item["passed"] for item in promotion.values())
    require(isinstance(final["all_registered_gates_passed"], bool), f"{path}: aggregate gate type")
    require(final["all_registered_gates_passed"] == all_passed, f"{path}: aggregate gate")
    analysis_inconclusive = not replicated or any(result["model_comparison"]["status"] == INCONCLUSIVE for result in results.values())
    if analysis_inconclusive:
        require(final["decision"] == "inconclusive", f"{path}: unresolved real/model evidence requires inconclusive decision")
    elif all_passed:
        require(final["decision"] in ("promote", "do_not_promote"), f"{path}: invalid passing decision")
    else:
        require(final["decision"] == "do_not_promote", f"{path}: failed promotion/rail gate requires do_not_promote")


def validate_templates() -> None:
    manifest = read_json(HERE / "manifest.template.json")
    acquisition = read_json(HERE / "acquisition-freeze.template.json")
    holdout = read_json(HERE / "holdout-freeze.template.json")
    result = read_json(HERE / "result-receipt.template.json")
    event_detail = read_json(HERE / "event-detail.template.json")
    damping = read_json(HERE / "damping-receipt.template.json")
    decode_ledger = read_json(HERE / "decode-ledger.template.json")
    coefficient = read_json(HERE / "coefficient-receipt.template.json")
    build_receipt = read_json(HERE / "build-receipt.template.json")
    cpu_timing = read_json(HERE / "cpu-timing.template.json")
    rail_test_schedule = read_json(HERE / "rail-test-schedule.template.json")
    rail_test_result = read_json(HERE / "rail-test-result.template.json")
    rails = read_json(HERE / "rails-receipt.template.json")
    final = read_json(HERE / "final-receipt.template.json")
    performance = manifest["performance"]
    require(performance["pairs_per_string"] == 64 == len(performance["target_phase_fractions"]) * performance["pairs_per_target_phase"], "template pair count")
    require(performance["second_strokes_per_phase"] == {"down": 8, "up": 8}, "template direction count")
    require(len(manifest["takes"]) == 2 and all(item["pairs"] == 64 for item in manifest["takes"]), "template takes")
    require(len(acquisition["clusters"]) == 5 and Counter(item["split"] for item in acquisition["clusters"]) == Counter({"train": 3, "holdout": 2}), "template clusters")
    require(acquisition["versioned_contract"]["model_render_contract_canonicalization"] == CANONICALIZATION, "template canonicalization")
    require(tuple(acquisition["versioned_contract"]["analysis_contract_source_json_pointers"]) == ANALYSIS_POINTERS, "template analysis pointers")
    protocol_hash_fields = {
        "capture_readme_sha256", "manifest_template_sha256", "validator_sha256",
        "acquisition_freeze_template_sha256", "holdout_freeze_template_sha256",
        "result_receipt_template_sha256", "event_detail_template_sha256",
        "damping_receipt_template_sha256", "decode_ledger_template_sha256",
        "coefficient_receipt_template_sha256", "build_receipt_template_sha256",
        "cpu_timing_template_sha256",
        "rail_test_schedule_template_sha256",
        "rail_test_result_template_sha256",
        "rails_receipt_template_sha256", "final_receipt_template_sha256",
    }
    require(all(acquisition["versioned_contract"][key] == "0" * 64 for key in protocol_hash_fields), "template protocol file hashes")
    require(set(acquisition["pretrain_detached_anchor"]) == {
        "sha256_file", "custodian_signature_file", "signature_method", "signature_namespace",
        "signer_identity", "pretrusted_allowed_signers_sha256",
        "custodian_attests_signature_completed_before_train_decode",
    }, "template external signature trust root")
    require(set(holdout["preholdout_detached_anchor"]) == {
        "sha256_file", "custodian_signature_file", "signature_method", "signature_namespace",
        "signer_identity", "pretrusted_allowed_signers_sha256",
        "custodian_attests_signature_completed_before_holdout_decode",
    }, "template HOLDOUT external signature trust root")
    require(len(holdout["train_evidence"]) == 3 and len(holdout["holdouts"]) == 2 and holdout["registered_holdout_gates"]["registered_cells_per_holdout"] == 16, "template holdout counts")
    require(result_cells(result) == CELLS and len(result["real_cells"]) == 16 and len(result["real_profiles"]) == 4, "template result cells")
    require(result["model_comparison"]["summary"].startswith("REPLACE_"), "template result summary variant placeholder")
    require(result["analyzer"]["analysis_contract_canonical_json_sha256"] == "0" * 64, "template result analysis hash")
    require(event_detail["schema"] == "electry-repick-phase-event-detail/v1" and event_detail["real_events"] == [] and event_detail["model_events"] == [], "template event detail")
    require(event_detail["analysis_contract_canonical_json_sha256"] == "0" * 64, "template event-detail analysis hash")
    require(tuple(event_detail["registered_exclusion_reasons"]) == EXCLUSION_REASONS, "template exclusion reasons")
    require(set(event_detail["required_real_event_schema"]) == {
        "string", "trial", "second_stroke_direction", "eligible", "measured_ioi_ms",
        "contact_onset_half_width_ms", "measured_phase_degrees",
        "calibration_phase_half_width_degrees", "fit_phase_half_width_degrees",
        "f0_phase_half_width_degrees", "phase_interval_half_width_degrees",
        "f0_hz", "f0_interval_hz", "pre_normal_motion_h1_snr_db",
        "pre_bridge_di_h1_h4_snr_db", "extra_transient_detected",
        "clipping_or_nonfinite_detected", "quadrant_centre_degrees", "G_db", "exclusion_reasons",
    }, "template real event fields")
    require(set(event_detail["required_model_event_schema"]) == {
        "variant", "source_real_string", "source_real_trial", "midi_note", "physical_string_number",
        "first_stroke_direction", "second_stroke_direction", "first_velocity", "second_velocity", "eligible",
        "diagnostic_first_contact_sample", "diagnostic_second_contact_sample",
        "diagnostic_note_off_sample", "render_end_sample", "precontact_identity_sha256",
        "f0_hz", "f0_interval_hz", "measured_phase_degrees",
        "fit_phase_half_width_degrees", "f0_phase_half_width_degrees",
        "phase_interval_half_width_degrees", "quadrant_centre_degrees", "G_db", "exclusion_reasons",
    }, "template model event fields")
    damping_keys = ("e1_target_results_db_physical_strings_7_to_1_each_three_strokes", "e2_target_results_db_physical_strings_8_7_5_4_3_2_1_each_three_strokes")
    require(damping["schema"] == "electry-repick-phase-damping-result/v1" and all(len(damping[key]) == 7 and all(len(row) == 3 for row in damping[key]) for key in damping_keys), "template damping receipt")
    require(decode_ledger["schema"] == "electry-repick-phase-decode-ledger/v1" and decode_ledger["attempt_count"] == 1 and decode_ledger["attempt"]["ordinal"] == 1, "template decode ledger")
    require(set(decode_ledger["custodian_attestation"]) == {
        "no_prior_parallel_or_unlogged_decode", "no_second_decode_authorized", "sha256_file",
        "custodian_signature_file", "signature_method", "signature_namespace", "signer_identity",
        "pretrusted_allowed_signers_sha256", "custodian_attests_ledger_sealed_before_any_listening",
    }, "template decode-ledger trust root")
    require(coefficient["schema"] == "electry-repick-phase-coefficient-derivation/v1" and len(coefficient["train_results"]) == 3 and coefficient["one_fitted_parameter_only"] is True and coefficient["derived_from_train_only"] is True and coefficient["holdout_data_or_forbidden_metadata_accessed"] is False, "template coefficient receipt")
    require(build_receipt["schema"] == "electry-repick-phase-build-receipt/v1" and build_receipt["common_build_contract"]["configuration"] == "Release" and build_receipt["candidate_contact_variant"]["feature"] == "electry_phase_repick_candidate", "template build receipt")
    require(cpu_timing["schema"] == "electry-repick-phase-cpu-timing/v1" and cpu_timing["scenarios"] == [] and set(cpu_timing["required_scenario_schema"]) == {"sample_rate_hz", "block_size_samples", "repicks_per_second_per_string", "rounds", "scenario_median_paired_overhead_percent"} and set(cpu_timing["required_round_schema"]) == {"order", "shipping_render_seconds", "candidate_render_seconds", "paired_overhead_percent"}, "template CPU timing")
    require(rail_test_schedule["schema"] == "electry-repick-phase-rail-test-schedule/v1" and rail_test_schedule["cases"] == [] and set(rail_test_schedule["required_case_schema"]) == {"rail", "case_id"}, "template rail test schedule")
    require(rail_test_result["schema"] == "electry-repick-phase-rail-test-result/v1" and rail_test_result["rail_results"] == [] and set(rail_test_result["required_rail_result_schema"]) == {"rail", "registered_test_id", "scheduled_case_count", "completed_case_count", "failed_case_count", "completed_case_ids", "failed_case_ids", "passed"}, "template rail test result")
    require(rails["schema"] == "electry-repick-phase-rails-result/v1" and rails["cpu_limits_inclusive"] is True, "template rails receipt")
    require(tuple(rails["compatibility_rail_contract"]["registered_test_ids"]) == RAIL_KEYS, "template registered compatibility rails")
    require(len(final["holdout_results"]) == 2 and len(final["promotion_by_holdout"]) == 2, "template final counts")
    print("phase-repick templates: PASS")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("files", nargs="*", type=Path, help="acquisition-freeze.json [holdout-freeze.json [final-receipt.json]]")
    parser.add_argument("--templates", action="store_true", help="validate the versioned templates")
    parser.add_argument("--allowed-signers", type=Path, help="pretrusted OpenSSH allowed-signers file, held outside the study receipt")
    parser.add_argument("--signer-identity", help="pretrusted custodian identity in the allowed-signers file")
    parser.add_argument("--train-result", action="append", type=Path, default=[], help="validate a sealed TRAIN result without creating a HOLDOUT freeze; repeat as needed")
    args = parser.parse_args()
    try:
        if args.templates:
            require(not args.files and args.allowed_signers is None and args.signer_identity is None and not args.train_result, "--templates takes no other arguments")
            validate_templates()
            return 0
        require(1 <= len(args.files) <= 3, "provide acquisition, optional HOLDOUT freeze, and optional final receipt")
        require(args.allowed_signers is not None and args.signer_identity, "filled-chain validation requires --allowed-signers and --signer-identity from a pretrusted source")
        acquisition_path = args.files[0].resolve()
        acquisition, manifests = validate_acquisition(acquisition_path, args.allowed_signers.resolve(), args.signer_identity)
        if args.train_result:
            require(len(args.files) == 1 and len({path.resolve() for path in args.train_result}) == len(args.train_result), "--train-result requires only the acquisition positional file and distinct result paths")
            result_clusters = set()
            for result_path in args.train_result:
                result = validate_result(result_path.resolve(), acquisition_path, args.allowed_signers.resolve(), args.signer_identity)
                require(result["split"] == "train", f"{result_path}: --train-result accepts TRAIN only")
                result_clusters.add(result["cluster_id"])
            require(len(result_clusters) == len(args.train_result), "--train-result duplicate cluster")
            print("phase-repick TRAIN result chain: PASS")
            return 0
        if len(args.files) >= 2:
            holdout_path = args.files[1].resolve()
            holdout = validate_holdout(holdout_path, acquisition_path, acquisition, manifests, args.allowed_signers.resolve(), args.signer_identity)
        if len(args.files) == 3:
            validate_final(args.files[2].resolve(), holdout_path, holdout, acquisition_path, args.allowed_signers.resolve(), args.signer_identity)
        print("phase-repick freeze chain: PASS")
        return 0
    except (Invalid, KeyError, TypeError, IndexError, OSError) as exc:
        print(f"phase-repick validation: FAIL: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
