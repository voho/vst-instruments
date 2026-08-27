#!/usr/bin/env python3

import hashlib
import json
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def digest(label):
    return hashlib.sha256(label.encode("utf-8")).hexdigest()


def canonical_sha256(value):
    data = json.dumps(
        value, ensure_ascii=True, separators=(",", ":"), sort_keys=True).encode("utf-8")
    return hashlib.sha256(data).hexdigest()


def write_pcm24(path, value, frames):
    payload = value.to_bytes(3, "little", signed=True) * frames
    padding = b"\0" if len(payload) & 1 else b""
    path.write_bytes(
        b"RIFF" + struct.pack("<I", 36 + len(payload) + len(padding)) + b"WAVE"
        b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, 44100, 132300, 3, 24)
        + b"data" + struct.pack("<I", len(payload)) + payload + padding)
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _qc(frames, hits):
    if hits == 1:
        onsets = [2205]
    else:
        step = (frames - 4410) // hits
        onsets = [2205 + step * index for index in range(hits)]
    record = {
        "physical_onset_frames": onsets,
        "electry_onset_frames": onsets,
        "physical_attenuation_db": 0.5,
        "electry_attenuation_db": 0.0,
        "post_match_delta_db": 0.0,
        "final_true_peak_dbtp": -3.1,
        "passed": True,
        "failure_flags": [],
    }
    return {**record, "record_sha256": canonical_sha256(record)}


def build_valid_study(root, prepare, study_id="self-test", presentation_seed=None):
    template = (ROOT / "Docs" / "capture" / "electry-mute-capture-v1"
                / "comparison-manifest.template.json")
    comparison = json.loads(template.read_text(encoding="utf-8"))
    presentation_seed = presentation_seed or digest(study_id + "-presentation-seed")
    selection_seed = digest(study_id + "-selection-seed")

    def pair(pair_id, number, frames):
        physical = root / f"source-{number:02d}-physical.wav"
        electry = root / f"source-{number:02d}-electry.wav"
        return {
            "id": pair_id,
            "physical": {
                "path": physical.name,
                "sha256": write_pcm24(physical, number * 100, frames),
            },
            "electry": {
                "path": electry.name,
                "sha256": write_pcm24(electry, number * 100 + 50, frames),
            },
        }

    practice = [pair("practice-1", 1, 30870), pair("practice-2", 2, 30870)]
    cells = [pair(number, number + 2, prepare.CORE_FRAMES[number])
             for number in range(1, 11)]

    all_take_files = list(prepare.CAPTURE_TAKE_FILES)
    capture_takes = {}
    normalized_sources = {}

    def capture(session_id, player_id, guitar_id, split, take_files):
        takes = []
        for name in take_files:
            spec = prepare.CAPTURE_TAKE_SPECS[name]
            take = {
                "file": name,
                **spec,
                "frames": (1719900 if spec["kind"] == "isolated"
                           else 407007 if spec["kind"] == "rapid"
                           else 352800 if name == "dead-e1-e2-groove.wav"
                           else 429975),
                "sha256": digest(f"{session_id}-{name}"),
            }
            if spec["kind"] == "rapid":
                take["run_bpms_in_order"] = [120, 180, 240]
            takes.append(take)
        value = {
            "schema": "electry-mute-capture/v1",
            "session": {
                "split": split,
                "session_id": session_id,
                "player_id": player_id,
            },
            "instrument": {"guitar_id": guitar_id},
            "takes": takes,
        }
        path = root / f"{session_id}-capture.json"
        path.write_text(json.dumps(value), encoding="utf-8")
        capture_takes[session_id] = {take["file"]: take["sha256"] for take in takes}
        normalized_sources[session_id] = {
            "session_id": session_id,
            "take_hashes": capture_takes[session_id],
            "take_metadata": {
                take["file"]: {"run_bpms_in_order": take.get("run_bpms_in_order")}
                for take in takes
            },
        }
        return {
            "session_id": session_id,
            "player_id": player_id,
            "guitar_id": guitar_id,
            "capture_manifest": {
                "path": path.name,
                "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            },
        }

    source_records = {
        "session-a": capture(
            "session-a", "player-a", "guitar-a", "holdout", all_take_files),
        "session-b": capture(
            "session-b", "player-b", "guitar-b", "holdout", all_take_files),
        "train-a": capture(
            "train-a", "train-player-a", "train-guitar-a", "train",
            all_take_files),
        "train-b": capture(
            "train-b", "train-player-b", "train-guitar-b", "train",
            all_take_files),
        "train-c": capture(
            "train-c", "train-player-c", "train-guitar-c", "train",
            all_take_files),
    }
    comparison["schema"] = "electry-blind-comparison/v1"
    comparison["status"] = "frozen"
    comparison["study_id"] = study_id
    comparison["presentation_seed"] = presentation_seed
    comparison["participant_count"] = 30
    comparison["source_cohort"] = {
        "holdout_cluster_count": 2,
        "holdout_clusters": [
            {
                "cluster_id": "holdout-a",
                "player_ids": ["player-a"],
                "guitar_ids": ["guitar-a"],
                "sessions": [source_records["session-a"]],
            },
            {
                "cluster_id": "holdout-b",
                "player_ids": ["player-b"],
                "guitar_ids": ["guitar-b"],
                "sessions": [source_records["session-b"]],
            },
        ],
        "engineering_train_clusters": [
            {
                "cluster_id": "train-cluster-a",
                "player_ids": ["train-player-a"],
                "guitar_ids": ["train-guitar-a"],
                "sessions": [source_records["train-a"]],
            },
            {
                "cluster_id": "train-cluster-b",
                "player_ids": ["train-player-b"],
                "guitar_ids": ["train-guitar-b"],
                "sessions": [source_records["train-b"]],
            },
            {
                "cluster_id": "train-cluster-c",
                "player_ids": ["train-player-c"],
                "guitar_ids": ["train-guitar-c"],
                "sessions": [source_records["train-c"]],
            },
        ],
        "practice_train_session_ids": ["train-a", "train-b"],
    }
    practice_sources = (("train-a", "e1-palm-middle.wav", "palm_single_e1"),
                        ("train-b", "e1-dead.wav", "dead_single_e1"))
    event_descriptors = {}

    def event_descriptor(event_id):
        if event_id not in event_descriptors:
            path = root / f"{event_id}.event.json"
            path.write_text(json.dumps({"event_id": event_id}), encoding="utf-8")
            event_descriptors[event_id] = {
                "path": path.name,
                "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            }
        return event_descriptors[event_id]

    comparison["practice"] = []
    for index, (pair_value, source_info) in enumerate(zip(practice, practice_sources), 1):
        session_id, take_file, content = source_info
        pair_id = f"practice-{index}"
        unit_candidates = [
            {
                "source_session_id": session_id,
                "capture_take_file": take_file,
                "capture_take_sha256": capture_takes[session_id][take_file],
                "selection_unit": {
                    "kind": "slot", "index": slot,
                    "stroke": "down" if slot % 2 else "up"},
            }
            for slot in range(1, 13)
        ]
        selected_unit = prepare._rank_candidates(
            selection_seed, f"draw:{pair_id}", unit_candidates)[0]["selection_unit"]
        provenance = {
            "capture_take_file": take_file,
            "capture_take_sha256": capture_takes[session_id][take_file],
            "selection_unit": selected_unit,
            "electry_event_or_score": event_descriptor(f"practice-{index}-event"),
        }
        comparison["practice"].append({
            "id": pair_id,
            "content": content,
            "processing": "dry",
            "frames": 30870,
            "source_session_id": session_id,
            "physical_sha256": pair_value["physical"]["sha256"],
            "electry_sha256": pair_value["electry"]["sha256"],
            "provenance": provenance,
            "qc": _qc(30870, 1),
        })

    cluster_assignment = prepare._cluster_assignment_candidates(
        selection_seed, ["holdout-a", "holdout-b"])[0]
    cluster_by_cell = {
        cell: group["source_cluster_id"]
        for group in cluster_assignment["groups"]
        for cell in group["cells"]
    }
    session_by_cluster = {"holdout-a": "session-a", "holdout-b": "session-b"}
    source_by_cell = {
        cell: session_by_cluster[cluster_by_cell[cell]] for cell in range(1, 11)
    }
    direction_assignment = prepare._single_direction_candidates(selection_seed)[0]
    down_cells = set(direction_assignment["down_cells"])
    take_by_content = {
        "palm_single_e1": "e1-palm-middle.wav",
        "palm_single_e2": "e2-palm-middle.wav",
        "dead_single_e1": "e1-dead.wav",
        "dead_single_e2": "e2-dead.wav",
        "palm_rapid_e1": "e1-palm-middle-rapid.wav",
        "dead_e1_e2_groove": "dead-e1-e2-groove.wav",
        "palm_open_e1_e2_groove": "palm-open-e1-e2-groove.wav",
    }
    comparison["cells"] = []
    for cell, pair_value in enumerate(cells, 1):
        content, processing, kind, hit_count = prepare.CELL_SEMANTICS[cell]
        session_id = source_by_cell[cell]
        take_file = take_by_content[content]
        cluster_id = cluster_by_cell[cell]
        phrase_root = {6: 5, 8: 7, 10: 9}.get(cell, cell)
        if kind == "slot":
            stroke = "down" if cell in down_cells else "up"
            indices = range(1 if stroke == "down" else 2, 13, 2)
        else:
            stroke = "down_first"
            indices = [2] if cell in (5, 6) else [1, 2, 3]
        candidate_values = [
            {
                "source_cluster_id": cluster_id,
                "source_session_id": session_id,
                "capture_take_file": take_file,
                "capture_take_sha256": capture_takes[session_id][take_file],
                "selection_unit": {"kind": kind, "index": unit_index,
                                   "stroke": stroke},
            }
            for unit_index in indices
        ]
        selected_candidate = prepare._rank_candidates(
            selection_seed, f"draw:{phrase_root}",
            candidate_values)[0]
        unit_index = selected_candidate["selection_unit"]["index"]
        provenance = {
            "capture_take_file": take_file,
            "capture_take_sha256": capture_takes[session_id][take_file],
            "selection_unit": {"kind": kind, "index": unit_index, "stroke": stroke},
            "electry_event_or_score": event_descriptor(f"cell-{phrase_root}-event"),
        }
        comparison["cells"].append({
            "id": cell,
            "content": content,
            "processing": processing,
            "frames": prepare.CORE_FRAMES[cell],
            "source_cluster_id": cluster_id,
            "source_session_id": session_id,
            "physical_sha256": pair_value["physical"]["sha256"],
            "electry_sha256": pair_value["electry"]["sha256"],
            "provenance": provenance,
            "qc": _qc(prepare.CORE_FRAMES[cell], hit_count),
        })

    freezes = comparison["freezes"]
    freezes["selection"]["seed"] = selection_seed
    for name in ("selection", "render", "chain", "analysis"):
        freezes[name]["implementation_sha256"] = digest(name + "-implementation")
        freezes[name]["settings_sha256"] = canonical_sha256(freezes[name]["settings"])
    freezes["selection"]["implementation_sha256"] = hashlib.sha256(
        (ROOT / "Tools" / "PrepareBlindListening.py").read_bytes()).hexdigest()
    freezes["chain"]["preset_sha256"] = digest("chain-preset")
    freezes["chain"]["assets"] = [{"id": "cab-ir", "sha256": digest("cab-ir")}]
    freezes["analysis"]["listener_scorer_sha256"] = hashlib.sha256(
        (ROOT / "Tools" / "ScoreBlindListening.py").read_bytes()).hexdigest()

    engineering = comparison["engineering_freeze"]
    for field in ("baseline_build_sha256", "candidate_build_sha256", "analyzer_sha256",
                  "evaluator_sha256", "event_generator_sha256"):
        engineering[field] = digest(field)
    engineering["endpoints"] = [
        {
            "id": endpoint_id,
            "orientation": "lower_is_better",
            "metric": metric,
            "aggregation": aggregation,
            "no_regression_margin": {
                "method": prepare.ENGINEERING_MARGIN_METHODS[endpoint_id],
                "detector_analysis_quantization": 0.01,
                "train_repeatability_p90": (
                    0.0 if endpoint_id.startswith("rapid_") else 0.02),
                "value": 0.01 if endpoint_id.startswith("rapid_") else 0.02,
                "formula": (
                    "detector_analysis_quantization"
                    if endpoint_id.startswith("rapid_")
                    else "max(detector_analysis_quantization,train_repeatability_p90)"),
            },
        }
        for endpoint_id, (metric, aggregation) in prepare.ENGINEERING_ENDPOINTS.items()
    ]
    engineering["endpoint_weights"] = {
        endpoint_id: 1.0 if index == 0 else 0.0
        for index, endpoint_id in enumerate(prepare.ENGINEERING_ENDPOINTS)
    }
    engineering["engineering_contract_sha256"] = canonical_sha256({
        key: value for key, value in engineering.items()
        if key not in {"derivation_receipt", "engineering_contract_sha256"}
    })
    samples_by_endpoint = {endpoint["id"]: [] for endpoint in engineering["endpoints"]}
    train_cluster_results = []
    for letter in ("a", "b", "c"):
        cluster_id = f"train-cluster-{letter}"
        session_id = f"train-{letter}"
        cluster_context = {
            "cluster_id": cluster_id,
            "sources": [normalized_sources[session_id]],
        }
        sessions = [{
            "session_id": session_id,
            "capture_manifest_sha256": source_records[
                session_id]["capture_manifest"]["sha256"],
        }]
        raw_endpoints = []
        for endpoint in engineering["endpoints"]:
            endpoint_id = endpoint["id"]
            method = endpoint["no_regression_margin"]["method"]
            eligible_units = prepare._engineering_eligible_units(
                cluster_context, endpoint_id, method)
            if method == "detector_analysis_quantization_only":
                samples = []
            else:
                samples = [
                    {
                        **unit,
                        "value": endpoint["no_regression_margin"][
                            "train_repeatability_p90"],
                    }
                    for unit in eligible_units
                ]
            raw_endpoints.append({
                "id": endpoint_id,
                "eligible_input_units": eligible_units,
                "excluded_input_units": [],
                "repeatability_samples": samples,
            })
            samples_by_endpoint[endpoint_id].extend(samples)
        train_analysis = {
            "schema": "electry-engineering-train-analysis/v1",
            "status": "frozen",
            "study_id": study_id,
            "cluster_id": cluster_id,
            "sessions": sessions,
            "depth_mapping": engineering["depth_mapping"],
            "passed": True,
            "failure_flags": [],
            "exclusions": [],
            "endpoints": raw_endpoints,
        }
        train_analysis_path = root / f"{cluster_id}-analysis.json"
        train_analysis_path.write_text(json.dumps(train_analysis), encoding="utf-8")
        train_cluster_results.append({
            "cluster_id": cluster_id,
            "sessions": sessions,
            "analysis_result": {
                "path": train_analysis_path.name,
                "sha256": hashlib.sha256(train_analysis_path.read_bytes()).hexdigest(),
                "required_schema": "electry-engineering-train-analysis/v1",
                "required_status": "frozen",
            },
        })

    result_endpoints = []
    for endpoint in engineering["endpoints"]:
        margin = endpoint["no_regression_margin"]
        method = margin["method"]
        samples = samples_by_endpoint[endpoint["id"]]
        result_endpoints.append({
            "id": endpoint["id"],
            "method": method,
            "detector_analysis_quantization": margin[
                "detector_analysis_quantization"],
            "repeatability_input_summary": {
                "unit": ("none_no_within_session_same_tempo_repeat"
                         if method == "detector_analysis_quantization_only"
                         else "balanced_three_versus_three_halves"
                         if method
                         == "balanced_three_versus_three_isolated_repetitions"
                         else "complete_groove_runs"),
                "sample_count": len(samples),
                "cluster_ids": ([] if not samples else [
                    "train-cluster-a", "train-cluster-b", "train-cluster-c"]),
            },
            "train_repeatability_samples": samples,
            "train_repeatability_p90": margin["train_repeatability_p90"],
            "value": margin["value"],
            "formula": margin["formula"],
        })
    engineering_contract_sha256 = engineering["engineering_contract_sha256"]
    engineering_result = {
        "schema": "electry-engineering-derivation-result/v1",
        "status": "frozen",
        "study_id": study_id,
        "train_clusters": train_cluster_results,
        "artifacts": {
            field: engineering[field]
            for field in ("baseline_build_sha256", "candidate_build_sha256",
                          "analyzer_sha256", "evaluator_sha256",
                          "event_generator_sha256")
        },
        "depth_mapping": engineering["depth_mapping"],
        "repeatability_percentile": 0.90,
        "percentile_method": "r7_linear_interpolation",
        "candidate_contour_rmse_reduction_min":
            engineering["candidate_contour_rmse_reduction_min"],
        "engineering_contract_sha256": engineering_contract_sha256,
        "failure_rule": "missing_train_derivation_or_sample_fails",
        "exclusions": [],
        "endpoints": result_endpoints,
    }
    engineering_result_path = root / "engineering-derivation-result.json"
    engineering_result_path.write_text(
        json.dumps(engineering_result), encoding="utf-8")
    engineering_receipt = {
        "schema": "electry-engineering-derivation-receipt/v1",
        "status": "frozen",
        "study_id": study_id,
        "derivation_result": {
            "path": engineering_result_path.name,
            "sha256": hashlib.sha256(
                engineering_result_path.read_bytes()).hexdigest(),
            "required_schema": "electry-engineering-derivation-result/v1",
            "required_status": "frozen",
        },
    }
    engineering_receipt_path = root / "engineering-derivation-receipt.json"
    engineering_receipt_path.write_text(
        json.dumps(engineering_receipt), encoding="utf-8")
    engineering["derivation_receipt"] = {
        "path": engineering_receipt_path.name,
        "sha256": hashlib.sha256(
            engineering_receipt_path.read_bytes()).hexdigest(),
        "required_schema": "electry-engineering-derivation-receipt/v1",
        "required_status": "frozen",
    }

    def normalized_pair(pair_value):
        result = {
            "source_session_id": pair_value["source_session_id"],
            "provenance": {
                "capture_take_file": pair_value["provenance"]["capture_take_file"],
                "capture_take_sha256": pair_value["provenance"][
                    "capture_take_sha256"],
                "selection_unit": pair_value["provenance"]["selection_unit"],
            },
        }
        if "source_cluster_id" in pair_value:
            result["source_cluster_id"] = pair_value["source_cluster_id"]
        return result

    normalized_practice = {
        value["id"]: normalized_pair(value) for value in comparison["practice"]
    }
    normalized_cells = {
        value["id"]: normalized_pair(value) for value in comparison["cells"]
    }
    receipt = prepare._selection_receipt_value(
        study_id, selection_seed, normalized_practice, normalized_cells,
        normalized_sources, {"session-a": "holdout-a", "session-b": "holdout-b"})
    receipt_path = root / "selection-receipt.json"
    receipt_path.write_text(json.dumps(receipt), encoding="utf-8")
    freezes["selection"]["receipt"] = {
        "path": receipt_path.name,
        "sha256": hashlib.sha256(receipt_path.read_bytes()).hexdigest(),
        "required_schema": "electry-blind-selection-receipt/v1",
        "required_status": "frozen",
    }
    split_by_session = {
        "session-a": ("holdout", "holdout-a"),
        "session-b": ("holdout", "holdout-b"),
        "train-a": ("train", "train-cluster-a"),
        "train-b": ("train", "train-cluster-b"),
        "train-c": ("train", "train-cluster-c"),
    }
    selection_input = {
        "schema": "electry-blind-selection-input/v1",
        "study_id": study_id,
        "selection_seed": selection_seed,
        "sessions": [
            {
                "session_id": session_id,
                "split": split,
                "source_cluster_id": cluster_id,
                "takes": [
                    {
                        "file": take_file,
                        "sha256": take_hash,
                        "run_bpms_in_order": normalized_sources[session_id][
                            "take_metadata"][take_file]["run_bpms_in_order"],
                    }
                    for take_file, take_hash in normalized_sources[session_id][
                        "take_hashes"].items()
                ],
            }
            for session_id, (split, cluster_id) in split_by_session.items()
        ],
        "practice": [
            {
                "id": pair["id"],
                "source_session_id": pair["source_session_id"],
                "capture_take_file": pair["provenance"]["capture_take_file"],
                "selection_kind": pair["provenance"]["selection_unit"]["kind"],
            }
            for pair in comparison["practice"]
        ],
        "cells": [
            {
                "id": cell,
                "capture_take_file": comparison["cells"][cell - 1]["provenance"][
                    "capture_take_file"],
            }
            for cell in (1, 2, 3, 4, 5, 7, 9)
        ],
    }
    selection_input_path = root / "selection-input.json"
    selection_input_path.write_text(json.dumps(selection_input), encoding="utf-8")

    artifact_hashes = {
        freezes[name]["implementation_sha256"]
        for name in ("selection", "render", "chain", "analysis")
    }
    artifact_hashes.update({
        freezes["chain"]["preset_sha256"],
        freezes["analysis"]["listener_scorer_sha256"],
        *[asset["sha256"] for asset in freezes["chain"]["assets"]],
        *[engineering[field] for field in (
            "baseline_build_sha256", "candidate_build_sha256", "analyzer_sha256",
            "evaluator_sha256", "event_generator_sha256")],
        *[cluster["analysis_result"]["sha256"]
          for cluster in train_cluster_results],
    })
    registry = {
        "schema": "electry-artifact-registry/v1",
        "status": "sealed",
        "artifacts": [
            {"id": f"artifact-{index:02d}", "sha256": artifact_hash}
            for index, artifact_hash in enumerate(sorted(artifact_hashes), 1)
        ],
    }
    registry_path = root / "artifact-registry.json"
    registry_path.write_text(json.dumps(registry), encoding="utf-8")
    comparison["artifact_registry"] = {
        "path": registry_path.name,
        "sha256": hashlib.sha256(registry_path.read_bytes()).hexdigest(),
        "required_schema": "electry-artifact-registry/v1",
        "required_status": "sealed",
    }

    comparison_path = root / "comparison.json"
    comparison_path.write_text(json.dumps(comparison), encoding="utf-8")
    study = {
        "schema": "electry-blind-study/v1",
        "status": "frozen_ready_to_pack",
        "study_id": study_id,
        "presentation_seed": presentation_seed,
        "participant_count": 30,
        "comparison_manifest": {
            "path": comparison_path.name,
            "sha256": hashlib.sha256(comparison_path.read_bytes()).hexdigest(),
            "required_schema": "electry-blind-comparison/v1",
            "required_status": "frozen",
        },
        "practice": practice,
        "cells": cells,
    }
    study_path = root / "study.json"
    study_path.write_text(json.dumps(study), encoding="utf-8")
    return {
        "cells": cells,
        "comparison": comparison,
        "comparison_path": comparison_path,
        "manifest": study,
        "manifest_path": study_path,
        "practice": practice,
        "source_records": source_records,
        "selection_input_path": selection_input_path,
        "selection_receipt_path": receipt_path,
    }
