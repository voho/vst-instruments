#!/usr/bin/env python3
"""Recording-only AG-PT-set hammer-on plateau audit; no synthesizer or model fit.

Source: https://zenodo.org/records/10159492
Domenico Stefani and Luca Turchet / CIMIL, AG-PT-set, CC BY 4.0.
Protocol paper: https://www.lucaturchet.it/PUBLIC_DOWNLOADS/publications/conferences/On_the_Importance_of_Temporally_Precise_Onset_Annotations_for_Real-Time_Music_Information_Retrieval-_Findings_from_the_AG-PT-set_Dataset.pdf
The published hammer-on technique is upward semitone legato. These files have
NO authored event onsets or pitch labels: inferred adjacent pitch plateaus are
not individually verified hammer events. The general recording protocol starts
open and advances chromatically, three repeats per key/dynamic. Neither pitch
estimator is constrained to that sequence and no confidence gaps are bridged.

Frozen selection BEFORE audio inspection: earliest lexicographic Taylor 114CE
(guitar_id 2) hammer-on filename separately for piano and forte; same player 3,
2020-08-14. The first TEN seconds were selected before plateau scoring. Published
companion soundhole-pick labels identify allstring1 as high E4/MIDI 64, despite
the paper's conflicting string-number description. LR Baggs Anthem capture;
the paper reports a 50% internal condenser / 50% piezo blend. Actual transfer,
gain calibration and finger force/velocity are unknown. Piano/forte are ordinal.

Reproduce the frozen pitch_trace.py + plateau_ratio.py analysis: unwindowed
overlap-normalized lag correlation; independent Hann FFT dominant peak AND
six-harmonic peak-energy sum. Preserve octave disagreements, both agreement
masks, all retained plateaus, rejected upward candidates and coverage. Analysis
windows are 2048 samples at 48 kHz, with a 96-sample hop. Correlation >= .85,
window RMS within 45 dB of excerpt peak, FFT agreement <= 35 cents, distance to
semitone STRICTLY < 25 cents, contiguous plateau center span >= 25 ms. Compare
adjacent upward semitones only when their confidence gap is <= one window.
Trim HALF a window from BOTH ends of EACH plateau before measuring raw PCM RMS.
No RMS normalization, denoising, spectral weighting or DC subtraction of those
measured excerpts. Unequal-duration plateau ratios are recorded levels, NOT
injected mechanical energy, attack energy or calibrated playing velocity.
Dominant-peak agreement excludes fundamental estimates when an overtone wins;
report both masks without choosing the result that favors a model. Coverage is
incomplete and the 42.7 ms analysis window makes event boundaries uncertain.

Whole-file SHA256 values below are from the published files.csv. This tool
verifies both exact external WAVs, never downloads or vendors audio, and does
not claim verification of the 6.75 GB archive's published MD5. Historical source
and selection hashes are provenance commitments, not files needed at runtime.
The stdlib PCM24 decoder is numerically identical to the old SciPy int32/2**31
reader. Silent spectra now yield invalid estimates, degenerate interpolation
uses zero offset, and nonfinite trace values serialize as JSON null. Selected
recording results are unchanged. NumPy and SciPy are existing project
dependencies; no plotting dependency is required.

  python3 Tools/BenchmarkHammerOnNotes.py --audio-dir /path/to/audio \
      --output /tmp/acustra-hammer-report.json
  python3 Tools/BenchmarkHammerOnNotes.py --self-test

Output must be new. One JSON retains estimates, masks, measured sample bounds,
source/tool hashes and dependency versions. Never commit the source recordings.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import platform
import wave

import numpy as np
import scipy
from scipy.signal import find_peaks


SOURCE = "https://zenodo.org/records/10159492"
SR, N, HOP, SECONDS = 48000, 2048, 96, 10
FMIN, FMAX = 150, 1500
WINDOW, TRIM = N / SR, N / SR / 2
TARGETS = (
    {"dynamic_label": "p", "playing_dynamics": "piano",
     "file": "acoustic_guitar_pitched_allstring1_hammeron_p_GiaFer_20200814.wav",
     "sha256": "7db78e20ec91b762f247ba587622ed09ccc14a6748a71c57ca307c222b912cc6",
     "bytes": 6210730, "frames": 2070160},
    {"dynamic_label": "f", "playing_dynamics": "forte",
     "file": "acoustic_guitar_pitched_allstring1_hammeron_f_GiaFer_20200814.wav",
     "sha256": "d590ba319e44c363a19a12be93da43120c32f265becd2854ad6d23df5c15c51c",
     "bytes": 6279274, "frames": 2093008},
)
SOURCE_METADATA_SHA256 = {
    "files.csv": "92be5a356dfa9c2e3e43b4c5e4a2e2b3c7cbbdb2e0eacced66fcef1ba35410e1",
    "instruments.csv": "5337ef4590b93e082ea5c1a35ec4eb119e6151d9e034819ef13c2bd03047d41d",
    "expressive_techniques.csv": "bb709bc8e13b591d350529fd6db36811da12b768a2bed0aa3458751e6a004582",
    "note_labels.csv": "75502d20e5149641eb4d3449240413673ace6885a5c822df38760df422e98fa1",
}
AUDIT_LINEAGE_SHA256 = {
    "selection-before-audio.json": "a1de0423cab4e18f31c3718568786c5edb6e6aabf747ab32ef1e9eae6fe0ffbd",
    "pitch_trace.py": "78fca6c63fbf33d19556d03557387f9fc7992578b9abba55c6229c6908168727",
    "plateau_ratio.py": "98bc1055d87f762d8f7b3a0da97cc075076a258f22a564da804ef1d4688a3fb5",
    "plateau-rms-ratios.json": "fed1c9719e220b7f1e84ceb220311181c3fc27deb3e2acd81bab8e4130e198f6",
}
METHODS = {
    "dominant_fft_agreement": "dominant_fft_peak_hz",
    "six_harmonic_sum_fft_agreement": "harmonic_sum_fft_hz",
}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def decode_pcm24(raw: bytes) -> np.ndarray:
    octets = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3).astype(np.int32)
    values = octets[:, 0] | (octets[:, 1] << 8) | (octets[:, 2] << 16)
    values = (values ^ 0x800000) - 0x800000
    return values.astype(np.float64) / 2**23


def read_target(path: Path, target: dict) -> np.ndarray:
    if path.stat().st_size != target["bytes"] or digest(path) != target["sha256"]:
        raise ValueError(f"published file size/SHA256 mismatch: {path}")
    with wave.open(str(path), "rb") as wav:
        if (wav.getnchannels(), wav.getsampwidth(), wav.getframerate(),
                wav.getnframes(), wav.getcomptype()) != (1, 3, SR, target["frames"], "NONE"):
            raise ValueError(f"expected published mono 48 kHz PCM24 dimensions: {path}")
        raw = wav.readframes(SECONDS * SR)
    if len(raw) != SECONDS * SR * 3:
        raise ValueError(f"truncated first-ten-second excerpt: {path}")
    return decode_pcm24(raw)


def parabolic_offset(values: np.ndarray) -> float:
    denominator = values[0] - 2 * values[1] + values[2]
    return float(.5 * (values[0] - values[2]) / denominator) if denominator else 0.


def spectral_frequency(magnitude: np.ndarray, index: int) -> float:
    if magnitude[index] == 0:
        return np.nan
    offset = parabolic_offset(np.log(np.maximum(magnitude[index-1:index+2], 1e-30)))
    return (index + offset) * SR / 32768


def pitch_trace(x: np.ndarray) -> list[dict]:
    frames = []
    lags = np.arange(int(SR / FMAX), int(SR / FMIN) + 1)
    frequency = np.fft.rfftfreq(32768, 1 / SR)
    bins = np.where((frequency >= FMIN) & (frequency <= FMAX))[0]
    window = np.hanning(N)
    # Preserve the historical strict endpoint: start == len(x)-N is excluded.
    for start in range(0, len(x) - N, HOP):
        y = x[start:start+N].copy()
        y -= y.mean()
        level = np.sqrt(np.mean(y*y))
        spectrum = np.fft.rfft(y, n=4096)
        correlation = np.fft.irfft(spectrum * spectrum.conj(), n=4096)[:N]
        squares = np.r_[0, np.cumsum(y*y)]
        rho = correlation[lags] / np.sqrt(np.maximum(
            squares[N-lags] * (squares[N] - squares[lags]), 1e-40))
        peaks, _ = find_peaks(rho)
        if len(peaks):
            candidates = peaks[rho[peaks] >= max(.80, float(rho[peaks].max()) - .02)]
            index = int(candidates[0] if len(candidates) else peaks[np.argmax(rho[peaks])])
            hz = SR / (lags[index] + parabolic_offset(rho[index-1:index+2]))
            confidence = float(rho[index])
        else:
            hz, confidence = np.nan, 0.

        magnitude = np.abs(np.fft.rfft(y * window, n=32768))
        dominant = spectral_frequency(magnitude, int(bins[np.argmax(magnitude[bins])]))
        peaks, _ = find_peaks(magnitude[bins], height=magnitude[bins].max() * .01)
        peak_bins = bins[peaks]
        scores = []
        for peak in peak_bins:
            harmonics = np.arange(1, 7) * peak
            values = np.maximum.reduce([
                magnitude[np.minimum(harmonics + delta, len(magnitude)-1)]
                for delta in (-1, 0, 1)])
            scores.append(float(np.square(values).sum()))
        harmonic = (spectral_frequency(magnitude, int(peak_bins[np.argmax(scores)]))
                    if scores else dominant)
        frames.append({"time_center_seconds": (start + .5*N) / SR,
                       "correlation_hz": float(hz), "correlation_peak": confidence,
                       "dominant_fft_peak_hz": float(dominant),
                       "harmonic_sum_fft_hz": float(harmonic), "rms": float(level)})
    return frames


def segments(frames: list[dict], fft_column: str) -> tuple[list[dict], dict, dict]:
    times = np.array([r["time_center_seconds"] for r in frames])
    hz = np.array([r["correlation_hz"] for r in frames])
    fft = np.array([r[fft_column] for r in frames])
    rho = np.array([r["correlation_peak"] for r in frames])
    rms = np.array([r["rms"] for r in frames])
    midi = 69 + 12 * np.log2(hz / 440)
    rounded = np.rint(midi)
    cents = 1200 * np.log2(hz / fft)
    voiced = (rho >= .85) & (rms >= rms.max() * 10**(-45 / 20)) & np.isfinite(hz)
    agreement = voiced & (abs(cents) <= 35)
    good = agreement & (abs(midi - rounded) < .25)
    output, start = [], None
    for i in range(len(frames) + 1):
        valid = i < len(frames) and good[i]
        if start is not None and (not valid or rounded[i] != rounded[start]):
            if times[i-1] - times[start] >= .025:
                part = slice(start, i)
                output.append({
                    "begin_window_center_seconds": float(times[start]),
                    "end_window_center_seconds": float(times[i-1]),
                    "midi_semitone_estimate": int(rounded[start]),
                    "minimum_correlation": float(rho[part].min()),
                    "median_correlation": float(np.median(rho[part])),
                    "median_absolute_method_difference_cents": float(np.median(abs(cents[part]))),
                    "maximum_absolute_method_difference_cents": float(abs(cents[part]).max()),
                    "frames": i-start,
                })
            start = None
        if valid and start is None:
            start = i
    return output, {
        "total_analysis_frames": len(frames),
        "correlation_voiced_frames": int(voiced.sum()),
        "method_agreement_frames": int(agreement.sum()),
        "method_agreement_fraction_of_all_frames": float(agreement.mean()),
        "semitone_agreement_frames": int(good.sum()),
        "retained_plateau_frames": sum(s["frames"] for s in output),
        "retained_plateaus": len(output),
    }, {"correlation_voiced": voiced.tolist(), "method_agreement": agreement.tolist(),
        "semitone_agreement": good.tolist()}


def measured_window(plateau: dict, x: np.ndarray) -> dict | None:
    begin = int(np.ceil((plateau["begin_window_center_seconds"] + TRIM) * SR))
    end = int(np.floor((plateau["end_window_center_seconds"] - TRIM) * SR))
    if end <= begin:
        return None
    if not 0 <= begin < end <= len(x):
        raise ValueError("trimmed plateau falls outside the selected excerpt")
    level = float(np.sqrt(np.mean(x[begin:end] ** 2)))
    if level == 0:
        raise ValueError("a retained pitch plateau has zero measured RMS")
    return dict(plateau, measured_sample_start_inclusive=begin,
                measured_sample_end_exclusive=end,
                measured_begin_seconds=begin/SR, measured_end_seconds=end/SR,
                measured_duration_seconds=(end-begin)/SR,
                rms_full_scale=level, rms_dbfs=float(20*np.log10(level)))


def analyse(frames: list[dict], x: np.ndarray, fft_column: str) -> dict:
    plateaus, coverage, masks = segments(frames, fft_column)
    pairs, rejected, upward = [], [], 0
    for index, (before, after) in enumerate(zip(plateaus, plateaus[1:])):
        if after["midi_semitone_estimate"] != before["midi_semitone_estimate"] + 1:
            continue
        upward += 1
        gap = after["begin_window_center_seconds"] - before["end_window_center_seconds"]
        b, a = measured_window(before, x), measured_window(after, x)
        if gap > WINDOW or b is None or a is None:
            rejected.append({"before_plateau_index": index,
                "before_midi": before["midi_semitone_estimate"],
                "after_midi": after["midi_semitone_estimate"],
                "between_plateaus_seconds": gap,
                "reason": "confidence gap exceeds analysis window" if gap > WINDOW
                          else "one or both plateaus empty after required trim"})
            continue
        ratio = a["rms_full_scale"] / b["rms_full_scale"]
        pairs.append({"before_plateau_index": index, "before": b, "after": a,
            "between_plateaus_seconds": gap,
            "estimated_transition_center_seconds": .5*(before["end_window_center_seconds"]
                                                        + after["begin_window_center_seconds"]),
            "after_over_before_rms_ratio": ratio,
            "after_minus_before_rms_db": float(20*np.log10(ratio))})
    values = np.array([p["after_minus_before_rms_db"] for p in pairs])
    coverage.update(adjacent_upward_semitone_candidates=upward,
                    retained_pairs=len(pairs), rejected_upward_candidates=len(rejected),
                    measured_before_duration_seconds=sum(p["before"]["measured_duration_seconds"] for p in pairs),
                    measured_after_duration_seconds=sum(p["after"]["measured_duration_seconds"] for p in pairs))
    return {"coverage": coverage, "frame_masks": masks,
        "summary_db": None if not len(values) else {"median": float(np.median(values)),
            "mean": float(values.mean()), "minimum": float(values.min()), "maximum": float(values.max())},
        "pairs": pairs, "rejected_upward_candidates": rejected,
        "all_confident_plateaus": plateaus}


def self_test() -> None:
    def require(condition: bool, message: str) -> None:
        if not condition:
            raise AssertionError(message)

    # PCM24 sign and full-scale conversion, independently specified byte values.
    raw = bytes.fromhex("000000 ffff7f 000080 ffffff 010000")
    require(np.array_equal(decode_pcm24(raw), np.array([0, 8388607, -8388608, -1, 1]) / 2**23),
            "PCM24 signed full-scale decoding")
    time = np.arange(int(.4 * SR)) / SR
    dominant_second = np.sin(2*np.pi*440*time) + 2*np.sin(2*np.pi*880*time)
    frames = pitch_trace(dominant_second)
    first = analyse(frames, dominant_second, METHODS["dominant_fft_agreement"])
    second = analyse(frames, dominant_second, METHODS["six_harmonic_sum_fft_agreement"])
    require(first["coverage"]["method_agreement_frames"] == 0,
            "dominant second harmonic must remain an octave disagreement")
    require(second["coverage"]["method_agreement_fraction_of_all_frames"] > .95
            and all(p["midi_semitone_estimate"] == 69 for p in second["all_confident_plateaus"]),
            "harmonic sum must recover the present 440 Hz fundamental independently")

    # A known one-semitone transition with half amplitude, then the same notes
    # separated by silence. Test measured audio and rejection, not just masks.
    first_tone = .2 * np.sin(2*np.pi*440*time)
    next_tone = .1 * np.sin(2*np.pi*(440*2**(1/12))*time)
    x = np.r_[first_tone, next_tone]
    frames = pitch_trace(x)
    for column in METHODS.values():
        result = analyse(frames, x, column)
        require(result["coverage"]["retained_pairs"] == 1, "known semitone pair missing")
        pair = result["pairs"][0]
        require(abs(pair["after_minus_before_rms_db"] - 20*np.log10(.5)) < .03,
                "trimmed recorded amplitude ratio must be -6.02 dB")
        require(pair["before"]["midi_semitone_estimate"] == 69
                and pair["after"]["midi_semitone_estimate"] == 70
                and abs(pair["estimated_transition_center_seconds"] - .4) < WINDOW,
                "inferred transition pitch/time")
        for side in ("before", "after"):
            plateau = pair[side]
            require(plateau["measured_begin_seconds"] >= plateau["begin_window_center_seconds"] + TRIM
                    and plateau["measured_end_seconds"] <= plateau["end_window_center_seconds"] - TRIM,
                    "RMS must exclude half a window at each plateau end")
    x = np.r_[first_tone, np.zeros(int(.2*SR)), next_tone]
    frames = pitch_trace(x)
    for column in METHODS.values():
        result = analyse(frames, x, column)
        require(result["coverage"]["retained_pairs"] == 0
                and any(p["reason"] == "confidence gap exceeds analysis window"
                        for p in result["rejected_upward_candidates"]),
                "silence must not be bridged into a measured transition")
    print("Hammer-on recording audit self-test passed")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--audio-dir", type=Path)
    parser.add_argument("--output", type=Path, help="new JSON report file")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        if args.audio_dir or args.output:
            parser.error("--self-test does not take input/output paths")
        self_test()
        return
    if args.audio_dir is None or args.output is None:
        parser.error("--audio-dir and --output are required")
    if args.output.exists():
        parser.error(f"refusing to overwrite {args.output}")
    source_hash = digest(Path(__file__))
    # Verify both sources before analysing either; no partial report on failure.
    signals = [read_target(args.audio_dir / target["file"], target) for target in TARGETS]
    rows = []
    for target, x in zip(TARGETS, signals):
        frames = pitch_trace(x)
        analyses = {label: analyse(frames, x, column) for label, column in METHODS.items()}
        serial_frames = [{k: value if np.isfinite(value) else None for k, value in frame.items()}
                         for frame in frames]
        rows.append(dict(target, path=str((args.audio_dir / target["file"]).resolve()),
                         published_sha256_verified=True, excerpt_sample_start_inclusive=0,
                         excerpt_sample_end_exclusive=SECONDS*SR, sample_rate=SR,
                         channels=1, bits_per_sample=24, frame_estimates=serial_frames,
                         analyses=analyses))
    report = {
        "source": SOURCE, "license": "CC-BY-4.0",
        "instrument": "Taylor 114CE, LR Baggs Anthem internal pickup",
        "guitar_id": 2, "player_id": 3, "recording_date": "2020-08-14",
        "source_string_identifier": "allstring1 (high E4 from companion authored pitch labels)",
        "duration_per_file_seconds": SECONDS, "analysis_window_seconds": WINDOW,
        "hop_seconds": HOP/SR, "required_trim_each_end_seconds": TRIM,
        "maximum_plateau_gap_seconds": WINDOW, "estimator_frequency_range_hz": [FMIN, FMAX],
        "protocol_and_limitations": __doc__,
        "historical_source_metadata_sha256": SOURCE_METADATA_SHA256,
        "historical_audit_lineage_sha256": AUDIT_LINEAGE_SHA256,
        "whole_archive_verified": False, "helper_sha256": source_hash,
        "versions": {"python": platform.python_version(), "numpy": np.__version__, "scipy": scipy.__version__},
        "rows": rows,
    }
    if digest(Path(__file__)) != source_hash:
        raise ValueError("audit source changed during analysis; rerun with a fixed tool")
    payload = json.dumps(report, indent=2, allow_nan=False) + "\n"
    with args.output.open("x") as output:
        output.write(payload)
    for row in rows:
        for method, result in row["analyses"].items():
            print(row["dynamic_label"], method, json.dumps(result["coverage"]), json.dumps(result["summary_db"]))
    print(args.output)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, wave.Error) as error:
        raise SystemExit(str(error)) from error
