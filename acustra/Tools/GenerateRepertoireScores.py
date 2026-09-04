#!/usr/bin/env python3
"""Generate the note tables for Acustra's two repertoire demonstrations.

The demonstrations play Francisco Tárrega (1852-1909).  His compositions are
in the public domain everywhere: he died in 1909, so every term measured from
the author's death expired in 1979 at the latest.  What this tool takes from
the files below is the composition -- which pitch sounds when, and for how
long -- and nothing else.  No engraving, fingering, barre indication, page
layout or editorial marking is read or reproduced, and none of those reach
the emitted header, which holds pitch, onset and length only.

    Recuerdos de la Alhambra, bars 1-12
        https://www.mutopiaproject.org/ftp/TarregaF/recuerdos/recuerdos.mid
        md5 b91ef372bc2f64e383be1a539f033f62
    Lágrima, bars 1-8
        https://www.mutopiaproject.org/ftp/TarregaF/lagrima-duo/lagrima-duo.mid
        md5 0b20164983fe93c99a0afc689d55f86b

Both were typeset by the Mutopia Project.  Mutopia declares the Lágrima file
public domain outright; the Recuerdos typesetting carries CC BY-SA 3.0, which
covers that edition's own engraving and not Tárrega's notes, and the engraving
is exactly what this tool does not take.

MIDI carries no dynamics here -- Mutopia's files are written at one velocity --
so the velocities are an authored performance and are marked as such wherever
they appear.  Two things are authored, both stated in the header they produce:

  * A role split.  The voice sounding on top at a note's onset carries the
    melody and is played a little harder than what lies under it.  In Lágrima
    that is the melody over its accompaniment; in Recuerdos the tremolo is on
    top and the thumb's arpeggio underneath it is the accompaniment.
  * A tremolo finger cycle.  Recuerdos' melody is a tremolo: the ring, middle
    and index fingers strike the same string in turn, and they do not strike
    equally.  Successive notes of one tremolo run get a small fixed cycle
    rather than one repeated velocity.

Neither is a physical constant and neither enters the model; they are the
performance the demonstration plays, in the same sense as the hand-authored
takes in RenderDemos.cpp.  Regenerate or verify with:

    python3 Tools/GenerateRepertoireScores.py --midi-dir /path/with/the/two/mid
    python3 Tools/GenerateRepertoireScores.py --midi-dir /path --check
"""

from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path

# The header is written beside this tool and compiled into RenderDemos.
HEADER = Path(__file__).resolve().parent / "RepertoireScores.h"

SOURCES = {
    "recuerdos": ("recuerdos.mid", "b91ef372bc2f64e383be1a539f033f62"),
    "lagrima": ("lagrima-duo.mid", "0b20164983fe93c99a0afc689d55f86b"),
}

# Bars kept, counted from the first sounding note; both pieces are in 3/4.
BARS = {"recuerdos": 12, "lagrima": 8}
BEATS_PER_BAR = 3

# Authored performance. Whatever sings on top is played a little harder than
# the line under it, and the three tremolo fingers do not strike equally, so a
# run of repeated melody notes cycles instead of repeating one value. The
# values sit inside the engine's own velocity map, which spans 0.1 to 1.0.
MELODY_VELOCITY = 0.58
ACCOMPANIMENT_VELOCITY = 0.48
TREMOLO_CYCLE = (0.54, 0.46, 0.50)
# The tremolo is written in thirty-seconds; nothing else in either excerpt is
# that short, so length alone separates it from the line underneath.
TREMOLO_MAX_TICKS_PER_QUARTER = 0.1875


def read_var(data: bytes, i: int) -> tuple[int, int]:
    value = 0
    while True:
        byte = data[i]
        i += 1
        value = (value << 7) | (byte & 0x7F)
        if not byte & 0x80:
            return value, i


def parse_midi(path: Path) -> tuple[int, int, list[tuple[int, int, int]]]:
    """Return (ticks per quarter, microseconds per quarter, [(on, off, note)])."""
    data = path.read_bytes()
    if data[:4] != b"MThd":
        raise SystemExit(f"{path}: not a MIDI file")
    header_length = struct.unpack(">I", data[4:8])[0]
    _, track_count, division = struct.unpack(">HHH", data[8:14])
    if division & 0x8000:
        raise SystemExit(f"{path}: SMPTE time division is not supported")
    i = 8 + header_length
    micros = 500000
    notes: list[tuple[int, int, int]] = []
    for _ in range(track_count):
        if data[i : i + 4] != b"MTrk":
            raise SystemExit(f"{path}: expected a track chunk")
        length = struct.unpack(">I", data[i + 4 : i + 8])[0]
        j, end = i + 8, i + 8 + length
        tick, running, sounding = 0, None, {}
        while j < end:
            delta, j = read_var(data, j)
            tick += delta
            status = data[j]
            if status & 0x80:
                running, j = status, j + 1
            else:
                status = running
            if status == 0xFF:
                meta = data[j]
                j += 1
                size, j = read_var(data, j)
                if meta == 0x51:
                    micros = int.from_bytes(data[j : j + size], "big")
                j += size
            elif status in (0xF0, 0xF7):
                size, j = read_var(data, j)
                j += size
            elif status & 0xF0 in (0x80, 0x90, 0xA0, 0xB0, 0xE0):
                first, second = data[j], data[j + 1]
                j += 2
                key = (status & 0x0F, first)
                if status & 0xF0 == 0x90 and second > 0:
                    sounding.setdefault(key, []).append(tick)
                elif status & 0xF0 == 0x80 or status & 0xF0 == 0x90:
                    if sounding.get(key):
                        notes.append((sounding[key].pop(0), tick, first))
            else:
                j += 1
        i = end
    return division, micros, sorted(notes)


def classify(notes, division):
    """Label each note tremolo, melody or accompaniment."""
    tremolo_ceiling = TREMOLO_MAX_TICKS_PER_QUARTER * division
    roles = []
    for on, off, pitch in notes:
        if (off - on) <= tremolo_ceiling:
            roles.append("tremolo")
            continue
        # Against everything that overlaps it, not just what has already
        # started: Recuerdos' opening bass is struck before the tremolo it
        # sits under, and it is not the melody.
        highest = max(n[2] for n in notes if n[0] < off and n[1] > on)
        roles.append("melody" if pitch == highest else "accompaniment")
    return roles


def velocities(notes, roles):
    """Authored, not measured: see the module docstring."""
    out = []
    run_pitch, run_index = None, 0
    for (on, off, pitch), role in zip(notes, roles):
        if role == "tremolo":
            if pitch != run_pitch:
                run_pitch, run_index = pitch, 0
            out.append(TREMOLO_CYCLE[run_index % len(TREMOLO_CYCLE)])
            run_index += 1
        else:
            run_pitch, run_index = None, 0
            out.append(MELODY_VELOCITY if role == "melody"
                       else ACCOMPANIMENT_VELOCITY)
    return out


def build(name: str, path: Path):
    division, micros, notes = parse_midi(path)
    if not notes:
        raise SystemExit(f"{path}: no notes")
    origin = notes[0][0]
    keep = origin + BARS[name] * BEATS_PER_BAR * division
    notes = [(on, off, pitch) for on, off, pitch in notes if on < keep]
    notes = [(on, min(off, keep), pitch) for on, off, pitch in notes]
    roles = classify(notes, division)
    velocity = velocities(notes, roles)
    rows = []
    for (on, off, pitch), role, level in zip(notes, roles, velocity):
        rows.append((round((on - origin) / division, 6),
                     round((off - on) / division, 6), pitch, level, role))
    return round(6e7 / micros, 3), rows


def emit(pieces) -> str:
    lines = [
        "// Generated by Tools/GenerateRepertoireScores.py; do not hand-edit.",
        "// Two pieces by Francisco Tárrega (1852-1909), whose compositions are",
        "// in the public domain. The tables hold the composition alone -- which",
        "// pitch sounds when, and for how long. No engraving, fingering or",
        "// editorial marking from any edition is reproduced here.",
        "// The velocities are an authored performance, not a measurement: a",
        "// thumb note is played harder than the fingers' line above it, and a",
        "// tremolo run cycles through three levels because the ring, middle and",
        "// index fingers do not strike a string equally. The generator's",
        "// docstring states both, and neither enters the model.",
        "",
        "#pragma once",
        "",
        "#include <array>",
        "",
        "namespace acustra::repertoire",
        "{",
        "// One note of a score: when it starts and how long it is held, both in",
        "// beats, the MIDI note, and the authored velocity.",
        "struct ScoreNote",
        "{",
        "    double startBeats;",
        "    double heldBeats;",
        "    int note;",
        "    float velocity;",
        "};",
        "",
    ]
    for name, (tempo, rows) in pieces.items():
        upper = name.upper()
        lines.append(f"// {name}: {len(rows)} notes, {BARS[name]} bars of 3/4 at "
                     f"{tempo:g} beats per minute.")
        lines.append(f"constexpr double {name}BeatsPerMinute = {tempo:g};")
        lines.append(f"constexpr std::array<ScoreNote, {len(rows)}> {name} {{{{")
        for start, held, pitch, level, role in rows:
            lines.append(f"    {{ {start:>9.6f}, {held:>9.6f}, {pitch:3d}, "
                         f"{level:.2f}f }}, // {role}")
        lines.append("}};")
        lines.append("")
    lines.append("} // namespace acustra::repertoire")
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--midi-dir", required=True, type=Path)
    parser.add_argument("--check", action="store_true",
                        help="verify the committed header instead of writing it")
    args = parser.parse_args()

    pieces = {}
    for name, (filename, digest) in SOURCES.items():
        path = args.midi_dir / filename
        if not path.exists():
            raise SystemExit(f"{path}: missing; see this tool's docstring for its URL")
        actual = hashlib.md5(path.read_bytes()).hexdigest()
        if actual != digest:
            raise SystemExit(f"{path}: md5 {actual}, expected {digest}")
        pieces[name] = build(name, path)

    text = emit(pieces)
    if args.check:
        if not HEADER.exists():
            print(f"{HEADER}: missing", file=sys.stderr)
            return 1
        if HEADER.read_text() != text:
            print(f"{HEADER}: differs from the generated table", file=sys.stderr)
            return 1
        counts = ", ".join(f"{n} {len(p[1])} notes" for n, p in pieces.items())
        print(f"{HEADER.name} matches: {counts}")
        return 0
    HEADER.write_text(text)
    counts = ", ".join(f"{n} {len(p[1])} notes" for n, p in pieces.items())
    print(f"wrote {HEADER} ({counts})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
