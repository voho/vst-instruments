#!/usr/bin/env python3
"""Fit Electry's lowest-string stiffness from frozen exact-eight measurements.

This is a calibration gate, not a spectral tracker.  A frozen manifest binds
each supplied H1--H12 frequency measurement to an upstream-original WAVE by SHA-256,
records the analyzer that produced those measurements, and carries per-asset
commercial-calibration rights evidence plus a named human review.  This tool
then performs the small, deterministic fit that belongs in the repository:

    f_n = n f_1 sqrt((1 + B n^2) / (1 + B)).

Every take first gets one fixed, jointly fitted (f1, B_take) observation from
H1--H12.  With those f1 values frozen, only H2--H12 enter the global dispersion
objective and reported errors.  Baseline and candidate therefore use exactly
the same take tuning.  TRAIN is immutable: even frets 0--12 of the captured
F#1 lowest string.  Odd frets are HOLDOUT.
Captured fret r maps to fret r + 2 of Electry's Drop-E lowest string, so
``B_r = B_open * 2^((r + 2) / 6)``.  HOLDOUT never enters the coefficient fit.

The output reports the candidate open-string B, its equivalent empirical
``bendingCoreScale``, and baseline/candidate TRAIN and HOLDOUT error metrics.
It never edits source or audio.  Promotion still needs independent capture
replication; this one-string split is an internal gate, not market-wide proof.
Hash binding detects later substitution; it does not independently prove that
the supplied frequencies were extracted from those WAVE bytes.  That evidence
belongs to the frozen analyzer and its separately reviewed receipt. Keeping
``f1`` fixed is an offline scoring constraint; production allpass tuning still
needs a rendered regression before any coefficient can ship.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import re
import statistics
import sys
import tempfile
import wave
from datetime import datetime
from pathlib import Path
from urllib.parse import urlsplit


SCHEMA = "electry-dispersion-fit/v1"
STATUS = "frozen"
SCOPE = "lowest_string_dispersion_calibration"
REPRESENTATION = "upstream_original"
MEASURED_PARTIALS = tuple(range(1, 13))
SCORED_PARTIALS = tuple(range(2, 13))
CAPTURE_FRETS = tuple(range(13))
CAPTURE_OPEN_MIDI = 30  # F#1
MODEL_OPEN_MIDI = 28    # Drop-E E1
MODEL_FRET_OFFSET = CAPTURE_OPEN_MIDI - MODEL_OPEN_MIDI
BASELINE_OPEN_B = 0.00016228882056907437
BASELINE_BENDING_CORE_SCALE = 0.22
MAXIMUM_LIVE_B = 0.003
MAXIMUM_OPEN_B = MAXIMUM_LIVE_B / 2.0 ** ((12 + MODEL_FRET_OFFSET) / 6.0)
SHA256 = re.compile(r"[0-9a-f]{64}\Z")
IDENTIFIER = re.compile(r"[A-Za-z0-9][A-Za-z0-9._-]{0,79}\Z")
ZERO_SHA256 = "0" * 64
RIGHTS_KINDS = {
    "per_asset_cc0_record",
    "written_rightsholder_confirmation",
}


class Invalid(ValueError):
    pass


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise Invalid(message)


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        _require(key not in result, f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _exact_keys(value, expected, label: str) -> dict:
    _require(isinstance(value, dict), f"{label} must be an object")
    _require(set(value) == set(expected), f"{label} keys differ")
    return value


def _identifier(value, label: str) -> str:
    _require(isinstance(value, str) and IDENTIFIER.fullmatch(value) is not None,
             f"{label} is invalid")
    return value


def _https(value, label: str) -> str:
    try:
        parts = urlsplit(value) if isinstance(value, str) else None
        port = parts.port if parts is not None else None
    except ValueError as exc:
        raise Invalid(f"{label} is not a valid HTTPS URL") from exc
    del port
    valid = (parts is not None and parts.scheme == "https"
             and parts.hostname is not None
             and parts.username is None and parts.password is None)
    if isinstance(value, str) and any(
            character.isspace() or ord(character) < 32
            or ord(character) == 127 for character in value):
        valid = False
    _require(valid, f"{label} must be an HTTPS URL without credentials")
    return value


def _utc(value, label: str) -> str:
    _require(isinstance(value, str), f"{label} must be a UTC timestamp")
    try:
        parsed = datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ")
    except ValueError as exc:
        raise Invalid(f"{label} must be YYYY-MM-DDTHH:MM:SSZ") from exc
    _require(parsed.strftime("%Y-%m-%dT%H:%M:%SZ") == value,
             f"{label} must be YYYY-MM-DDTHH:MM:SSZ")
    return value


def _finite_number(value, label: str) -> float:
    _require(type(value) in (int, float) and math.isfinite(value),
             f"{label} must be a finite number")
    return float(value)


def _checked_hash(value, label: str) -> str:
    _require(isinstance(value, str) and SHA256.fullmatch(value) is not None,
             f"{label} must be a lowercase SHA-256")
    _require(value != ZERO_SHA256, f"{label} is still a zero placeholder")
    return value


def _read_json(path: Path, label: str) -> tuple[dict, bytes, str]:
    try:
        data = path.read_bytes()
    except OSError as exc:
        raise Invalid(f"{label}: cannot read {path}: {exc}") from exc
    try:
        value = json.loads(
            data.decode("utf-8"),
            parse_constant=lambda item: (_ for _ in ()).throw(
                Invalid(f"non-finite JSON value: {item}")),
            object_pairs_hook=_unique_object,
        )
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise Invalid(f"{label}: {exc}") from exc
    return value, data, hashlib.sha256(data).hexdigest()


def _resolve_asset(manifest_path: Path, value, label: str) -> Path:
    _require(isinstance(value, str) and value, f"{label} path is invalid")
    relative = Path(value)
    _require(not relative.is_absolute() and ".." not in relative.parts,
             f"{label} must stay inside the manifest directory")
    try:
        root = manifest_path.parent.resolve(strict=True)
        path = (root / relative).resolve(strict=True)
        path.relative_to(root)
    except (OSError, RuntimeError, ValueError) as exc:
        raise Invalid(f"{label} is missing or outside the manifest directory") from exc
    _require(path.is_file(), f"{label} is not a regular file")
    return path


def _file_hash(path: Path, label: str) -> str:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(block)
    except OSError as exc:
        raise Invalid(f"{label}: cannot read {path}: {exc}") from exc
    return digest.hexdigest()


def _wave_sample_rate(path: Path, label: str) -> int:
    try:
        with wave.open(str(path), "rb") as source:
            channels = source.getnchannels()
            sample_width = source.getsampwidth()
            sample_rate = source.getframerate()
            frames = source.getnframes()
            compression = source.getcomptype()
    except (OSError, EOFError, wave.Error) as exc:
        raise Invalid(f"{label} must be an upstream-original PCM WAVE: {exc}") from exc
    _require(compression == "NONE", f"{label} WAVE must be uncompressed PCM")
    _require(1 <= channels <= 2, f"{label} WAVE must have one or two channels")
    _require(sample_width in (2, 3, 4),
             f"{label} WAVE must be 16-, 24-, or 32-bit PCM")
    _require(8000 <= sample_rate <= 384000 and frames > 0,
             f"{label} WAVE rate or frame count is outside the fit contract")
    return sample_rate


def _midi_hz(note: int) -> float:
    return 440.0 * 2.0 ** ((note - 69) / 12.0)


def _role_for_fret(fret: int) -> str:
    return "TRAIN" if fret % 2 == 0 else "HOLDOUT"


def _load_manifest(path: Path) -> tuple[dict, str, list[dict], set[Path]]:
    path = path.resolve()
    manifest, _, manifest_hash = _read_json(path, "manifest")
    manifest = _exact_keys(
        manifest,
        {"schema", "status", "scientific_scope", "source", "capture",
         "model", "measurement", "rights_review", "assets"},
        "manifest",
    )
    _require(manifest["schema"] == SCHEMA, "manifest schema is unsupported")
    _require(manifest["status"] == STATUS, "manifest is not frozen")
    _require(manifest["scientific_scope"] == SCOPE,
             "manifest scientific scope is unsupported")

    source = _exact_keys(
        manifest["source"],
        {"dataset_id", "canonical_url", "upstream_snapshot", "retrieved_utc"},
        "source",
    )
    _identifier(source["dataset_id"], "source.dataset_id")
    _https(source["canonical_url"], "source.canonical_url")
    _require(isinstance(source["upstream_snapshot"], str)
             and 1 <= len(source["upstream_snapshot"]) <= 256,
             "source.upstream_snapshot is invalid")
    _utc(source["retrieved_utc"], "source.retrieved_utc")

    capture = _exact_keys(
        manifest["capture"],
        {"instrument_string_count", "physical_string_number",
         "open_midi_note", "frets"},
        "capture",
    )
    _require(capture == {
        "instrument_string_count": 8,
        "physical_string_number": 8,
        "open_midi_note": CAPTURE_OPEN_MIDI,
        "frets": list(CAPTURE_FRETS),
    }, "capture must be lowest-string F#1 at frets 0..12 on an exact eight")

    model = _exact_keys(
        manifest["model"],
        {"physical_string_number", "open_midi_note", "fret_offset",
         "baseline_open_inharmonicity", "baseline_bending_core_scale"},
        "model",
    )
    _require(model["physical_string_number"] == 8
             and model["open_midi_note"] == MODEL_OPEN_MIDI
             and model["fret_offset"] == MODEL_FRET_OFFSET,
             "model must map F#1 captures to Drop-E string-8 fret +2")
    baseline_b = _finite_number(
        model["baseline_open_inharmonicity"],
        "model.baseline_open_inharmonicity")
    baseline_scale = _finite_number(
        model["baseline_bending_core_scale"],
        "model.baseline_bending_core_scale")
    _require(abs(baseline_b - BASELINE_OPEN_B) <= 1.0e-15
             and abs(baseline_scale - BASELINE_BENDING_CORE_SCALE) <= 1.0e-15,
             "model baseline differs from the frozen shipping fit")

    measurement = _exact_keys(
        manifest["measurement"],
        {"analyzer_id", "analyzer_sha256", "quantity", "partials"},
        "measurement",
    )
    _identifier(measurement["analyzer_id"], "measurement.analyzer_id")
    _checked_hash(measurement["analyzer_sha256"],
                  "measurement.analyzer_sha256")
    _require(measurement["quantity"] == "tracked_partial_frequency_hz"
             and measurement["partials"] == list(MEASURED_PARTIALS),
             "measurement must contain tracked H1..H12 frequencies")

    review = _exact_keys(
        manifest["rights_review"],
        {"reviewer_id", "reviewed_utc", "covers_all_listed_audio_assets",
         "commercial_model_calibration"},
        "rights_review",
    )
    _identifier(review["reviewer_id"], "rights_review.reviewer_id")
    _utc(review["reviewed_utc"], "rights_review.reviewed_utc")
    _require(review["covers_all_listed_audio_assets"] is True
             and review["commercial_model_calibration"] is True,
             "rights review must cover every asset and commercial calibration")

    assets = manifest["assets"]
    _require(isinstance(assets, list) and len(assets) == len(CAPTURE_FRETS),
             "assets must contain exactly one take for every fret 0..12")
    loaded = []
    frets = set()
    identifiers = set()
    paths = set()
    hashes = set()
    input_paths = {path, Path(__file__).resolve()}
    for index, raw_asset in enumerate(assets):
        label = f"assets[{index}]"
        asset = _exact_keys(
            raw_asset,
            {"id", "source_url", "file", "sha256",
             "download_representation", "capture_fret", "role",
             "nominal_midi_note", "rights", "partials"},
            label,
        )
        asset_id = _identifier(asset["id"], f"{label}.id")
        _require(asset_id not in identifiers, f"{label}.id is duplicated")
        identifiers.add(asset_id)
        _https(asset["source_url"], f"{label}.source_url")
        _require(asset["download_representation"] == REPRESENTATION,
                 f"{label} must use the upstream original, not a preview")

        fret = asset["capture_fret"]
        _require(type(fret) is int and fret in CAPTURE_FRETS,
                 f"{label}.capture_fret is outside 0..12")
        _require(fret not in frets, f"capture fret {fret} is duplicated")
        frets.add(fret)
        expected_role = _role_for_fret(fret)
        _require(asset["role"] == expected_role,
                 f"capture fret {fret} must have immutable role {expected_role}")
        _require(asset["nominal_midi_note"] == CAPTURE_OPEN_MIDI + fret,
                 f"capture fret {fret} has the wrong nominal MIDI note")

        audio_path = _resolve_asset(path, asset["file"], f"{label} audio")
        _require(audio_path not in paths, f"{label} audio path is duplicated")
        paths.add(audio_path)
        input_paths.add(audio_path)
        expected_hash = _checked_hash(asset["sha256"], f"{label}.sha256")
        _require(expected_hash not in hashes, f"{label} audio hash is duplicated")
        hashes.add(expected_hash)
        _require(_file_hash(audio_path, f"{label} audio") == expected_hash,
                 f"{label} audio SHA-256 mismatch")
        sample_rate = _wave_sample_rate(audio_path, f"{label} audio")

        rights = _exact_keys(
            asset["rights"],
            {"evidence_kind", "basis_id", "commercial_model_calibration",
             "evidence_text"},
            f"{label}.rights",
        )
        kind = rights["evidence_kind"]
        _require(kind in RIGHTS_KINDS,
                 f"{label} needs per-asset CC0 evidence or a written grant")
        if kind == "per_asset_cc0_record":
            _require(rights["basis_id"] == "CC0-1.0",
                     f"{label} CC0 evidence must name CC0-1.0")
        else:
            _identifier(rights["basis_id"], f"{label}.rights.basis_id")
        _require(rights["commercial_model_calibration"] is True,
                 f"{label} rights do not permit commercial model calibration")
        evidence = rights["evidence_text"]
        _require(isinstance(evidence, str)
                 and 20 <= len(evidence.strip()) <= 100000,
                 f"{label} rights evidence text is missing")

        partials = asset["partials"]
        _require(isinstance(partials, list)
                 and len(partials) == len(MEASURED_PARTIALS),
                 f"{label}.partials must contain H1..H12 exactly once")
        measured = []
        seen_partials = set()
        nominal = _midi_hz(asset["nominal_midi_note"])
        previous_frequency = 0.0
        for partial_index, raw_partial in enumerate(partials):
            partial_label = f"{label}.partials[{partial_index}]"
            partial = _exact_keys(raw_partial, {"index", "frequency_hz"},
                                  partial_label)
            harmonic = partial["index"]
            _require(type(harmonic) is int and harmonic in MEASURED_PARTIALS,
                     f"{partial_label}.index is outside H1..H12")
            _require(harmonic not in seen_partials,
                     f"{label} partial H{harmonic} is duplicated")
            seen_partials.add(harmonic)
            frequency = _finite_number(partial["frequency_hz"],
                                       f"{partial_label}.frequency_hz")
            ratio = frequency / (harmonic * nominal)
            _require(frequency > previous_frequency
                     and 0.94 <= ratio <= 1.25
                     and frequency < 0.49 * sample_rate,
                     f"{partial_label} frequency is implausible or above Nyquist")
            previous_frequency = frequency
            measured.append((harmonic, frequency))
        _require(seen_partials == set(MEASURED_PARTIALS),
                 f"{label}.partials are incomplete")
        loaded.append({
            "id": asset_id,
            "capture_fret": fret,
            "model_fret": fret + MODEL_FRET_OFFSET,
            "role": expected_role,
            "partials": measured,
        })

    _require(frets == set(CAPTURE_FRETS), "capture frets 0..12 are incomplete")
    loaded.sort(key=lambda item: item["capture_fret"])
    return manifest, manifest_hash, loaded, input_paths


def _phase_stretch(partial: int, inharmonicity: float) -> float:
    return 0.5 * (
        math.log1p(inharmonicity * partial * partial)
        - math.log1p(inharmonicity)
    )


def _fit_take_parameters(asset: dict) -> tuple[float, float]:
    # y = (f_n / n)^2 is affine in x = n^2 for the normalized law:
    # intercept = f1^2/(1+B), slope = f1^2*B/(1+B).
    points = [
        (float(harmonic * harmonic), (frequency / harmonic) ** 2)
        for harmonic, frequency in asset["partials"]
    ]
    mean_x = sum(point[0] for point in points) / len(points)
    mean_y = sum(point[1] for point in points) / len(points)
    denominator = sum((x - mean_x) ** 2 for x, _ in points)
    slope = sum((x - mean_x) * (y - mean_y) for x, y in points) / denominator
    intercept = mean_y - slope * mean_x
    _require(intercept > 0.0 and slope > 0.0,
             f"{asset['id']} has no positive joint (f1, B) fit")
    live_b = slope / intercept
    _require(1.0e-10 < live_b < MAXIMUM_LIVE_B * (1.0 - 1.0e-9),
             f"{asset['id']} joint B fit reached the supported boundary")
    return math.sqrt(intercept + slope), live_b


def _errors(asset: dict, open_b: float) -> list[tuple[int, float]]:
    live_b = open_b * 2.0 ** (asset["model_fret"] / 6.0)
    log_f1 = math.log(asset["fixed_fundamental_hz"])
    cents_scale = 1200.0 / math.log(2.0)
    return [
        (harmonic, cents_scale * (
            math.log(frequency / harmonic)
            - _phase_stretch(harmonic, live_b)
            - log_f1
        ))
        for harmonic, frequency in asset["partials"]
    ]


def _squared_error(assets: list[dict], open_b: float) -> float:
    total = 0.0
    for asset in assets:
        errors = _errors(asset, open_b)
        total += sum(error * error for harmonic, error in errors
                     if harmonic in SCORED_PARTIALS)
    return total


def _golden_minimum(objective, high: float) -> float:
    low = 0.0
    ratio = (math.sqrt(5.0) - 1.0) / 2.0
    left = high - ratio * (high - low)
    right = low + ratio * (high - low)
    left_error = objective(left)
    right_error = objective(right)
    for _ in range(96):
        if left_error <= right_error:
            high = right
            right = left
            right_error = left_error
            left = high - ratio * (high - low)
            left_error = objective(left)
        else:
            low = left
            left = right
            left_error = right_error
            right = low + ratio * (high - low)
            right_error = objective(right)
    return 0.5 * (low + high)


def _fit_open_b(train: list[dict]) -> float:
    _require(train, "TRAIN is empty")
    # Fixed take tuning plus H2--H12 TRAIN-only scoring prevents B from trading
    # against tuning and leaves HOLDOUT no route into the coefficient.
    candidate = _golden_minimum(
        lambda open_b: _squared_error(train, open_b),
        MAXIMUM_OPEN_B,
    )
    _require(1.0e-10 < candidate < MAXIMUM_OPEN_B * (1.0 - 1.0e-9),
             "dispersion optimum landed on the supported B boundary")
    return candidate


def _percentile_nearest_rank(values: list[float], percentile: float) -> float:
    ordered = sorted(values)
    index = max(0, math.ceil(percentile * len(ordered)) - 1)
    return ordered[index]


def _median_reduction(baseline: float, candidate: float) -> float:
    if baseline <= 1.0e-12:
        return 0.0 if candidate <= 1.0e-12 else -1.0
    return 1.0 - candidate / baseline


def _metrics(assets: list[dict], open_b: float) -> tuple[dict, list[dict]]:
    absolute_errors = []
    per_asset = []
    for asset in assets:
        errors = _errors(asset, open_b)
        absolute = [abs(error) for harmonic, error in errors
                    if harmonic in SCORED_PARTIALS]
        absolute_errors.extend(absolute)
        per_asset.append({
            "id": asset["id"],
            "capture_fret": asset["capture_fret"],
            "model_fret": asset["model_fret"],
            "role": asset["role"],
            "median_absolute_cents": statistics.median(absolute),
            "maximum_absolute_cents": max(absolute),
        })
    _require(absolute_errors, "metric cohort is empty")
    return ({
        "partial_count": len(absolute_errors),
        "median_absolute_cents": statistics.median(absolute_errors),
        "p95_absolute_cents": _percentile_nearest_rank(
            absolute_errors, 0.95),
        "maximum_absolute_cents": max(absolute_errors),
        "rms_cents": math.sqrt(sum(value * value for value in absolute_errors)
                               / len(absolute_errors)),
    }, per_asset)


def fit(path: Path) -> dict:
    _, manifest_hash, assets, _ = _load_manifest(path)
    for asset in assets:
        asset["fixed_fundamental_hz"], asset["fitted_take_inharmonicity"] = (
            _fit_take_parameters(asset)
        )
    train = [asset for asset in assets if asset["role"] == "TRAIN"]
    holdout = [asset for asset in assets if asset["role"] == "HOLDOUT"]
    candidate_b = _fit_open_b(train)
    candidate_scale = BASELINE_BENDING_CORE_SCALE * (
        candidate_b / BASELINE_OPEN_B) ** 0.25

    roles = {}
    per_asset = []
    for role, cohort in (("TRAIN", train), ("HOLDOUT", holdout)):
        baseline, baseline_assets = _metrics(cohort, BASELINE_OPEN_B)
        candidate, candidate_assets = _metrics(cohort, candidate_b)
        median_reduction = _median_reduction(
            baseline["median_absolute_cents"],
            candidate["median_absolute_cents"],
        )
        roles[role] = {
            "baseline": baseline,
            "candidate": candidate,
            "median_error_reduction_fraction": median_reduction,
        }
        candidate_by_id = {item["id"]: item for item in candidate_assets}
        for baseline_item in baseline_assets:
            item = dict(baseline_item)
            candidate_item = candidate_by_id[item["id"]]
            source_asset = next(asset for asset in cohort
                                if asset["id"] == item["id"])
            item["fixed_fundamental_hz"] = source_asset[
                "fixed_fundamental_hz"]
            item["fitted_take_inharmonicity"] = source_asset[
                "fitted_take_inharmonicity"]
            item["scoring_fundamental_change_cents"] = 0.0
            item["baseline_median_absolute_cents"] = item.pop(
                "median_absolute_cents")
            item["baseline_maximum_absolute_cents"] = item.pop(
                "maximum_absolute_cents")
            item["candidate_median_absolute_cents"] = candidate_item[
                "median_absolute_cents"]
            item["candidate_maximum_absolute_cents"] = candidate_item[
                "maximum_absolute_cents"]
            per_asset.append(item)

    holdout_baseline = roles["HOLDOUT"]["baseline"]
    holdout_candidate = roles["HOLDOUT"]["candidate"]
    checks = {
        "median_error_reduced_at_least_50_percent":
            roles["HOLDOUT"]["median_error_reduction_fraction"] >= 0.5,
        "p95_error_not_worse":
            holdout_candidate["p95_absolute_cents"]
                <= holdout_baseline["p95_absolute_cents"],
        "maximum_error_not_worse":
            holdout_candidate["maximum_absolute_cents"]
                <= holdout_baseline["maximum_absolute_cents"],
    }
    tool_hash = _file_hash(Path(__file__).resolve(), "fit tool")
    return {
        "schema": "electry-dispersion-fit-result/v1",
        "status": "internal_gate_only",
        "manifest_sha256": manifest_hash,
        "fit_tool_sha256": tool_hash,
        "model_mapping": {
            "capture_open_midi_note": CAPTURE_OPEN_MIDI,
            "model_open_midi_note": MODEL_OPEN_MIDI,
            "model_fret_offset": MODEL_FRET_OFFSET,
            "fret_law": "B = B_open * 2^(model_fret/6)",
        },
        "baseline": {
            "open_inharmonicity": BASELINE_OPEN_B,
            "bending_core_scale": BASELINE_BENDING_CORE_SCALE,
        },
        "candidate": {
            "open_inharmonicity": candidate_b,
            "bending_core_scale": candidate_scale,
        },
        "roles": roles,
        "holdout_checks": checks,
        "holdout_gate_passed": all(checks.values()),
        "scoring_constraints": {
            "same_fixed_fundamental_for_baseline_and_candidate": True,
        },
        "per_asset": sorted(per_asset, key=lambda item: item["capture_fret"]),
        "claim_boundary": (
            "supplied partial observations are hash-bound but not extracted "
            "by this tool; one exact-eight lowest string is an internal split, "
            "and a separate installed-string-set holdout is required before "
            "shipping promotion; production allpass tuning remains untested"
        ),
    }


def _write_new(path: Path, data: bytes, input_paths: set[Path]) -> None:
    path = path.resolve()
    _require(path not in input_paths, "output path aliases a fit input")
    _require(not path.exists(), "output path already exists")
    temporary_path = None
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(
                mode="wb", dir=path.parent, prefix=f".{path.name}.",
                suffix=".tmp", delete=False) as stream:
            temporary_path = Path(stream.name)
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary_path, path)
    except OSError as exc:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)
        raise Invalid(f"cannot write output {path}: {exc}") from exc


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--output", type=Path,
                        help="write a new result file instead of stdout")
    args = parser.parse_args()
    try:
        result = fit(args.manifest)
        serialized = (json.dumps(result, indent=2, ensure_ascii=False,
                                 allow_nan=False) + "\n").encode("utf-8")
        if args.output is None:
            sys.stdout.buffer.write(serialized)
        else:
            _, _, _, input_paths = _load_manifest(args.manifest)
            _write_new(args.output, serialized, input_paths)
            print(f"PASS {args.output} SHA-256 "
                  f"{hashlib.sha256(serialized).hexdigest()}")
    except (Invalid, OSError, OverflowError, ValueError) as exc:
        print(f"invalid: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
