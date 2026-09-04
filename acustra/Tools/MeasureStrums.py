#!/usr/bin/env python3
"""Measure strum timing and level statistics from GuitarSet's hex pickup.

The input is GuitarSet (Xi, Bittner, Pauwels, Ye, Bello, ISMIR 2018,
Zenodo 3371780, CC BY 4.0): six players' comping and soloing takes on an
Ubertar hex pickup (bleed removed by KAMIR; per-coil sensitivity is
uncalibrated, so an absolute level cannot be compared across strings) with
JAMS ``note_midi`` annotations per string (``annotation_metadata.data_source``
0 = low E .. 5 = high e), themselves onset/offset-detected on the hex
channels. This tool re-derives strum events from those onsets rather than
trusting a single detector: a same-stroke group is 3 or more distinct
strings whose annotated onsets fall within a clustering window. "Comp"
(comping) tracks are used; "solo" tracks rarely strum.

The window itself is derived from the tracks, not chosen: GuitarSet's comp
filenames carry their annotated tempo (``<style>-<bpm>-<key>_comp``, e.g.
``01_Rock2-142-D_comp``), and no chord in 4/4 comping is voiced faster than a
sixteenth note, so a window at or under the shortest sixteenth note among the
tracks used cannot merge two distinct strokes into one. STRUM_WINDOW is set
at runtime to that bound (main() takes the minimum sixteenth-note duration,
15/bpm seconds, over the tracks actually loaded), not hand-picked.

Two measurements come out clean, and one does not carry across:

  * Inter-string onset intervals within a stroke, and the pooled 10-90%
    traversal speed at the shipping 10.8 mm steel saddle spacing -- these
    resolve far above the ~1 ms onset-detector floor (median interval
    7 ms, 5% below 1 ms) and show no resolvable dependence on stroke level
    (plain Pearson r of stroke speed against each stroke's mean level in dB,
    about 0.03; that mean level is averaged across strings on the hex
    pickup's uncalibrated per-coil sensitivity, which biases a cross-string
    correlation toward zero, so this null is weaker evidence than a
    same-channel measurement -- the conservative reading, keeping the
    by-ear velocity map rather than fitting one, is unaffected either way),
    so AcustraEngine's strumDelaySamples keeps its by-ear functional form
    but its two speed endpoints are re-pinned to the measured range.
  * Stroke-to-stroke variability, from runs of 3+ consecutive strokes that
    repeat one chord and direction within a 2 s gap (a rhythmic strumming
    pattern): a stroke's total span (10-90% traversal time) and a single
    string's own level both vary from repeat to repeat even though the
    hand's intent does not change. Per-coil sensitivity is a fixed gain per
    channel, so it cancels out of a same-string, same-channel comparison
    across repeats (unlike a cross-string comparison within one stroke,
    which this tool does not use for a level bound). The pooled deviation
    (each repeat's value minus its own run's mean, pooled over every run --
    for span, as a fraction of that run's own mean span, since it is the
    pick's speed that repeats, and a stroke's whole span scales with it)
    gives AcustraEngine's per-stroke pick-speed and per-string level jitter
    their bounds (Source/DSP/AcustraEngine.cpp, beginStrum and
    initialisePluck's strumMember path).
  * A real repeated stroke's own waveform does NOT stay highly correlated
    with its predecessor: two separate strikes of the same strings have no
    shared phase reference, so the raw-sample Pearson correlation of the
    first 250 ms (summed across strings) of two consecutive same-direction
    strokes centres near zero (median about -0.02) with a wide spread. That
    is the ceiling AcustraEngine's own repeated strums must fall under.

Requires numpy and soundfile. Usage:

    python3 Tools/MeasureStrums.py --annotation-zip annotation.zip \\
        --audio-dir /path/to/audio_hex-pickup_debleeded/ [--tracks track1,track2,...]

The audio directory holds individual ``<track>_hex_cln.wav`` files pulled
from GuitarSet's ``audio_hex-pickup_debleeded.zip`` (3.6 GB whole; pull only
the needed tracks, e.g. with a ranged zip reader, rather than the archive).
"""

from __future__ import annotations

import argparse
import collections
import json
import re
import zipfile
from pathlib import Path

import numpy as np
import soundfile as sf

STRING_SPACING = 0.0108  # m: the shipping steel saddle spacing (AcustraEngine.cpp)
LEVEL_WINDOW = 0.040    # s: matches the pluck-point measurement window (decisions.md)
CORR_WINDOW = 0.250     # s: matches the prior same-note repeat measurement (decisions.md)
REPEAT_GAP = 2.0        # s: a rest this short is still one rhythmic pattern

_TEMPO_RE = re.compile(r"-(\d+)-")


def sixteenth_note_window(stems) -> float:
    """The clustering window: at or under the shortest sixteenth note among
    the tracks used, so it cannot merge two distinct comped strokes into one
    group. GuitarSet's comp filenames carry their annotated tempo directly
    (``<style>-<bpm>-<key>_comp``)."""
    bpms = [int(_TEMPO_RE.search(stem).group(1)) for stem in stems]
    return min(15.0 / bpm for bpm in bpms)


def load_track(stem: str, ann_dir: Path, audio_dir: Path):
    with open(ann_dir / f"{stem}.jams") as f:
        jam = json.load(f)
    strings_data = []
    for annotation in jam["annotations"]:
        if annotation["namespace"] != "note_midi":
            continue
        string_index = int(annotation["annotation_metadata"]["data_source"])
        strings_data.append((string_index, annotation["data"]))
    strings_data.sort(key=lambda item: item[0])
    audio, rate = sf.read(str(audio_dir / f"{stem}_hex_cln.wav"), always_2d=True)
    return strings_data, audio, rate


def cluster(strings_data, use_offset: bool, window: float):
    events = []
    for string_index, notes in strings_data:
        for note in notes:
            t = note["time"] + note["duration"] if use_offset else note["time"]
            events.append((t, string_index))
    events.sort()
    groups, used = [], [False] * len(events)
    i = 0
    while i < len(events):
        if used[i]:
            i += 1
            continue
        group = [events[i]]
        seen = {events[i][1]}
        j = i + 1
        while j < len(events) and events[j][0] - group[0][0] <= window:
            if events[j][1] not in seen:
                group.append(events[j])
                seen.add(events[j][1])
            j += 1
        if len(group) >= 3:
            for g in group:
                used[events.index(g)] = True
            groups.append(sorted(group))
        i += 1
    return groups


def peak_level(audio, rate, t, string_index):
    start = int(t * rate)
    end = min(start + int(LEVEL_WINDOW * rate), audio.shape[0])
    if end <= start or string_index >= audio.shape[1]:
        return 0.0
    window = audio[start:end, string_index]
    return float(np.max(np.abs(window))) if len(window) else 0.0


def measure_strokes(stems, ann_dir, audio_dir, window: float):
    strokes = []
    release_intervals_ms = []
    for stem in stems:
        strings_data, audio, rate = load_track(stem, ann_dir, audio_dir)
        for group in cluster(strings_data, use_offset=True, window=window):
            times = [t for t, _ in group]
            release_intervals_ms.extend(
                (b - a) * 1000.0 for a, b in zip(times, times[1:]))
        for group in cluster(strings_data, use_offset=False, window=window):
            onsets = [t for t, _ in group]
            order = [s for _, s in group]
            span = onsets[-1] - onsets[0]
            levels = [peak_level(audio, rate, t, s) for t, s in group]
            if span <= 0 or min(levels) <= 0:
                continue
            direction = "down" if order[-1] > order[0] else "up"
            speed = (len(group) - 1) * STRING_SPACING / span
            start = int(onsets[0] * rate)
            end = min(start + int(CORR_WINDOW * rate), audio.shape[0])
            waveform = audio[start:end, :].sum(axis=1) if end > start else np.zeros(1)
            strokes.append(dict(
                stem=stem, t0=onsets[0], span=span, speed=speed, n=len(group),
                direction=direction, order=order, onsets=onsets,
                levels=dict(zip(order, levels)), waveform=waveform))
    return strokes, np.array(release_intervals_ms)


def repeated_pattern_runs(strokes):
    by_track = collections.defaultdict(list)
    for stroke in strokes:
        by_track[stroke["stem"]].append(stroke)
    for track_strokes in by_track.values():
        track_strokes.sort(key=lambda s: s["t0"])

    runs = []
    for track_strokes in by_track.values():
        run = [track_strokes[0]]
        for previous, current in zip(track_strokes, track_strokes[1:]):
            same_pattern = (current["t0"] - previous["t0"] < REPEAT_GAP
                             and current["direction"] == previous["direction"]
                             and set(current["order"]) == set(previous["order"]))
            if same_pattern:
                run.append(current)
            else:
                if len(run) >= 3:
                    runs.append(run)
                run = [current]
        if len(run) >= 3:
            runs.append(run)
    return runs


def report(strokes, release_intervals_ms, runs, window: float):
    print(f"clustering window: {window * 1000.0:.1f} ms")
    print(f"strokes clustered: {len(strokes)}")

    onset_gap_ms = np.array([
        (b - a) * 1000.0
        for stroke in strokes
        for a, b in zip(stroke["onsets"], stroke["onsets"][1:])
    ])
    print(f"inter-string onset intervals ms: n={len(onset_gap_ms)} "
          f"median={np.median(onset_gap_ms):.2f} "
          f"fraction<1ms={np.mean(onset_gap_ms < 1.0):.3f}")

    speeds = np.array([s["speed"] for s in strokes])
    levels = np.array([s["levels"][s["order"][0]] for s in strokes])
    print(f"pooled traversal speed 10/50/90%: "
          f"{np.percentile(speeds, [10, 50, 90])} m/s")
    print(f"speed vs. mean-string-level correlation: "
          f"{np.corrcoef(speeds, [20*np.log10(np.mean(list(s['levels'].values()))) for s in strokes])[0,1]:.3f}")

    print(f"release (offset) intervals ms: n={len(release_intervals_ms)} "
          f"median={np.median(release_intervals_ms):.2f} "
          f"fraction<1ms={np.mean(release_intervals_ms < 1.0):.3f}")

    print(f"repeated-pattern runs (>=3 repeats): {len(runs)}")

    span_devs_ms = []
    span_devs_relative = []
    for run in runs:
        spans = np.array([r["span"] for r in run]) * 1000.0
        span_devs_ms.extend(spans - spans.mean())
        span_devs_relative.extend((spans - spans.mean()) / spans.mean())
    span_devs_ms = np.array(span_devs_ms)
    span_devs_relative = np.array(span_devs_relative)
    print(f"pooled stroke-span deviation std: {span_devs_ms.std():.2f} ms "
          f"(n={len(span_devs_ms)})")
    # Each run's own span mean sets the pick speed for that repeat; a stroke
    # that draws a k-times-slower or -faster speed than its run's own mean
    # scales every string's delay by the same k, so the coherent quantity a
    # single per-stroke draw should match is this relative deviation, not
    # the absolute ms figure above (which is tied to this corpus's own
    # tempo and chord width).
    print(f"pooled stroke-span deviation, relative to its run's own mean "
          f"std: {span_devs_relative.std():.4f} (n={len(span_devs_relative)})")

    level_devs_db = []
    for run in runs:
        for string in set(run[0]["order"]):
            values = np.array([r["levels"][string] for r in run
                                if string in r["levels"] and r["levels"][string] > 0])
            if len(values) >= 3:
                db = 20 * np.log10(values)
                level_devs_db.extend(db - db.mean())
    level_devs_db = np.array(level_devs_db)
    print(f"pooled same-string level deviation std: {level_devs_db.std():.2f} dB "
          f"(n={len(level_devs_db)})")

    def envelope_free_corr(a, b):
        n = min(len(a), len(b))
        if n < 100 or np.std(a[:n]) == 0 or np.std(b[:n]) == 0:
            return None
        return float(np.corrcoef(a[:n], b[:n])[0, 1])

    corrs = []
    for run in runs:
        for a, b in zip(run, run[1:]):
            c = envelope_free_corr(a["waveform"], b["waveform"])
            if c is not None:
                corrs.append(c)
    corrs = np.array(corrs)
    print(f"real adjacent same-direction stroke correlation: n={len(corrs)} "
          f"mean={corrs.mean():.3f} median={np.median(corrs):.3f} "
          f"min={corrs.min():.3f} max={corrs.max():.3f}")


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                      formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--annotation-zip", type=Path, default=None,
                         help="GuitarSet's annotation.zip (not needed if "
                              "--extract-dir already holds every track's "
                              ".jams)")
    parser.add_argument("--audio-dir", type=Path, required=True,
                         help="directory of <track>_hex_cln.wav files")
    parser.add_argument("--tracks", type=str, default=None,
                         help="comma-separated track stems (default: every "
                              "*_hex_cln.wav found in --audio-dir)")
    parser.add_argument("--extract-dir", type=Path, default=Path("ann"),
                         help="where to extract the needed .jams files")
    args = parser.parse_args()

    if args.tracks:
        stems = args.tracks.split(",")
    else:
        stems = sorted(p.name[:-len("_hex_cln.wav")]
                        for p in args.audio_dir.glob("*_hex_cln.wav"))

    args.extract_dir.mkdir(parents=True, exist_ok=True)
    missing = [stem for stem in stems
               if not (args.extract_dir / f"{stem}.jams").exists()]
    if missing:
        if args.annotation_zip is None:
            raise SystemExit(f"--annotation-zip required to fetch: {missing}")
        with zipfile.ZipFile(args.annotation_zip) as zf:
            for stem in missing:
                zf.extract(f"{stem}.jams", args.extract_dir)

    window = sixteenth_note_window(stems)
    strokes, release_intervals_ms = measure_strokes(
        stems, args.extract_dir, args.audio_dir, window)
    runs = repeated_pattern_runs(strokes)
    report(strokes, release_intervals_ms, runs, window)


if __name__ == "__main__":
    main()
