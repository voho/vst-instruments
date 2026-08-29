#!/usr/bin/env python3
"""Validate successor-only external exact-eight reference fixtures.

Archive each original WAVE beside one structured per-asset evidence record,
fill a receipt from ``--print-template``, and run this tool before analysis.
The evidence record binds dataset, snapshot, exact-eight identity, source URL,
audio hash, and either a per-asset CC0 record or written rightsholder grant.
A repository-root license is deliberately insufficient.

The receipt admits assets only to future v4 descriptive comparisons. It does
not alter a frozen cohort, authorize a fit, or itself decide legal rights; a
named human review remains mandatory.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import struct
import sys
from datetime import datetime
from pathlib import Path
from urllib.parse import urlsplit


SCHEMA = "electry-reference-fixture/v1"
EVIDENCE_SCHEMA = "electry-reference-asset-evidence/v1"
STATUS = "frozen"
TARGET_PROTOCOL = "electry-evaluation/v4"
SCIENTIFIC_SCOPE = "descriptive_comparison_only_no_fit_or_promotion"
REPRESENTATION = "upstream_original"
RIGHTS_KINDS = {
    "per_asset_cc0_record",
    "written_rightsholder_confirmation",
}
# The validator binds the reviewed evidence and permitted uses; it cannot infer
# that the reviewer owns every relevant right. CC0's scope is recorded at:
# https://creativecommons.org/publicdomain/zero/1.0/
IDENTIFIER = re.compile(r"[A-Za-z0-9][A-Za-z0-9._-]{0,79}\Z")
SHA256 = re.compile(r"[0-9a-f]{64}\Z")
ZERO_SHA256 = "0" * 64

RECEIPT_TEMPLATE = {
    "schema": SCHEMA,
    "status": STATUS,
    "fixture_id": "REPLACE",
    "target_protocol": TARGET_PROTOCOL,
    "scientific_scope": SCIENTIFIC_SCOPE,
    "source": {
        "dataset_id": "REPLACE",
        "canonical_url": "REPLACE",
        "upstream_snapshot": "REPLACE",
        "retrieved_utc": "REPLACE",
        "instrument_string_count": 8,
    },
    "rights_review": {
        "reviewer_id": "REPLACE",
        "reviewed_utc": "REPLACE",
        "covers_all_listed_audio_assets": True,
        "commercial_model_calibration": True,
        "private_competitive_evaluation": True,
    },
    "assets": [{
        "upstream_asset_id": "REPLACE",
        "source_url": "REPLACE",
        "download_representation": REPRESENTATION,
        "file": "REPLACE",
        "sha256": ZERO_SHA256,
        "evidence_record_file": "REPLACE",
        "evidence_record_sha256": ZERO_SHA256,
        "rights_evidence_kind": "per_asset_cc0_record",
        "rights_basis_id": "CC0-1.0",
    }],
}

EVIDENCE_TEMPLATE = {
    "schema": EVIDENCE_SCHEMA,
    "dataset_id": "REPLACE",
    "upstream_snapshot": "REPLACE",
    "instrument_string_count": 8,
    "upstream_asset_id": "REPLACE",
    "source_url": "REPLACE",
    "download_representation": REPRESENTATION,
    "audio_sha256": ZERO_SHA256,
    "rights_evidence_kind": "per_asset_cc0_record",
    "rights_basis_id": "CC0-1.0",
    "commercial_model_calibration": True,
    "private_competitive_evaluation": True,
    "evidence_text": "REPLACE",
}


class Invalid(ValueError):
    pass


def _unique_object(pairs):
    value = {}
    for key, item in pairs:
        if key in value:
            raise Invalid(f"duplicate JSON key: {key}")
        value[key] = item
    return value


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise Invalid(message)


def _exact_keys(value, expected, label: str) -> dict:
    _require(isinstance(value, dict), f"{label} must be an object")
    _require(set(value) == set(expected), f"{label} keys differ")
    return value


def _decode_json(data: bytes, label: str) -> dict:
    try:
        return json.loads(
            data.decode("utf-8"),
            parse_constant=lambda value: (_ for _ in ()).throw(
                Invalid(f"non-finite JSON value: {value}")),
            object_pairs_hook=_unique_object,
        )
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise Invalid(f"{label}: {exc}") from exc


def _read_snapshot(path: Path, label: str) -> tuple[bytes, str]:
    try:
        data = path.read_bytes()
    except OSError as exc:
        raise Invalid(f"{label}: cannot read {path}: {exc}") from exc
    return data, hashlib.sha256(data).hexdigest()


def _load_json_snapshot(path: Path, label: str) -> tuple[dict, str]:
    data, digest = _read_snapshot(path, label)
    return _decode_json(data, label), digest


def _scan_filled(value, label: str) -> None:
    if value is None:
        raise Invalid(f"{label} contains a null placeholder")
    if isinstance(value, float) and not math.isfinite(value):
        raise Invalid(f"{label} contains a non-finite number")
    if isinstance(value, str):
        if "REPLACE" in value:
            raise Invalid(f"{label} contains a REPLACE placeholder")
        if len(value) in (40, 64) and set(value) == {"0"}:
            raise Invalid(f"{label} contains a zero hash placeholder")
    elif isinstance(value, list):
        for index, item in enumerate(value):
            _scan_filled(item, f"{label}[{index}]")
    elif isinstance(value, dict):
        for key, item in value.items():
            _scan_filled(item, f"{label}.{key}")


def _identifier(value, label: str) -> str:
    _require(isinstance(value, str) and IDENTIFIER.fullmatch(value) is not None,
             f"{label} is invalid")
    return value


def _https(value, label: str) -> str:
    try:
        parts = urlsplit(value) if isinstance(value, str) else None
        port = parts.port if parts is not None else None
        valid = (parts is not None and parts.scheme == "https"
                 and parts.hostname is not None
                 and parts.username is None and parts.password is None)
        if isinstance(value, str) and any(
                character.isspace() or ord(character) < 32 or ord(character) == 127
                for character in value):
            valid = False
    except ValueError as exc:
        raise Invalid(f"{label} is not a valid HTTPS URL") from exc
    del port  # Access itself validates malformed and out-of-range ports.
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


def _resolve_file(owner: Path, name, label: str) -> Path:
    _require(isinstance(name, str) and name, f"{label} path must be non-empty")
    relative = Path(name)
    _require(not relative.is_absolute() and ".." not in relative.parts,
             f"{label} must stay inside the fixture directory")
    try:
        root = owner.parent.resolve(strict=True)
        path = (root / relative).resolve(strict=True)
        path.relative_to(root)
    except (OSError, RuntimeError, ValueError) as exc:
        raise Invalid(f"{label} is missing or outside the fixture directory") from exc
    _require(path.is_file(), f"{label} is not a regular file")
    return path


def _same_file(first: Path, second: Path, label: str) -> bool:
    try:
        return first.samefile(second)
    except OSError as exc:
        raise Invalid(f"{label}: cannot compare file identities") from exc


def _wave_info(data: bytes, label: str) -> dict:
    _require(len(data) >= 44 and data[:4] == b"RIFF" and data[8:12] == b"WAVE",
             f"{label} is not a RIFF/WAVE file")
    declared_size = struct.unpack_from("<I", data, 4)[0] + 8
    _require(declared_size == len(data), f"{label} RIFF size is inconsistent")

    position = 12
    format_chunk = None
    data_offset = None
    data_size = None
    while position < len(data):
        _require(position + 8 <= len(data), f"{label} has a truncated chunk header")
        chunk_id = data[position:position + 4]
        chunk_size = struct.unpack_from("<I", data, position + 4)[0]
        start = position + 8
        end = start + chunk_size
        _require(end <= len(data), f"{label} has a truncated chunk")
        if chunk_id == b"fmt ":
            _require(format_chunk is None, f"{label} has duplicate format chunks")
            format_chunk = data[start:end]
        elif chunk_id == b"data":
            _require(data_size is None, f"{label} has duplicate data chunks")
            data_offset = start
            data_size = chunk_size
        position = end + (chunk_size & 1)
        _require(position <= len(data), f"{label} has a missing RIFF pad byte")

    _require(position == len(data) and format_chunk is not None and data_size is not None,
             f"{label} needs exactly one format and one data chunk")
    _require(len(format_chunk) >= 16, f"{label} format chunk is truncated")
    encoding, channels, sample_rate, byte_rate, block_align, bits = struct.unpack_from(
        "<HHIIHH", format_chunk)
    if encoding in (1, 3):
        _require(len(format_chunk) == 16
                 or (len(format_chunk) == 18
                     and struct.unpack_from("<H", format_chunk, 16)[0] == 0),
                 f"{label} has an invalid PCM/float format chunk size")
        effective_encoding = encoding
    elif encoding == 0xfffe:
        _require(len(format_chunk) >= 40,
                 f"{label} has a short WAVE_FORMAT_EXTENSIBLE chunk")
        extension_size, valid_bits = struct.unpack_from("<HH", format_chunk, 16)
        _require(extension_size >= 22 and 18 + extension_size <= len(format_chunk)
                 and valid_bits == bits,
                 f"{label} has an invalid WAVE_FORMAT_EXTENSIBLE extension")
        subtype = format_chunk[24:40]
        pcm_guid = bytes.fromhex("0100000000001000800000aa00389b71")
        float_guid = bytes.fromhex("0300000000001000800000aa00389b71")
        _require(subtype in (pcm_guid, float_guid),
                 f"{label} has an unsupported WAVE_FORMAT_EXTENSIBLE subtype")
        effective_encoding = 1 if subtype == pcm_guid else 3
    else:
        raise Invalid(f"{label} must be PCM or IEEE-float WAVE")
    _require(1 <= channels <= 2 and 8000 <= sample_rate <= 384000,
             f"{label} channel count or sample rate is outside the reference contract")
    _require(bits in (16, 24, 32) and (effective_encoding != 3 or bits == 32),
             f"{label} sample encoding is outside the reference contract")
    expected_align = channels * (bits // 8)
    _require(block_align == expected_align and byte_rate == sample_rate * block_align,
             f"{label} byte rate or block alignment is inconsistent")
    _require(data_size > 0 and data_size % block_align == 0,
             f"{label} audio payload is empty or not frame-aligned")
    return {
        "encoding": "PCM" if effective_encoding == 1 else "IEEE_FLOAT",
        "channels": channels,
        "sample_rate_hz": sample_rate,
        "bits_per_sample": bits,
        "block_align": block_align,
        "data_offset": data_offset,
        "data_size": data_size,
        "frames": data_size // block_align,
    }


def _checked_audio(path: Path, expected, label: str) -> None:
    _require(isinstance(expected, str) and SHA256.fullmatch(expected) is not None,
             f"{label} SHA-256 must be lowercase hexadecimal")
    data, digest = _read_snapshot(path, label)
    _require(digest == expected, f"{label} SHA-256 mismatch")
    _wave_info(data, label)


def _checked_evidence(path: Path, expected, label: str) -> dict:
    _require(isinstance(expected, str) and SHA256.fullmatch(expected) is not None,
             f"{label} SHA-256 must be lowercase hexadecimal")
    data, digest = _read_snapshot(path, label)
    _require(digest == expected, f"{label} SHA-256 mismatch")
    return _decode_json(data, label)


def validate(path: Path) -> str:
    path = path.resolve()
    receipt, receipt_digest = _load_json_snapshot(path, "receipt")
    receipt = _exact_keys(
        receipt,
        {"schema", "status", "fixture_id", "target_protocol",
         "scientific_scope", "source", "rights_review", "assets"},
        "receipt",
    )
    _scan_filled(receipt, "receipt")
    _require(receipt["schema"] == SCHEMA, "receipt schema is not reference-fixture v1")
    _require(receipt["status"] == STATUS, "receipt is not frozen")
    _identifier(receipt["fixture_id"], "fixture_id")
    _require(receipt["target_protocol"] == TARGET_PROTOCOL,
             "target_protocol must be successor electry-evaluation/v4")
    _require(receipt["scientific_scope"] == SCIENTIFIC_SCOPE,
             "scientific scope must forbid fit or promotion")

    source = _exact_keys(
        receipt["source"],
        {"dataset_id", "canonical_url", "upstream_snapshot", "retrieved_utc",
         "instrument_string_count"},
        "source",
    )
    dataset_id = _identifier(source["dataset_id"], "source.dataset_id")
    _https(source["canonical_url"], "source.canonical_url")
    _require(isinstance(source["upstream_snapshot"], str)
             and 1 <= len(source["upstream_snapshot"]) <= 256,
             "source.upstream_snapshot is invalid")
    _utc(source["retrieved_utc"], "source.retrieved_utc")
    _require(type(source["instrument_string_count"]) is int
             and source["instrument_string_count"] == 8,
             "source must be an exact eight-string instrument")

    review = _exact_keys(
        receipt["rights_review"],
        {"reviewer_id", "reviewed_utc", "covers_all_listed_audio_assets",
         "commercial_model_calibration", "private_competitive_evaluation"},
        "rights_review",
    )
    _identifier(review["reviewer_id"], "rights_review.reviewer_id")
    _utc(review["reviewed_utc"], "rights_review.reviewed_utc")
    _require(review["covers_all_listed_audio_assets"] is True
             and review["commercial_model_calibration"] is True
             and review["private_competitive_evaluation"] is True,
             "rights review must cover every asset and both permitted uses")

    assets = receipt["assets"]
    _require(isinstance(assets, list) and assets, "assets must be a non-empty array")
    upstream_ids = set()
    local_files = set()
    audio_hashes = set()
    for index, raw_asset in enumerate(assets):
        label = f"assets[{index}]"
        asset = _exact_keys(
            raw_asset,
            {"upstream_asset_id", "source_url", "download_representation", "file",
             "sha256", "evidence_record_file", "evidence_record_sha256",
             "rights_evidence_kind", "rights_basis_id"},
            label,
        )
        upstream_id = _identifier(asset["upstream_asset_id"],
                                  f"{label}.upstream_asset_id")
        _require(upstream_id not in upstream_ids,
                 f"{label} upstream asset ID is duplicated")
        upstream_ids.add(upstream_id)
        source_url = _https(asset["source_url"], f"{label}.source_url")
        _require(asset["download_representation"] == REPRESENTATION,
                 f"{label} must be an upstream original, not a lossy preview")

        file_value = asset["file"]
        hash_value = asset["sha256"]
        _require(isinstance(file_value, str) and file_value,
                 f"{label} local audio path is invalid")
        _require(isinstance(hash_value, str) and SHA256.fullmatch(hash_value) is not None,
                 f"{label} audio SHA-256 is invalid")
        _require(file_value not in local_files,
                 f"{label} local audio path is duplicated")
        _require(hash_value not in audio_hashes,
                 f"{label} audio hash is duplicated")
        local_files.add(file_value)
        audio_hashes.add(hash_value)

        audio_path = _resolve_file(path, file_value, f"{label} audio")
        evidence_path = _resolve_file(
            path, asset["evidence_record_file"], f"{label} evidence record")
        _require(not _same_file(audio_path, evidence_path, label),
                 f"{label} audio cannot serve as its own evidence")
        _checked_audio(audio_path, hash_value, f"{label} audio")
        evidence = _checked_evidence(
            evidence_path, asset["evidence_record_sha256"],
            f"{label} evidence record")
        evidence = _exact_keys(
            evidence,
            {"schema", "dataset_id", "upstream_snapshot", "instrument_string_count",
             "upstream_asset_id", "source_url", "download_representation",
             "audio_sha256", "rights_evidence_kind", "rights_basis_id",
             "commercial_model_calibration", "private_competitive_evaluation",
             "evidence_text"},
            f"{label} evidence record",
        )
        _scan_filled(evidence, f"{label} evidence record")
        _require(evidence["schema"] == EVIDENCE_SCHEMA,
                 f"{label} evidence record schema is invalid")
        _require(evidence["dataset_id"] == dataset_id
                 and evidence["upstream_snapshot"] == source["upstream_snapshot"]
                 and type(evidence["instrument_string_count"]) is int
                 and evidence["instrument_string_count"] == 8
                 and evidence["upstream_asset_id"] == upstream_id
                 and evidence["source_url"] == source_url
                 and evidence["download_representation"] == REPRESENTATION
                 and evidence["audio_sha256"] == hash_value,
                 f"{label} evidence record is not cross-bound to source and audio")

        kind = asset["rights_evidence_kind"]
        _require(isinstance(kind, str) and kind in RIGHTS_KINDS,
                 f"{label} rights evidence must be per-asset CC0 evidence or "
                 "written rightsholder confirmation; repository-root licenses are insufficient")
        if kind == "per_asset_cc0_record":
            _require(asset["rights_basis_id"] == "CC0-1.0",
                     f"{label} per-asset CC0 evidence must name CC0-1.0")
        else:
            _identifier(asset["rights_basis_id"], f"{label}.rights_basis_id")
        _require(evidence["rights_evidence_kind"] == kind
                 and evidence["rights_basis_id"] == asset["rights_basis_id"]
                 and evidence["commercial_model_calibration"] is True
                 and evidence["private_competitive_evaluation"] is True,
                 f"{label} evidence record does not grant both reviewed uses")
        _require(isinstance(evidence["evidence_text"], str)
                 and 20 <= len(evidence["evidence_text"].strip()) <= 1_000_000,
                 f"{label} evidence text is missing")

    return receipt_digest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--print-template", choices=("receipt", "evidence"),
        help="print one fillable JSON template and exit")
    parser.add_argument("receipts", nargs="*", type=Path)
    args = parser.parse_args()
    if args.print_template:
        if args.receipts:
            parser.error("--print-template does not accept receipt paths")
        template = (RECEIPT_TEMPLATE if args.print_template == "receipt"
                    else EVIDENCE_TEMPLATE)
        print(json.dumps(template, indent=2, ensure_ascii=False))
        return 0
    if not args.receipts:
        parser.error("provide at least one receipt or --print-template")
    try:
        for receipt in args.receipts:
            digest = validate(receipt)
            print(f"PASS {receipt} SHA-256 {digest}")
    except Invalid as exc:
        print(f"invalid: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
