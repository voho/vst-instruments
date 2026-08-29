#!/usr/bin/env python3
"""Synthetic contract tests for Tools/FitDispersion.py."""

from __future__ import annotations

import copy
import hashlib
import importlib.util
import json
import math
import struct
import sys
import tempfile
import unittest
import wave
from pathlib import Path


sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "fit_dispersion", ROOT / "Tools" / "FitDispersion.py")
assert SPEC is not None and SPEC.loader is not None
FIT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FIT)

KNOWN_OPEN_B = 0.00022


def _midi_hz(note: int) -> float:
    return 440.0 * 2.0 ** ((note - 69) / 12.0)


def _partials(f1: float, live_b: float) -> list[dict]:
    return [
        {
            "index": harmonic,
            "frequency_hz": harmonic * f1 * math.sqrt(
                (1.0 + live_b * harmonic * harmonic) / (1.0 + live_b)),
        }
        for harmonic in range(1, 13)
    ]


def _phase_stretch(harmonic: int, inharmonicity: float) -> float:
    return 0.5 * (
        math.log1p(inharmonicity * harmonic * harmonic)
        - math.log1p(inharmonicity)
    )


def _take_fit_oracle(partials: list[tuple[int, float]]) -> tuple[float, float]:
    points = [
        (float(harmonic * harmonic), (frequency / harmonic) ** 2)
        for harmonic, frequency in partials
    ]
    mean_x = sum(x for x, _ in points) / len(points)
    mean_y = sum(y for _, y in points) / len(points)
    slope = sum((x - mean_x) * (y - mean_y) for x, y in points) / sum(
        (x - mean_x) ** 2 for x, _ in points)
    intercept = mean_y - slope * mean_x
    return math.sqrt(intercept + slope), slope / intercept


def _golden_minimum(objective, high: float) -> float:
    low = 0.0
    ratio = (math.sqrt(5.0) - 1.0) / 2.0
    left = high - ratio * (high - low)
    right = low + ratio * (high - low)
    left_error = objective(left)
    right_error = objective(right)
    for _ in range(96):
        if left_error <= right_error:
            high, right, right_error = right, left, left_error
            left = high - ratio * (high - low)
            left_error = objective(left)
        else:
            low, left, left_error = left, right, right_error
            right = low + ratio * (high - low)
            right_error = objective(right)
    return 0.5 * (low + high)


def _open_b_oracle(assets: list[dict], refit_fundamental: bool) -> float:
    cents_scale = 1200.0 / math.log(2.0)

    def objective(open_b: float) -> float:
        total = 0.0
        for asset in assets:
            live_b = open_b * 2.0 ** (asset["model_fret"] / 6.0)
            log_fundamentals = [
                math.log(frequency / harmonic)
                - _phase_stretch(harmonic, live_b)
                for harmonic, frequency in asset["partials"]
            ]
            log_f1 = (sum(log_fundamentals) / len(log_fundamentals)
                      if refit_fundamental
                      else math.log(asset["fixed_fundamental_hz"]))
            total += sum(
                (cents_scale * (value - log_f1)) ** 2
                for (harmonic, _), value in zip(
                    asset["partials"], log_fundamentals)
                if harmonic >= 2
            )
        return total

    return _golden_minimum(objective, FIT.MAXIMUM_OPEN_B)


class FitDispersionTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        audio = self.root / "originals"
        audio.mkdir()
        assets = []
        for fret in range(13):
            path = audio / f"fret-{fret:02d}.wav"
            with wave.open(str(path), "wb") as output:
                output.setnchannels(1)
                output.setsampwidth(2)
                output.setframerate(48000)
                output.writeframes(b"".join(
                    struct.pack("<h", ((frame * 97 + fret * 701) % 24000) - 12000)
                    for frame in range(96 + fret)
                ))
            f1 = _midi_hz(30 + fret) * (
                1.0 + 0.0025 * math.sin(0.73 * fret + 0.2))
            model_fret = fret + 2
            live_b = KNOWN_OPEN_B * 2.0 ** (model_fret / 6.0)
            assets.append({
                "id": f"cabled-fsharp-{fret:02d}",
                "source_url": f"https://example.test/source/{fret}",
                "file": str(path.relative_to(self.root)),
                "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "download_representation": "upstream_original",
                "capture_fret": fret,
                "role": "TRAIN" if fret % 2 == 0 else "HOLDOUT",
                "nominal_midi_note": 30 + fret,
                "rights": {
                    "evidence_kind": "per_asset_cc0_record",
                    "basis_id": "CC0-1.0",
                    "commercial_model_calibration": True,
                    "evidence_text": "Upstream asset page records this take as CC0.",
                },
                "partials": _partials(f1, live_b),
            })
        self.manifest = {
            "schema": "electry-dispersion-fit/v1",
            "status": "frozen",
            "scientific_scope": "lowest_string_dispersion_calibration",
            "source": {
                "dataset_id": "synthetic-exact-eight",
                "canonical_url": "https://example.test/source",
                "upstream_snapshot": "synthetic-v1",
                "retrieved_utc": "2026-08-29T18:00:00Z",
            },
            "capture": {
                "instrument_string_count": 8,
                "physical_string_number": 8,
                "open_midi_note": 30,
                "frets": list(range(13)),
            },
            "model": {
                "physical_string_number": 8,
                "open_midi_note": 28,
                "fret_offset": 2,
                "baseline_open_inharmonicity": FIT.BASELINE_OPEN_B,
                "baseline_bending_core_scale": 0.22,
            },
            "measurement": {
                "analyzer_id": "synthetic-partial-tracker-v1",
                "analyzer_sha256": "1" * 64,
                "quantity": "tracked_partial_frequency_hz",
                "partials": list(range(1, 13)),
            },
            "rights_review": {
                "reviewer_id": "synthetic-reviewer",
                "reviewed_utc": "2026-08-29T18:01:00Z",
                "covers_all_listed_audio_assets": True,
                "commercial_model_calibration": True,
            },
            "assets": assets,
        }
        self.path = self.root / "manifest.json"
        self._write()
        self.valid_manifest = copy.deepcopy(self.manifest)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def _write(self) -> None:
        self.path.write_text(json.dumps(self.manifest), encoding="utf-8")

    def _reset_manifest(self) -> None:
        self.manifest = copy.deepcopy(self.valid_manifest)
        self._write()

    def _invalid(self, expected: str) -> None:
        self._write()
        with self.assertRaisesRegex(FIT.Invalid, expected):
            FIT.fit(self.path)

    def test_recovers_known_b_with_detuned_fundamentals(self) -> None:
        result = FIT.fit(self.path)
        self.assertAlmostEqual(
            result["candidate"]["open_inharmonicity"], KNOWN_OPEN_B,
            delta=1.0e-12)
        self.assertAlmostEqual(
            result["candidate"]["bending_core_scale"],
            0.22 * (KNOWN_OPEN_B / FIT.BASELINE_OPEN_B) ** 0.25,
            delta=1.0e-12)
        self.assertEqual(result["model_mapping"]["model_fret_offset"], 2)
        self.assertEqual(result["per_asset"][0]["model_fret"], 2)
        self.assertEqual(result["roles"]["TRAIN"]["candidate"]["partial_count"], 77)
        self.assertEqual(result["roles"]["HOLDOUT"]["candidate"]["partial_count"], 66)
        self.assertIs(
            result["scoring_constraints"][
                "same_fixed_fundamental_for_baseline_and_candidate"],
            True)
        self.assertNotIn("fundamental_tuning_unchanged", result["holdout_checks"])
        for asset in result["per_asset"]:
            expected_f1 = _midi_hz(30 + asset["capture_fret"]) * (
                1.0 + 0.0025 * math.sin(0.73 * asset["capture_fret"] + 0.2))
            self.assertAlmostEqual(asset["fixed_fundamental_hz"], expected_f1,
                                   delta=1.0e-9)
            self.assertEqual(asset["scoring_fundamental_change_cents"], 0.0)
        self.assertIn("not extracted by this tool", result["claim_boundary"])
        self.assertIn("separate installed-string-set holdout",
                      result["claim_boundary"])
        self.assertIn("production allpass tuning remains untested",
                      result["claim_boundary"])

    def test_h1_is_required_for_take_fit_but_never_scored(self) -> None:
        _, _, assets, _ = FIT._load_manifest(self.path)
        original = assets[0]
        shifted = copy.deepcopy(original)
        shifted["partials"][0] = (
            1, shifted["partials"][0][1] * 1.002)
        original_f1, _ = FIT._fit_take_parameters(original)
        shifted_f1, shifted_b = FIT._fit_take_parameters(shifted)
        oracle_f1, oracle_b = _take_fit_oracle(shifted["partials"])
        self.assertAlmostEqual(shifted_f1, oracle_f1, delta=1.0e-12)
        self.assertAlmostEqual(shifted_b, oracle_b, delta=1.0e-15)
        self.assertNotAlmostEqual(
            shifted_f1, shifted["partials"][0][1], delta=1.0e-8)
        self.assertNotAlmostEqual(original_f1, shifted_f1, delta=1.0e-8)

        original["fixed_fundamental_hz"] = original_f1
        shifted["fixed_fundamental_hz"] = original_f1
        self.assertEqual(
            FIT._squared_error([original], KNOWN_OPEN_B),
            FIT._squared_error([shifted], KNOWN_OPEN_B))
        metrics, _ = FIT._metrics([original], KNOWN_OPEN_B)
        self.assertEqual(metrics["partial_count"], 11)

        del self.manifest["assets"][0]["partials"][0]
        self._invalid("H1..H12")

    def test_zero_baseline_cannot_claim_an_improvement(self) -> None:
        self.assertEqual(FIT._median_reduction(0.0, 0.0), 0.0)
        self.assertEqual(FIT._median_reduction(0.0, 1.0), -1.0)

    def test_global_fit_keeps_the_independently_fitted_fundamentals(self) -> None:
        for asset in self.manifest["assets"]:
            if asset["role"] == "TRAIN":
                asset["partials"][0]["frequency_hz"] *= 2.0 ** (20.0 / 1200.0)
        self._write()
        result = FIT.fit(self.path)
        _, _, assets, _ = FIT._load_manifest(self.path)
        for asset in assets:
            asset["fixed_fundamental_hz"], _ = _take_fit_oracle(
                asset["partials"])
        train = [asset for asset in assets if asset["role"] == "TRAIN"]
        fixed_candidate = _open_b_oracle(train, refit_fundamental=False)
        nuisance_candidate = _open_b_oracle(train, refit_fundamental=True)
        self.assertAlmostEqual(
            result["candidate"]["open_inharmonicity"], fixed_candidate,
            delta=1.0e-12)
        self.assertGreater(abs(fixed_candidate - nuisance_candidate), 1.0e-6)

    def test_holdout_cannot_influence_candidate(self) -> None:
        first = FIT.fit(self.path)
        for asset in self.manifest["assets"]:
            if asset["role"] == "HOLDOUT":
                fret = asset["capture_fret"]
                f1 = asset["partials"][0]["frequency_hz"]
                live_b = 0.00040 * 2.0 ** ((fret + 2) / 6.0)
                asset["partials"] = _partials(f1, live_b)
        self._write()
        second = FIT.fit(self.path)
        self.assertEqual(first["candidate"], second["candidate"])
        self.assertNotEqual(
            first["roles"]["HOLDOUT"]["candidate"],
            second["roles"]["HOLDOUT"]["candidate"])

    def test_rejects_previews_and_unreviewed_rights(self) -> None:
        self.manifest["assets"][0]["download_representation"] = "lossy_preview"
        self._invalid("not a preview")

        self._reset_manifest()
        self.manifest["assets"][0]["rights"]["commercial_model_calibration"] = False
        self._invalid("do not permit commercial model calibration")

        self._reset_manifest()
        del self.manifest["assets"][0]["rights"]["evidence_text"]
        self._invalid("rights keys differ")

        self._reset_manifest()
        self.manifest["rights_review"]["commercial_model_calibration"] = False
        self._invalid("rights review")

    def test_rejects_audio_hash_tampering_and_non_wave_bytes(self) -> None:
        audio = self.root / self.manifest["assets"][0]["file"]
        original = audio.read_bytes()
        audio.write_bytes(original + b"tampered")
        with self.assertRaisesRegex(FIT.Invalid, "SHA-256 mismatch"):
            FIT.fit(self.path)

        audio.write_bytes(original)
        audio.write_bytes(b"ID3 fake preview")
        self.manifest["assets"][0]["sha256"] = hashlib.sha256(
            audio.read_bytes()).hexdigest()
        self._invalid("PCM WAVE")

    def test_rejects_missing_duplicate_or_mutable_protocol_rows(self) -> None:
        self.manifest["assets"].pop()
        self._invalid("exactly one take")

        self._reset_manifest()
        self.manifest["assets"][1]["capture_fret"] = 0
        self._invalid("duplicated")

        self._reset_manifest()
        self.manifest["assets"][1]["role"] = "TRAIN"
        self._invalid("immutable role HOLDOUT")

        self._reset_manifest()
        self.manifest["assets"][0]["partials"][1]["index"] = 1
        self._invalid("partial H1 is duplicated")


if __name__ == "__main__":
    unittest.main()
