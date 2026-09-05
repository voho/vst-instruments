#!/usr/bin/env python3
"""Exploratory comparison with Mohammed Alkooheji's Acoustic Guitar Notes v3.

Source: https://www.kaggle.com/datasets/mohammedalkooheji/guitar-notes-dataset
Mohammed Alkooheji, 2024, CC BY-SA 4.0. The author identifies a steel-string
Walden G551E and nylon-string Yamaha CM-40. The capture chain, note velocities,
string/fret assignments and pluck positions are unknown. The author explicitly
warns that technique labels are inconsistent: 'f' merges finger AND thumb.
This corpus is independent exploratory descriptor evidence, not a controlled
technique/material experiment, a tuning set, or a perceptual realism ranking.

Frozen selection, declared BEFORE inspecting selected audio: all 43 pitches
D2 through G#5, labels spn/sfn/npn/nfn (normally sounded pick/finger-or-thumb),
and the first TWO NUMERIC ordinals of each pitch/label: 344 recordings. Never
replace a target after scoring. Verify the v3 archive SHA256 and filenames.
All source WAVs must contain exactly two seconds of 44.1 kHz PCM16 mono audio.

Render a two-second held note from frame zero at velocity 91, without inferring
velocity from recording level. Use the lowest-fret feasible string in standard
tuning, except D2/D#2 use the low string in Drop D. This is a reproducible
assumption, not the unknown recorded fingering. Map 'p' to Pick and the merged
'f' label to Finger; this does not independently benchmark Thumb. Use Stereo
mics, a declared Original/Fylde bridge and otherwise unchanged engine defaults.
The Fylde option affects steel only; nylon retains its original bridge.
An identical note/material/picking/control render is shared by both ordinals.

Reuse FitPhysicalModel.extract_features at 48 kHz with its own onset detector
and analysis windows: normalized attack bands, six-scale spectral bands,
harmonic levels, tuning, pitch trajectory, decay slopes and body bands. Its
onsets are reported, not scored as latency: source trimming is unknown. No
level/velocity score or new weighted composite is computed. Report each
descriptor's physical-unit MAE on finite target/model pairs, alongside target
availability and missing-model counts. A missing harmonic is NOT counted as a
zero error; MAEs with different coverage must not be treated as improvements.
Group means pool comparable descriptor elements, by pitch/material/label.
No EQ, denoising, parameter fitting, manual alignment or loudness-normalized
audio is written. Feature normalization is exactly the existing dry scorer's.

Usage (existing NumPy/SciPy dependencies):
  python3 Tools/BenchmarkTechniqueNotes.py --archive /tmp/guitar-notes-v3.zip \
      --renderer ./build-dsp/AcustraPerformanceRenderer --output /tmp/note-audit
  python3 Tools/BenchmarkTechniqueNotes.py --self-test

Output must be new. It retains selected original WAVs, events, model F32s and
hashes/controls in report.json; never commit the corpus or these audio copies.
"""

from __future__ import annotations

import argparse
from collections import defaultdict
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import zipfile

import numpy as np
import scipy
from scipy.io import wavfile

import FitPhysicalModel as physical
from BenchmarkPerformances import digest, write_events


SOURCE = "https://www.kaggle.com/datasets/mohammedalkooheji/guitar-notes-dataset"
ARCHIVE_SHA256 = "4563a70d5dd9a14919171d103f10f6731fd78e63f1498dd11047bfea471f6a6d"
ARCHIVE_BYTES = 178227614
RATE, SECONDS, VELOCITY = 48000, 2, 91
PITCH_NAMES = ("C", "Csharp", "D", "Dsharp", "E", "F", "Fsharp", "G", "Gsharp", "A", "Asharp", "B")
PITCHES = {PITCH_NAMES[midi % 12] + str(midi // 12 - 1): midi for midi in range(38, 81)}
LABELS = ("spn", "sfn", "npn", "nfn")
FILE_PATTERN = re.compile(r"Guitar Dataset/([A-G](?:sharp)?[2-5])/\1-([1-9][0-9]*)-([a-z]{2,3})\.wav")
DESCRIPTORS = {
    "attack": "dB", "multiscale": "dB", "harmonics": "dB",
    "tuning": "cents", "pitch_trajectory": "cents",
    "decay": "dB/s", "body": "dB",
}


def select_targets(names: list[str]) -> list[dict]:
    """Filename-only selection; no waveform information may affect this list."""
    if len(set(names)) != len(names):
        raise ValueError("archive contains duplicate member names")
    groups = defaultdict(list)
    for filename in names:
        match = FILE_PATTERN.fullmatch(filename)
        if match is None or match[1] not in PITCHES:
            raise ValueError(f"unexpected corpus filename: {filename}")
        pitch, ordinal, label = match.groups()
        if label in LABELS:
            groups[pitch, label].append({
                "source_member": filename, "pitch": pitch, "midi": PITCHES[pitch],
                "ordinal": int(ordinal), "label": label,
                "material": "steel" if label[0] == "s" else "nylon",
                "picking": "pick" if label[1] == "p" else "finger",
            })
    selected = []
    for pitch in PITCHES:
        for label in LABELS:
            rows = sorted(groups[pitch, label], key=lambda row: row["ordinal"])
            if len(rows) < 2:
                raise ValueError(f"expected two recordings for {pitch}/{label}")
            selected.extend(rows[:2])
    return selected


def fingering(midi: int) -> dict:
    if midi not in PITCHES.values():
        raise ValueError("pitch is outside the predeclared D2-G#5 range")
    tuning = "drop_d" if midi < 40 else "standard"
    opens = (38 if tuning == "drop_d" else 40, 45, 50, 55, 59, 64)
    fret, channel = min((midi - opened, index + 1) for index, opened in enumerate(opens)
                        if 0 <= midi - opened <= 20)
    return {"tuning": tuning, "channel": channel, "fret": fret}


def feature_errors(target: dict, model: dict) -> dict:
    result = {}
    for name, unit in DESCRIPTORS.items():
        first, second = (np.asarray(features[name], dtype=float).ravel() for features in (target, model))
        if first.shape != second.shape:
            raise ValueError(f"different {name} feature shapes")
        available = np.isfinite(first)
        comparable = available & np.isfinite(second)
        result[name] = {
            "unit": unit,
            "mean_absolute_error": float(np.mean(np.abs(second[comparable] - first[comparable])))
            if np.any(comparable) else None,
            "comparable_count": int(np.count_nonzero(comparable)),
            "target_available_count": int(np.count_nonzero(available)),
            "missing_model_count": int(np.count_nonzero(available & ~np.isfinite(second))),
            "target_unavailable_count": int(np.count_nonzero(~available)),
        }
    return result


def summarise(rows: list[dict]) -> dict:
    terms = {}
    for name, unit in DESCRIPTORS.items():
        values = [row["metrics"][name] for row in rows]
        counts = {key: sum(value[key] for value in values) for key in (
            "comparable_count", "target_available_count", "missing_model_count", "target_unavailable_count")}
        total_error = sum(value["mean_absolute_error"] * value["comparable_count"]
                          for value in values if value["comparable_count"])
        terms[name] = {"unit": unit, "mean_absolute_error": total_error / counts["comparable_count"]
                       if counts["comparable_count"] else None, **counts}
    return {"recordings": len(rows), "descriptors": terms}


def read_reference(raw: bytes, name: str) -> np.ndarray:
    rate, audio = wavfile.read(io.BytesIO(raw))
    if rate != 44100 or audio.ndim != 1 or audio.size != SECONDS * rate or audio.dtype != np.int16:
        raise ValueError(f"{name}: expected exactly two seconds of 44.1 kHz PCM16 mono audio")
    audio = audio.astype(float) / 32768.0
    if not np.isfinite(audio).all() or not np.any(audio):
        raise ValueError(f"{name}: silent or non-finite reference")
    return physical._resample(audio, rate, RATE)


def benchmark(archive_path: Path, renderer: Path, output: Path, bridge: str = "original") -> dict:
    if bridge not in ("original", "fylde"):
        raise ValueError("unknown bridge model")
    if archive_path.stat().st_size != ARCHIVE_BYTES or digest(archive_path) != ARCHIVE_SHA256:
        raise ValueError("archive does not match the frozen Acoustic Guitar Notes v3 download")
    renderer = renderer.resolve(strict=True)
    with zipfile.ZipFile(archive_path) as archive:
        selected = select_targets(archive.namelist())
        output.mkdir()  # Refuse to replace an earlier experiment.
        (output / "references").mkdir()
        (output / "models").mkdir()
        # Persist the filename-only selection before reading any target audio.
        (output / "selection.json").write_text(json.dumps(selected, indent=2) + "\n")
        models, rows = {}, []
        for target in selected:
            midi, material, picking = target["midi"], target["material"], target["picking"]
            model_id = f"{midi:03d}-{material}-{picking}"
            if model_id not in models:
                controls = {"midi": midi, "velocity": VELOCITY, "bend_semitones": 0,
                            "string_material": material, "picking": picking,
                            "capture": "stereo_mic", "bridge_model": bridge,
                            "effective_bridge_model": bridge if material == "steel" else "original",
                            **fingering(midi)}
                event_path = output / "models" / (model_id + ".events")
                model_path = output / "models" / (model_id + ".f32")
                write_events(event_path, [(0, controls["channel"], midi, VELOCITY, 0.0)], RATE * SECONDS)
                subprocess.run([str(renderer), str(event_path), str(model_path), "stereo_mic", picking, bridge,
                                "--string-material", material, "--tuning", controls["tuning"]], check=True)
                audio = np.fromfile(model_path, dtype="<f4")
                if audio.size != 2 * RATE * SECONDS or not np.isfinite(audio).all() or not np.any(audio):
                    raise ValueError(f"{model_id}: invalid or silent model render")
                mono = audio.reshape(-1, 2).mean(axis=1).astype(float)
                if not np.any(mono):
                    raise ValueError(f"{model_id}: stereo model cancels to silent mono")
                features = physical.extract_features(mono, RATE, midi)
                models[model_id] = {
                    "features": features,
                    "report": {"controls": controls, "model_file": str(model_path.relative_to(output)),
                               "model_sha256": digest(model_path), "events_file": str(event_path.relative_to(output)),
                               "events_sha256": digest(event_path), "onset_seconds": features["latency_seconds"]},
                }
            raw = archive.read(target["source_member"])
            reference = read_reference(raw, target["source_member"])
            reference_path = output / "references" / PurePosixPath(target["source_member"]).name
            reference_path.write_bytes(raw)
            features = physical.extract_features(reference, RATE, midi)
            rows.append({**target, "model_id": model_id,
                         "reference_file": str(reference_path.relative_to(output)),
                         "reference_sha256": hashlib.sha256(raw).hexdigest(),
                         "reference_onset_seconds": features["latency_seconds"],
                         "metrics": feature_errors(features, models[model_id]["features"])})
    report = {
        "dataset": "Acoustic Guitar Notes Dataset v3", "source": SOURCE,
        "attribution": "Mohammed Alkooheji, 2024; CC BY-SA 4.0",
        "archive_sha256": ARCHIVE_SHA256, "archive_bytes": ARCHIVE_BYTES,
        "renderer_sha256": digest(renderer), "scorer_sha256": digest(Path(__file__)),
        "physical_scorer_sha256": digest(Path(physical.__file__)),
        "event_writer_sha256": digest(Path(__file__).with_name("BenchmarkPerformances.py")),
        "numpy_version": np.__version__, "scipy_version": scipy.__version__,
        "analysis_rate": RATE, "duration_seconds": SECONDS,
        "selection": "all D2-G#5; spn/sfn/npn/nfn; first two numeric ordinals per pitch/label, chosen before audio inspection",
        "reference_instruments": {"steel": "Walden G551E", "nylon": "Yamaha CM-40"},
        "reference_capture": "unknown; author supplied mono recordings",
        "render_protocol": "held note at frame0; velocity91; zero bend; lowest-fret feasible string; standard except D2/D#2 DropD; 127-frame blocks; declared controls and otherwise engine defaults",
        "descriptor_protocol": "FitPhysicalModel onset/windows/feature normalizations; physical-unit MAE on finite pairs; pooled element means; availability/missing-model counts; no level, latency or composite score",
        "limitations": ["exploratory independent corpus; no parameter fitting or selection by scores",
                        "author warns technique variation is inconsistent",
                        "finger label includes thumb; no independent Thumb evaluation",
                        "instrument, material, capture and performance differences are confounded",
                        "source velocities, string/fret assignment, pluck location and source trim unknown",
                        "model missing descriptors remain explicit; different coverage cannot establish improvement",
                        "two-second clips limit decay observation; source onsets are estimates",
                        "does not establish controlled technique fidelity, perceptual equivalence or market rank"],
        "summary": summarise(rows),
        "by_label": {label: summarise([row for row in rows if row["label"] == label]) for label in LABELS},
        "by_material": {material: summarise([row for row in rows if row["material"] == material])
                        for material in ("steel", "nylon")},
        "by_pitch": {pitch: summarise([row for row in rows if row["pitch"] == pitch]) for pitch in PITCHES},
        "models": {name: model["report"] for name, model in models.items()},
        "recordings": rows,
    }
    (output / "report.json").write_text(json.dumps(report, indent=2, allow_nan=False) + "\n")
    return report


def self_test() -> None:
    names = [f"Guitar Dataset/{pitch}/{pitch}-{ordinal}-{label}.wav"
             for pitch in PITCHES for label in LABELS for ordinal in (10, 2, 7)]
    selected = select_targets(names)
    assert len(selected) == 344 and {row["ordinal"] for row in selected} == {2, 7}
    assert selected == select_targets(list(reversed(names)))
    for invalid in (names + [names[0]], names + ["Guitar Dataset/C3/D3-1-spn.wav"],
                    [name for name in names if not name.endswith("-spn.wav")]):
        try:
            select_targets(invalid)
            raise AssertionError("invalid or incomplete selection accepted")
        except ValueError:
            pass
    assert fingering(38) == {"tuning": "drop_d", "channel": 1, "fret": 0}
    assert fingering(39) == {"tuning": "drop_d", "channel": 1, "fret": 1}
    assert fingering(40) == {"tuning": "standard", "channel": 1, "fret": 0}
    assert fingering(80) == {"tuning": "standard", "channel": 6, "fret": 16}
    a = {name: np.array([1., 3., np.nan]) for name in DESCRIPTORS}
    b = {name: np.array([4., np.nan, 100.]) for name in DESCRIPTORS}
    result = feature_errors(a, b)
    assert all(row["mean_absolute_error"] == 3 and row["comparable_count"] == 1
               and row["target_available_count"] == 2 and row["missing_model_count"] == 1
               and row["target_unavailable_count"] == 1 for row in result.values())
    missing = feature_errors(a, {name: np.full(3, np.nan) for name in DESCRIPTORS})
    assert all(row["mean_absolute_error"] is None and row["missing_model_count"] == 2
               for row in missing.values())
    pooled = summarise([{"metrics": result}, {"metrics": missing}])
    assert pooled["descriptors"]["harmonics"]["missing_model_count"] == 3
    json.dumps(pooled, allow_nan=False)
    print("Technique benchmark self-test passed: frozen numeric selection, fingering and missing-feature accounting")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--renderer", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--bridge-model", choices=("original", "fylde"), default="original")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if args.archive is None or args.renderer is None or args.output is None:
        parser.error("--archive, --renderer and --output are required")
    report = benchmark(args.archive, args.renderer, args.output, args.bridge_model)
    print(json.dumps({"recordings": len(report["recordings"]), "renders": len(report["models"]),
                      "summary": report["summary"], "by_material": report["by_material"]}, indent=2))


if __name__ == "__main__":
    main()
