#!/usr/bin/env python3
"""Compare six fixed real GuitarSet performances with the shipping engine.

Protocol: before inspecting the audio, select the alphabetically first excerpt
for each player, alternating comp (00/02/04) and solo (01/03/05). This gives the
same BN1-129-Eb phrase across six players, three comping and three solo takes.
Use the first 12 seconds from source frame zero, with no onset alignment,
denoising, EQ, silence rejection or time warping. This is a small performance
audit of one phrase, not coverage of all genres, players or commercial rivals.

GuitarSet v1.1.0: Xi, Bittner, Pauwels, Ye and Bello, ISMIR 2018,
https://zenodo.org/records/3371780 (CC BY 4.0). Recordings use nickel-wound steel
strings and a Neumann U87 condenser about 30 cm from the 18th fret. The optional
magnetic reference is the simultaneous mono mix of six Ubertar single-coil
pickups, not piezo or microphone audio. Their position, field response, loaded
electronics and channel balance are not specified in the paper:
https://guitarset.weebly.com/uploads/1/2/1/6/121620128/xi_ismir_2018.pdf
Use annotation.zip and audio_mono-mic.zip (656.9 MB), or
audio_mono-pickup_mix.zip (683.1 MB); published MD5s are verified for the selected
archives. No six-channel download is needed. Archives/audio are never committed.

JAMS note_midi events determine the actual string, onset, duration and mean
pitch (fractional semitones retained as per-channel bend). Intersect each
offset with the next onset on that string. Render with string-per-channel
mode, fixed MIDI velocity 91 and sensed release velocity zero. Confidence is
not velocity. No gesture labels exist to choose a thumb, pick, slide, mute or
hammer-on; explicit capture/picking choices are reported. Previously measured
GuitarSet strum statistics informed the engine, so this is a different-instrument
performance evaluation, not a completely untouched holdout. The renderer
replays annotated timing, so this does not evaluate automatic MIDI strumming.
The optional --bridge-model fylde uses the measured steel bridge alternative;
all other controls and the microphone radiation remain as in the baseline.

Score at 48 kHz after one whole-clip RMS match. Reuse the dry-note scorer's six
prime STFT lengths for mean log-magnitude error (dB) and spectral convergence;
also report chroma cosine distance from a separate 4096-point spectrum. A shared
1e-5 magnitude floor relative to clip RMS bounds logarithms only; quiet frames
remain in every mean. Chroma measures pitch-class energy, not transcription
accuracy. There is no fitted loss, scalar realism ranking or measured velocity
score. Lower distances mean closer descriptors on exactly these recordings;
room/mic/body, unannotated noise and missing expressive controls also contribute.
The same 4096-point spectra report octave-band energy differences from 80 Hz to
10 kHz after the whole-clip RMS match. Positive means more model energy, negative
means less; these signed diagnostics locate spectral gaps and are not additional
terms in the distance. An exactly empty band on either side is reported as null.

Usage (NumPy and SciPy, no JAMS/MIDI dependency):
  python3 Tools/BenchmarkPerformances.py --dataset /tmp/acustra-guitarset \
      --renderer /path/to/AcustraPerformanceRenderer --output /tmp/performance-audit
  python3 Tools/BenchmarkPerformances.py --dataset /tmp/acustra-guitarset \
      --renderer /path/to/AcustraPerformanceRenderer --output /tmp/pickup-audit \
      --reference-capture magnetic_pickup --capture magnetic
  python3 Tools/BenchmarkPerformances.py --self-test --renderer /path/to/renderer
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import math
import subprocess
import tempfile
import zipfile
from pathlib import Path

import numpy as np
import scipy
import FitPhysicalModel
from scipy.io import wavfile
from scipy.signal import stft

from FitPhysicalModel import MULTISCALE_WINDOWS, _resample

RATE = 48_000
SECONDS = 12
VELOCITY = 91
OPEN_NOTES = (40, 45, 50, 55, 59, 64)
BAND_EDGES = (80, 160, 320, 640, 1280, 2560, 5120, 10000)
TRACKS = tuple(f"{player:02d}_BN1-129-Eb_{'comp' if player % 2 == 0 else 'solo'}"
               for player in range(6))
ARCHIVES = {"annotation.zip": "b39b78e63d3446f2e54ddb7a54df9b10",
            "audio_mono-mic.zip": "275966d6610ac34999b58426beb119c3",
            "audio_mono-pickup_mix.zip": "aecce79f425a44e2055e46f680e10f6a"}


def digest(path: Path, algorithm: str = "sha256") -> str:
    result = hashlib.new(algorithm)
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def events_from_jams(jam: dict) -> tuple[list[tuple], int]:
    events = []
    note_count = 0
    seen_strings = set()
    for annotation in jam["annotations"]:
        if annotation["namespace"] != "note_midi":
            continue
        string = int(annotation["annotation_metadata"]["data_source"])
        if string not in range(6) or string in seen_strings:
            raise ValueError("invalid or duplicate annotated string")
        seen_strings.add(string)
        notes = sorted(annotation["data"], key=lambda row: row["time"])
        for index, note in enumerate(notes):
            start, duration, pitch = (float(note[field]) for field in ("time", "duration", "value"))
            if not all(math.isfinite(value) for value in (start, duration, pitch)) or start < 0 or duration <= 0:
                raise ValueError("invalid annotated note")
            if start >= SECONDS:
                continue
            end = min(start + duration, SECONDS)
            if index + 1 < len(notes):
                end = min(end, float(notes[index + 1]["time"]))
            midi = round(pitch)
            if not 0 <= midi - OPEN_NOTES[string] <= 20:
                raise ValueError(f"unplayable annotated string/fret: {string}, {pitch}")
            first, last = round(start * RATE), round(end * RATE)
            if last <= first:
                raise ValueError("overlapping or zero-length annotated note")
            events.append((first, string + 1, midi, VELOCITY, pitch - midi))
            if last < SECONDS * RATE:
                events.append((last, string + 1, midi, 0, 0.0))
            note_count += 1
    if seen_strings != set(range(6)) or not note_count:
        raise ValueError("performance must annotate all six strings and contain notes")
    # Release before replucking, including a repeated pitch on the same sample.
    events.sort(key=lambda row: (row[0], row[3] != 0, row[1]))
    return events, note_count


def write_events(path: Path, events: list[tuple], frames: int = SECONDS * RATE) -> None:
    path.write_text(f"ACUSTRA_PERFORMANCE_V1 {RATE} {frames}\n" + "".join(
        " ".join(str(value) for value in row) + "\n" for row in events), encoding="utf-8")


def spectrum(audio: np.ndarray, size: int) -> tuple[np.ndarray, np.ndarray]:
    frequency, _, values = stft(audio, RATE, window="hann", nperseg=size,
                              noverlap=size // 2, boundary=None, padded=False)
    return frequency, np.abs(values)


def rms(audio: np.ndarray) -> float:
    return float(np.sqrt(np.mean(np.square(audio))))


def score(target: np.ndarray, model: np.ndarray) -> dict:
    if target.shape != model.shape or target.ndim != 1 or len(target) < 4096:
        raise ValueError("performance scoring requires equal-length mono clips")
    if not np.isfinite(target).all() or not np.isfinite(model).all() or min(rms(target), rms(model)) <= 0:
        raise ValueError("performance contains silence or non-finite samples")
    target, model = target / rms(target), model / rms(model)
    scales = []
    for size in MULTISCALE_WINDOWS:
        _, first = spectrum(target, size)
        _, second = spectrum(model, size)
        target_norm = float(np.linalg.norm(first))
        if target_norm == 0.0 or not math.isfinite(target_norm):
            raise ValueError("target has no finite energy in the analysis windows")
        scales.append({
            "fft_size": size,
            "log_magnitude_mae_db": float(np.mean(np.abs(
                20 * np.log10(np.maximum(first, 1e-5))
                - 20 * np.log10(np.maximum(second, 1e-5))))),
            "spectral_convergence": float(np.linalg.norm(first - second) / target_norm),
        })
    chromas, band_energies = [], []
    for audio in (target, model):
        frequency, magnitude = spectrum(audio, 4096)
        band_energies.append(np.array([
            np.square(magnitude[(frequency >= low) & (frequency < high)]).sum()
            for low, high in zip(BAND_EDGES[:-1], BAND_EDGES[1:])
        ]))
        selected = (frequency >= 80) & (frequency <= 5000)
        classes = np.rint(69 + 12 * np.log2(frequency[selected] / 440)).astype(int) % 12
        power = np.square(magnitude[selected])
        chroma = np.array([power[classes == pitch].sum(axis=0) for pitch in range(12)])
        norm = np.linalg.norm(chroma, axis=0)
        chromas.append(chroma / np.maximum(norm, 1e-30))
    similarity = np.sum(chromas[0] * chromas[1], axis=0)
    both_silent = (np.linalg.norm(chromas[0], axis=0) == 0) & (np.linalg.norm(chromas[1], axis=0) == 0)
    similarity[both_silent] = 1.0
    return {
        "log_magnitude_mae_db": float(np.mean([row["log_magnitude_mae_db"] for row in scales])),
        "spectral_convergence": float(np.mean([row["spectral_convergence"] for row in scales])),
        "chroma_cosine_distance": float(np.mean(1 - np.clip(similarity, 0, 1))),
        "octave_bands": [
            {"low_hz": low, "high_hz": high,
             "model_minus_reference_db": float(10 * np.log10(second / first))
             if min(first, second) > 0 else None}
            for low, high, first, second in zip(
                BAND_EDGES[:-1], BAND_EDGES[1:], *band_energies)
        ],
        "scales": scales,
    }


def archive_entry(archive: zipfile.ZipFile, filename: str) -> bytes:
    names = [name for name in archive.namelist() if Path(name).name == filename and not name.startswith("__MACOSX/")]
    if len(names) != 1:
        raise ValueError(f"archive must contain exactly one {filename}")
    return archive.read(names[0])


def benchmark(dataset: Path, renderer: Path, output: Path, capture: str, picking: str,
              bridge: str = "original", reference_capture: str = "microphone") -> dict:
    if reference_capture not in ("microphone", "magnetic_pickup"):
        raise ValueError("unknown reference capture")
    archive_name = "audio_mono-mic.zip" if reference_capture == "microphone" else "audio_mono-pickup_mix.zip"
    audio_suffix = "_mic.wav" if reference_capture == "microphone" else "_mix.wav"
    archives = {name: ARCHIVES[name] for name in ("annotation.zip", archive_name)}
    for filename, expected in archives.items():
        if digest(dataset / filename, "md5") != expected:
            raise ValueError(f"{filename}: does not match the published GuitarSet v1.1.0 archive")
    output.mkdir()  # Never replace a prior audit or listening artefact.
    rows = []
    with zipfile.ZipFile(dataset / "annotation.zip") as annotations, zipfile.ZipFile(dataset / archive_name) as audio:
        for track in TRACKS:
            annotation_bytes = archive_entry(annotations, track + ".jams")
            recording_bytes = archive_entry(audio, track + audio_suffix)
            events, note_count = events_from_jams(json.loads(annotation_bytes))
            event_path = output / (track + ".events")
            write_events(event_path, events)
            model_path = output / (track + ".f32")
            subprocess.run([str(renderer), str(event_path), str(model_path), capture, picking, bridge], check=True)
            source_rate, target = wavfile.read(io.BytesIO(recording_bytes))
            if target.ndim != 1 or len(target) < source_rate * SECONDS:
                raise ValueError(f"{track}: expected at least 12 seconds of {reference_capture} mono audio")
            if np.issubdtype(target.dtype, np.integer):
                target = target.astype(float) / max(abs(np.iinfo(target.dtype).min), np.iinfo(target.dtype).max)
            target = _resample(target[:source_rate * SECONDS], source_rate, RATE)
            model = np.fromfile(model_path, dtype="<f4").reshape(-1, 2).mean(axis=1).astype(float)
            metrics = score(target, model)
            normal_target, normal_model = target / rms(target), model / rms(model)
            audition_rms = min(0.1, 0.95 / max(np.max(np.abs(normal_target)), np.max(np.abs(normal_model))))
            for label, signal in (("reference", normal_target), ("model", normal_model)):
                wavfile.write(output / f"{track}-{label}.wav", RATE, (signal * audition_rms).astype(np.float32))
            rows.append({"track": track, "notes": note_count, "source_rate": source_rate,
                         "source_audio_filename": track + audio_suffix,
                         "source_audio_sha256": hashlib.sha256(recording_bytes).hexdigest(),
                         "annotation_sha256": hashlib.sha256(annotation_bytes).hexdigest(),
                         "events_sha256": digest(event_path), "model_sha256": digest(model_path),
                         "reference_audition_trim_db": 20 * math.log10(audition_rms / rms(target)),
                         "model_audition_trim_db": 20 * math.log10(audition_rms / rms(model)),
                         "metrics": metrics})
            print(track, note_count, json.dumps({k: v for k, v in metrics.items() if k != "scales"}), flush=True)
    return {
        "dataset": "GuitarSet v1.1.0", "source": "https://zenodo.org/records/3371780",
        "attribution": "Qingyang Xi, Rachel M. Bittner, Johan Pauwels, Xuzhou Ye, Juan P. Bello, ISMIR 2018; CC BY 4.0",
        "archive_md5": archives, "renderer_sha256": digest(renderer),
        "physical_scorer_sha256": digest(Path(FitPhysicalModel.__file__)),
        "scorer_sha256": digest(Path(__file__)),
        "numpy_version": np.__version__, "scipy_version": scipy.__version__,
        "analysis_rate": RATE, "start_seconds": 0, "duration_seconds": SECONDS,
        "velocity": VELOCITY, "capture": capture, "picking": picking, "bridge_model": bridge,
        "reference_capture": reference_capture,
        "reference_transducer": ("Neumann U87 condenser, about 30 cm from the 18th fret"
                                 if reference_capture == "microphone" else
                                 "Simultaneous mono mix of six Ubertar single-coil magnetic pickups"),
        "render_protocol": "shipping calibration/default controls; string-per-channel; 127-frame blocks; annotated mean pitch; zero release velocity",
        "limitations": "One phrase, six players; timing/strum statistics previously informed engine. No measured note velocities, gesture labels, matched guitar body or matched transducer. Pickup position, field response, loaded electronics and channel balance are undocumented. Descriptor distances include those differences and do not establish perceptual equivalence or market rank.",
        "mean": {name: float(np.mean([row["metrics"][name] for row in rows]))
                 for name in ("log_magnitude_mae_db", "spectral_convergence", "chroma_cosine_distance")},
        "performances": rows,
    }


def self_test(renderer: Path | None) -> None:
    time = np.arange(RATE) / RATE
    target = np.sin(2 * np.pi * 220 * time) * np.exp(-2 * time)
    wrong = np.sin(2 * np.pi * 277.183 * time) * np.exp(-6 * time)
    same, different = score(target, target * 0.3), score(target, wrong)
    for name in ("log_magnitude_mae_db", "spectral_convergence", "chroma_cosine_distance"):
        assert same[name] < 1e-10 and different[name] > same[name] + 0.05, (name, same, different)
    # A two-tone balance change must retain its sign and exact relative level
    # after the shared whole-clip RMS normalization. It cannot be read as a
    # louder-is-better score or disappear through per-band normalization.
    low = np.sin(2 * np.pi * 110 * time)
    high = np.sin(2 * np.pi * 3520 * time)
    balance = score(low + high, low + 2 * high)["octave_bands"]
    expected_low = 10 * np.log10(2 / 5)
    expected_high = 10 * np.log10(8 / 5)
    assert abs(balance[0]["model_minus_reference_db"] - expected_low) < 0.01
    assert abs(balance[5]["model_minus_reference_db"] - expected_high) < 0.01
    edge = np.zeros(4096)
    edge[-1] = 1.0
    try:
        score(edge, edge)
    except ValueError:
        pass
    else:
        raise AssertionError("unobserved edge impulse must not yield a NaN score")
    annotations = [{"namespace": "note_midi", "annotation_metadata": {"data_source": str(string)},
                    "data": []} for string in range(6)]
    annotations[0]["data"] = [
        {"time": 0.0, "duration": 0.8, "value": 40.2, "confidence": 0.01},
        {"time": 0.5, "duration": 0.2, "value": 40.0, "confidence": None},
    ]
    events, count = events_from_jams({"annotations": annotations})
    assert count == 2 and events[1][:4] == (RATE // 2, 1, 40, 0)
    assert events[2][:4] == (RATE // 2, 1, 40, VELOCITY)
    assert abs(events[0][4] - 0.2) < 1e-12
    try:
        events_from_jams({"annotations": annotations + [annotations[0]]})
    except ValueError:
        pass
    else:
        raise AssertionError("accepted a duplicate annotation string")
    if renderer is not None:
        with tempfile.TemporaryDirectory(prefix="acustra-performance-test-") as directory:
            root = Path(directory)
            events = [(0, 1, 40, 91, 0.0), (RATE // 2, 1, 40, 0, 0.0),
                      (RATE // 2, 1, 40, 91, 0.1)]
            event_path = root / "test.events"
            write_events(event_path, events, RATE)
            for letter in ("a", "b"):
                subprocess.run([str(renderer), str(event_path), str(root / (letter + ".f32"))], check=True)
            assert (root / "a.f32").read_bytes() == (root / "b.f32").read_bytes()
            default_hash = digest(root / "a.f32")
            for name, options in (
                ("legacy-capture", ["stereo_mic", "finger"]),
                ("explicit-default", ["stereo_mic", "finger", "original",
                                      "--string-material", "steel", "--tuning", "standard"]),
                ("flags-only", ["--tuning", "standard", "--string-material", "steel"]),
            ):
                output = root / (name + ".f32")
                subprocess.run([str(renderer), str(event_path), str(output), *options], check=True)
                assert digest(output) == default_hash, "default material/tuning changed existing audio"
            rendered = np.fromfile(root / "a.f32", dtype="<f4")
            assert len(rendered) == RATE * 2 and np.isfinite(rendered).all() and np.max(np.abs(rendered)) > 0
            alternative = root / "fylde.f32"
            subprocess.run([str(renderer), str(event_path), str(alternative),
                            "stereo_mic", "finger", "fylde"], check=True)
            assert alternative.read_bytes() != (root / "a.f32").read_bytes()
            nylon = root / "nylon.f32"
            subprocess.run([str(renderer), str(event_path), str(nylon),
                            "stereo_mic", "finger", "--string-material", "nylon"], check=True)
            nylon_audio = np.fromfile(nylon, dtype="<f4")
            assert np.isfinite(nylon_audio).all() and np.max(np.abs(nylon_audio)) > 0
            assert digest(nylon) != default_hash, "nylon selection did not change the physical strings"
            rejected = subprocess.run([str(renderer), str(event_path), str(root / "invalid.f32"),
                                       "stereo_mic", "finger", "unknown"], capture_output=True)
            assert rejected.returncode != 0 and not (root / "invalid.f32").exists()
            for options in (["--string-material", "bronze"], ["--tuning", "open_z"],
                            ["--tuning"], ["--unknown", "steel"],
                            ["--tuning", "standard", "--tuning", "drop_d"],
                            ["--string-material", "steel", "--string-material", "nylon"]):
                rejected = subprocess.run([str(renderer), str(event_path), str(root / "invalid.f32"),
                                           *options], capture_output=True)
                assert rejected.returncode != 0 and not (root / "invalid.f32").exists(), options
            rejected = subprocess.run([str(renderer), str(event_path), str(root / "a.f32")],
                                      capture_output=True)
            assert rejected.returncode != 0 and (root / "a.f32").read_bytes() == (root / "b.f32").read_bytes()
            # D2 is playable on Drop D's lowest string, but not Standard's.
            write_events(event_path, [(0, 1, 38, 91, 0.0), (RATE // 2, 1, 38, 0, 0.0)], RATE)
            rejected = subprocess.run([str(renderer), str(event_path), str(root / "invalid.f32")],
                                      capture_output=True)
            assert rejected.returncode != 0 and not (root / "invalid.f32").exists()
            for material in ("steel", "nylon"):
                output = root / ("drop-d-" + material + ".f32")
                subprocess.run([str(renderer), str(event_path), str(output), "stereo_mic", "finger", "original",
                                "--string-material", material, "--tuning", "drop_d"], check=True)
                audio = np.fromfile(output, dtype="<f4")
                assert len(audio) == RATE * 2 and np.isfinite(audio).all() and np.max(np.abs(audio)) > 0
            for note, channel in ((37, 1), (38, 2), (59, 1)):
                write_events(event_path, [(0, channel, note, 91, 0.0)], RATE)
                rejected = subprocess.run([str(renderer), str(event_path), str(root / "invalid.f32"),
                                           "--tuning", "drop_d"], capture_output=True)
                assert rejected.returncode != 0 and not (root / "invalid.f32").exists(), (note, channel)
            for invalid in ("not an event", "0 6 40 91 0"):
                event_path.write_text(f"ACUSTRA_PERFORMANCE_V1 {RATE} {RATE}\n{invalid}\n", encoding="utf-8")
                rejected = subprocess.run([str(renderer), str(event_path), str(root / "bad.f32")],
                                          capture_output=True)
                assert rejected.returncode != 0 and not (root / "bad.f32").exists()
    print("performance benchmark self-test passed: level invariance, pitch/timbre contrast, note scheduling"
          + (" and deterministic rendering" if renderer is not None else ""))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dataset", type=Path)
    parser.add_argument("--renderer", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--capture", default="stereo_mic", choices=("stereo_mic", "treble_mic", "bass_mic", "saddle_piezo", "magnetic", "upper_mic"))
    parser.add_argument("--reference-capture", default="microphone", choices=("microphone", "magnetic_pickup"))
    parser.add_argument("--picking", default="finger", choices=("finger", "pick", "thumb"))
    parser.add_argument("--bridge-model", default="original", choices=("original", "fylde"))
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test(args.renderer.resolve() if args.renderer else None)
    else:
        if args.dataset is None or args.renderer is None or args.output is None:
            parser.error("--dataset, --renderer and --output are required")
        report = benchmark(args.dataset, args.renderer.resolve(), args.output,
                           args.capture, args.picking, args.bridge_model, args.reference_capture)
        (args.output / "report.json").write_text(json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
