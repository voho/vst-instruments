#!/usr/bin/env python3
"""Build Acustra's dependency-free embedded guitar sample payload.

The source audio stays at its native sample rate and is converted only to signed
16-bit PCM.  Each zone is encoded losslessly as block-adaptive Rice-coded
temporal differences, then the combined byte stream is written as fixed-width
ASCII85 so the generated C++ remains toolchain-portable.

Required at generation time: Python 3 and ffmpeg/ffprobe.  Runtime has no codec,
filesystem, or third-party dependency.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import struct
import subprocess
import sys
from array import array
from dataclasses import dataclass
from pathlib import Path


TARGET_LOW = 38
TARGET_HIGH = 84
BLOCK_FRAMES = 2048
EASTMAN_PREROLL_SECONDS = 0.020
EASTMAN_SLICE_SECONDS = 3.000
EASTMAN_END_FADE_SECONDS = 0.060
SHINY_END_FADE_SECONDS = 0.060
CPP_PART_BYTES = 24 * 1024 * 1024

EASTMAN = {
    "steel_plucked": (
        "plucked.opus",
        [
            ("plucked_E2", 40, 5.700, 82.900),
            ("plucked_A#2", 46, 64.120, 116.710),
            ("plucked_E3", 52, 102.840, 165.520),
            ("plucked_A#3", 58, 139.040, 233.020),
            ("plucked_E4", 64, 189.450, 330.620),
            ("plucked_B4", 71, 228.900, 494.200),
            ("plucked_F5", 77, 254.100, 699.650),
            ("plucked_B5", 83, 282.050, 989.050),
        ],
    ),
}

@dataclass
class Zone:
    bank: str
    name: str
    path: str
    low_key: int
    high_key: int
    root_midi: int
    root_hz: float
    sample_rate: int
    channels: int
    frames: int
    onset_frame: int
    peak: int
    pcm: array
    low_velocity: int = 1
    high_velocity: int = 127
    round_robin: int = 0
    packed_offset: int = 0
    packed_bytes: int = 0
    decoded_hash: int = 0
    source_hash: int = 0
    terminal_fade_frames: int = 0
    end_jump: int = 0
    physical_string_index: int | None = None
    captured_open_midi: int | None = None
    captured_fret: int | None = None


def run_bytes(command: list[str]) -> bytes:
    result = subprocess.run(command, check=True, stdout=subprocess.PIPE)
    return result.stdout


def probe(path: Path) -> tuple[int, int]:
    result = subprocess.run(
        [
            "ffprobe",
            "-v",
            "error",
            "-select_streams",
            "a:0",
            "-show_entries",
            "stream=sample_rate,channels",
            "-of",
            "json",
            str(path),
        ],
        check=True,
        stdout=subprocess.PIPE,
        text=True,
    )
    stream = json.loads(result.stdout)["streams"][0]
    return int(stream["sample_rate"]), int(stream["channels"])


def decode(path: Path, start: float | None = None, duration: float | None = None) -> tuple[int, int, array]:
    sample_rate, channels = probe(path)
    command = ["ffmpeg", "-v", "error", "-i", str(path)]
    if start is not None:
        command += ["-ss", f"{start:.9f}"]
    if duration is not None:
        command += ["-t", f"{duration:.9f}"]
    command += ["-map", "0:a:0", "-c:a", "pcm_s16le", "-f", "s16le", "-"]
    raw = run_bytes(command)
    if len(raw) % 2:
        raise RuntimeError(f"odd PCM byte count from {path}")
    pcm = array("h")
    pcm.frombytes(raw)
    if sys.byteorder != "little":
        pcm.byteswap()
    if duration is not None:
        wanted = round(duration * sample_rate) * channels
        if len(pcm) < wanted:
            pcm.extend(array("h", [0]) * (wanted - len(pcm)))
        elif len(pcm) > wanted:
            pcm = pcm[:wanted]
    return sample_rate, channels, pcm


def parse_sfz(path: Path) -> list[dict[str, str]]:
    regions: list[dict[str, str]] = []
    group: dict[str, str] = {}
    current: dict[str, str] | None = None
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.split("//", 1)[0].strip()
        if not line:
            continue
        if line == "<group>":
            if current:
                regions.append(group | current)
            group = {}
            current = None
            continue
        if line == "<region>":
            if current:
                regions.append(group | current)
            current = {}
            continue
        for key, value in re.findall(r"(\w+)=([^\s]+)", line):
            if current is None:
                group[key] = value
            else:
                current[key] = value
    if current:
        regions.append(group | current)
    return regions


def find_onset(pcm: array, channels: int) -> tuple[int, int]:
    peak = max(abs(value) for value in pcm)
    threshold = max(1, math.ceil(peak * 0.03))  # Same -30.46 dB anchor rule as source preparation.
    for frame in range(len(pcm) // channels):
        if max(abs(pcm[frame * channels + channel]) for channel in range(channels)) >= threshold:
            return frame, peak
    return 0, peak


def estimate_root_hz(pcm: array, sample_rate: int, channels: int, onset: int,
                     root_midi: int, settle_seconds: float = 0.0) -> float:
    """Constrained autocorrelation; the narrow search prevents octave errors."""
    nominal = 440.0 * 2.0 ** ((root_midi - 69) / 12.0)
    decimation = max(1, sample_rate // 12000)
    start = min(len(pcm) // channels,
                onset + round((0.035 + settle_seconds) * sample_rate))
    stop = min(len(pcm) // channels, start + round(0.55 * sample_rate))
    mono = [
        sum(pcm[frame * channels : frame * channels + channels]) / channels
        for frame in range(start, stop, decimation)
    ]
    if len(mono) < 256:
        return nominal
    mean = sum(mono) / len(mono)
    mono = [value - mean for value in mono]
    rate = sample_rate / decimation
    low_hz = nominal * 2.0 ** (-80.0 / 1200.0)
    high_hz = nominal * 2.0 ** (80.0 / 1200.0)
    low_lag = max(2, math.floor(rate / high_hz))
    high_lag = min(len(mono) // 3, math.ceil(rate / low_hz))
    scores: dict[int, float] = {}
    for lag in range(low_lag, high_lag + 1):
        count = len(mono) - lag
        cross = sum(mono[i] * mono[i + lag] for i in range(count))
        left = sum(mono[i] * mono[i] for i in range(count))
        right = sum(mono[i + lag] * mono[i + lag] for i in range(count))
        scores[lag] = cross / math.sqrt(max(1.0, left * right))
    best = max(scores, key=scores.get)
    fractional = float(best)
    if best - 1 in scores and best + 1 in scores:
        y0, y1, y2 = scores[best - 1], scores[best], scores[best + 1]
        denominator = y0 - 2.0 * y1 + y2
        if abs(denominator) > 1.0e-12:
            fractional += 0.5 * (y0 - y2) / denominator
    return rate / fractional


def estimate_spectral_root_hz(pcm: array, sample_rate: int, channels: int,
                              onset: int, root_midi: int) -> float:
    """Measure H1 after the force-dependent attack has settled."""
    nominal = 440.0 * 2.0 ** ((root_midi - 69) / 12.0)
    start = min(len(pcm) // channels,
                onset + round(0.82 * sample_rate))
    stop = min(len(pcm) // channels,
               start + round(2.0 * sample_rate))
    mono = [
        sum(pcm[frame * channels : frame * channels + channels]) / channels
        for frame in range(start, stop)
    ]
    if len(mono) < 256:
        return nominal
    mean = sum(mono) / len(mono)
    windowed = [
        (value - mean) * (0.5 - 0.5 * math.cos(
            2.0 * math.pi * index / (len(mono) - 1)))
        for index, value in enumerate(mono)
    ]

    def log_power(cents: float) -> float:
        frequency = nominal * 2.0 ** (cents / 1200.0)
        angle = -2.0 * math.pi * frequency / sample_rate
        step_real = math.cos(angle)
        step_imaginary = math.sin(angle)
        oscillator_real = 1.0
        oscillator_imaginary = 0.0
        real = 0.0
        imaginary = 0.0
        for value in windowed:
            real += value * oscillator_real
            imaginary += value * oscillator_imaginary
            oscillator_real, oscillator_imaginary = (
                oscillator_real * step_real - oscillator_imaginary * step_imaginary,
                oscillator_real * step_imaginary + oscillator_imaginary * step_real,
            )
        return math.log(max(1.0e-30, real * real + imaginary * imaginary))

    candidates = list(range(-30, 31, 5))
    powers = [log_power(cents) for cents in candidates]
    best = max(range(len(powers)), key=powers.__getitem__)
    cents = float(candidates[best])
    if 0 < best < len(powers) - 1:
        denominator = powers[best - 1] - 2.0 * powers[best] + powers[best + 1]
        if abs(denominator) > 1.0e-12:
            cents += 2.5 * (powers[best - 1] - powers[best + 1]) / denominator
    return nominal * 2.0 ** (cents / 1200.0)


class BitWriter:
    def __init__(self) -> None:
        self.data = bytearray()
        self.byte = 0
        self.used = 0

    def bit(self, value: int) -> None:
        self.byte |= (value & 1) << self.used
        self.used += 1
        if self.used == 8:
            self.data.append(self.byte)
            self.byte = self.used = 0

    def bits(self, value: int, count: int) -> None:
        self.byte |= (value & ((1 << count) - 1)) << self.used
        self.used += count
        while self.used >= 8:
            self.data.append(self.byte & 0xFF)
            self.byte >>= 8
            self.used -= 8

    def rice(self, value: int, k: int) -> None:
        quotient = value >> k
        self.byte |= ((1 << quotient) - 1) << self.used
        self.used += quotient + 1  # The terminating zero needs no OR.
        while self.used >= 8:
            self.data.append(self.byte & 0xFF)
            self.byte >>= 8
            self.used -= 8
        self.bits(value, k)

    def finish(self) -> bytes:
        if self.used:
            self.data.append(self.byte)
        return bytes(self.data)


def zigzag(value: int) -> int:
    return (value << 1) ^ (value >> 31)


def encode_component(values: list[int]) -> tuple[int, bytes]:
    previous = 0
    unsigned: list[int] = []
    for value in values:
        unsigned.append(zigzag(value - previous))
        previous = value
    k = min(range(21), key=lambda candidate: sum((value >> candidate) + 1 + candidate for value in unsigned))
    writer = BitWriter()
    for value in unsigned:
        writer.rice(value, k)
    return k, writer.finish()


def encode_zone(zone: Zone) -> bytes:
    result = bytearray()
    for first in range(0, zone.frames, BLOCK_FRAMES):
        count = min(BLOCK_FRAMES, zone.frames - first)
        result += struct.pack("<H", count)
        components: list[list[int]] = []
        left = [zone.pcm[(first + frame) * zone.channels] for frame in range(count)]
        components.append(left)
        if zone.channels == 2:
            components.append(
                [zone.pcm[(first + frame) * 2 + 1] - left[frame] for frame in range(count)]
            )
        for values in components:
            k, encoded = encode_component(values)
            result += struct.pack("<BI", k, len(encoded))
            result += encoded
    return bytes(result)


class BitReader:
    def __init__(self, data: bytes) -> None:
        self.data = data
        self.position = 0

    def bit(self) -> int:
        if self.position >= len(self.data) * 8:
            raise RuntimeError("truncated Rice block")
        value = (self.data[self.position // 8] >> (self.position % 8)) & 1
        self.position += 1
        return value

    def bits(self, count: int) -> int:
        return sum(self.bit() << bit for bit in range(count))


def decode_zone(data: bytes, frames: int, channels: int) -> array:
    output = array("h", [0]) * (frames * channels)
    position = 0
    frame = 0
    while frame < frames:
        if position + 2 > len(data):
            raise RuntimeError("truncated Rice zone")
        count = struct.unpack_from("<H", data, position)[0]
        position += 2
        if not count or frame + count > frames:
            raise RuntimeError("bad Rice block size")
        for component in range(channels):
            if position + 5 > len(data):
                raise RuntimeError("truncated Rice component")
            k, byte_count = struct.unpack_from("<BI", data, position)
            position += 5
            encoded = data[position : position + byte_count]
            position += byte_count
            reader = BitReader(encoded)
            previous = 0
            for local in range(count):
                quotient = 0
                while reader.bit():
                    quotient += 1
                unsigned = (quotient << k) | reader.bits(k)
                delta = (unsigned >> 1) ^ -(unsigned & 1)
                value = previous + delta
                previous = value
                index = (frame + local) * channels + component
                if component == 1:
                    value += output[index - 1]
                if not -32768 <= value <= 32767:
                    raise RuntimeError("decoded sample outside int16")
                output[index] = value
        frame += count
    if position != len(data):
        raise RuntimeError("trailing Rice bytes")
    return output


def fnv1a(pcm: array) -> int:
    value = 0xCBF29CE484222325
    for sample in pcm:
        unsigned = sample & 0xFFFF
        for byte in (unsigned & 0xFF, unsigned >> 8):
            value ^= byte
            value = (value * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return value


def terminal_max_jump(pcm: array, channels: int) -> int:
    frames = len(pcm) // channels
    first = max(1, frames - 64)
    result = 0
    for channel in range(channels):
        previous = pcm[(first - 1) * channels + channel]
        for frame in range(first, frames):
            current = pcm[frame * channels + channel]
            result = max(result, abs(current - previous))
            previous = current
        result = max(result, abs(previous))  # final decoded sample -> implicit silence
    return result


def fixed_ascii85(data: bytes) -> str:
    padded = data + b"\0" * ((-len(data)) % 4)
    output: list[str] = []
    for offset in range(0, len(padded), 4):
        value = int.from_bytes(padded[offset : offset + 4], "big")
        chars = ["!"] * 5
        for index in range(4, -1, -1):
            chars[index] = chr(33 + value % 85)
            value //= 85
        output.extend(chars)
    return "".join(output)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def aggregate_sha256(paths: list[Path], root: Path) -> str:
    digest = hashlib.sha256()
    for path in sorted(paths, key=lambda candidate: candidate.relative_to(root).as_posix()):
        digest.update(path.relative_to(root).as_posix().encode("utf-8"))
        digest.update(b"\0")
        with path.open("rb") as source:
            while block := source.read(1024 * 1024):
                digest.update(block)
    return digest.hexdigest()


def apply_terminal_fade(pcm: array, channels: int, fade_frames: int) -> None:
    fade_frames = min(fade_frames, len(pcm) // channels)
    for local_frame in range(fade_frames):
        phase = local_frame / max(1, fade_frames - 1)
        gain = 0.5 * (1.0 + math.cos(math.pi * phase))
        frame = len(pcm) // channels - fade_frames + local_frame
        for channel in range(channels):
            index = frame * channels + channel
            pcm[index] = round(pcm[index] * gain)


def build_nylon(source: Path) -> list[Zone]:
    sfz = next(source.glob("*.sfz"))
    zones: list[Zone] = []
    for region in parse_sfz(sfz):
        low = int(region.get("lokey", region.get("key", region.get("pitch_keycenter", "-1"))))
        high = int(region.get("hikey", region.get("key", region.get("pitch_keycenter", "-1"))))
        if high < TARGET_LOW or low > TARGET_HIGH:
            continue
        root = int(region.get("pitch_keycenter", region.get("key", str(low))))
        sample_path = source / region["sample"]
        sample_rate, channels, pcm = decode(sample_path)
        if channels != 1:
            raise RuntimeError(f"FreePats zone is not mono: {sample_path}")
        onset, peak = find_onset(pcm, channels)
        root_hz = estimate_root_hz(pcm, sample_rate, channels, onset, root)
        zones.append(
            Zone(
                "nylon",
                sample_path.stem,
                sample_path.name,
                max(TARGET_LOW, low),
                min(TARGET_HIGH, high),
                root,
                root_hz,
                sample_rate,
                channels,
                len(pcm),
                onset,
                peak,
                pcm,
            )
        )
    covered = {key for zone in zones for key in range(zone.low_key, zone.high_key + 1)}
    if covered != set(range(TARGET_LOW, TARGET_HIGH + 1)):
        raise RuntimeError(f"FreePats map does not cover MIDI 38-84: {sorted(set(range(38, 85)) - covered)}")
    return zones


def key_ranges(roots: list[int]) -> list[tuple[int, int]]:
    ranges: list[tuple[int, int]] = []
    low = TARGET_LOW
    for index, root in enumerate(roots):
        high = TARGET_HIGH if index + 1 == len(roots) else (root + roots[index + 1]) // 2
        ranges.append((low, high))
        low = high + 1
    return ranges


def build_eastman(source: Path) -> list[Zone]:
    zones: list[Zone] = []
    for bank, (filename, anchors) in EASTMAN.items():
        path = source / filename
        ranges = key_ranges([anchor[1] for anchor in anchors])
        for (name, root, pinned_onset, root_hz), (low, high) in zip(anchors, ranges):
            slice_start = pinned_onset - EASTMAN_PREROLL_SECONDS
            sample_rate, channels, pcm = decode(path, slice_start, EASTMAN_SLICE_SECONDS)
            if channels != 2:
                raise RuntimeError(f"Eastman source is not stereo: {path}")
            onset, peak = find_onset(pcm, channels)
            source_hash = fnv1a(pcm)
            fade_frames = round(EASTMAN_END_FADE_SECONDS * sample_rate)
            apply_terminal_fade(pcm, channels, fade_frames)
            zones.append(
                Zone(
                    bank,
                    name,
                    filename,
                    low,
                    high,
                    root,
                    root_hz,
                    sample_rate,
                    channels,
                    len(pcm) // channels,
                    onset,
                    peak,
                    pcm,
                    source_hash=source_hash,
                    terminal_fade_frames=fade_frames,
                )
            )
    return zones


def build_shiny(source: Path) -> tuple[list[Zone], list[Path]]:
    sfz = source / "Programs/acoustic.sfz"
    zones: list[Zone] = []
    included: list[Path] = []
    for region in parse_sfz(sfz):
        sample = region.get("sample", "").replace("\\", "/")
        match = re.fullmatch(r"acoustic/(.+)_vl([1-4])_rr([1-4])_1\.wav", sample)
        if not match:
            continue
        low = int(region["lokey"])
        high = int(region["hikey"])
        if high < TARGET_LOW or low > TARGET_HIGH:
            continue
        # SFZ's default pitch_keycenter is MIDI 60; acoustic.sfz relies on it
        # for the C4 groups, whose explicit key range is centred there.
        root = int(region.get("pitch_keycenter", str((low + high) // 2)))
        low_velocity = int(region.get("lovel", "1"))
        high_velocity = int(region.get("hivel", "127"))
        round_robin = int(match[3]) - 1
        sample_path = source / "Samples" / sample
        sample_rate, channels, pcm = decode(sample_path)
        if channels != 1:
            raise RuntimeError(f"Shinyguitar acoustic-mic zone is not mono: {sample_path}")
        onset, peak = find_onset(pcm, channels)
        # Calibrate from the settled sustain, leaving the real force-dependent
        # sharp attack and its glide intact.
        root_hz = estimate_spectral_root_hz(
            pcm, sample_rate, channels, onset, root)
        source_hash = fnv1a(pcm)
        fade_frames = round(SHINY_END_FADE_SECONDS * sample_rate)
        apply_terminal_fade(pcm, channels, fade_frames)
        zones.append(
            Zone(
                "steel_picked",
                sample_path.stem,
                sample_path.relative_to(source).as_posix(),
                max(TARGET_LOW, low),
                min(TARGET_HIGH, high),
                root,
                root_hz,
                sample_rate,
                channels,
                len(pcm) // channels,
                onset,
                peak,
                pcm,
                low_velocity,
                high_velocity,
                round_robin,
                source_hash=source_hash,
                terminal_fade_frames=fade_frames,
            )
        )
        included.append(sample_path)

    zones.sort(key=lambda zone: (
        zone.root_midi, zone.low_velocity, zone.round_robin))
    if len(zones) != 272:
        raise RuntimeError(f"expected 272 Shinyguitar acoustic sustains, found {len(zones)}")

    # A weak H1 can put a constrained fit on its search boundary. Replace only
    # those failed fits with the median of the other RRs in the same root/layer.
    for root in sorted({zone.root_midi for zone in zones}):
        nominal = 440.0 * 2.0 ** ((root - 69) / 12.0)
        for low_velocity in (1, 33, 65, 97):
            group = [zone for zone in zones
                     if zone.root_midi == root
                     and zone.low_velocity == low_velocity]
            cents = [1200.0 * math.log2(zone.root_hz / nominal) for zone in group]
            valid = sorted(value for value in cents if abs(value) < 29.5)
            if not valid:
                raise RuntimeError(f"all H1 fits hit the search boundary at root {root}, velocity {low_velocity}")
            midpoint = len(valid) // 2
            median = (valid[midpoint] if len(valid) % 2
                      else 0.5 * (valid[midpoint - 1] + valid[midpoint]))
            for zone, value in zip(group, cents):
                if abs(value) >= 29.5:
                    zone.root_hz = nominal * 2.0 ** (median / 1200.0)

    for zone in zones:
        nominal = 440.0 * 2.0 ** ((zone.root_midi - 69) / 12.0)
        cents = 1200.0 * math.log2(zone.root_hz / nominal)
        if abs(cents) >= 20.0:
            raise RuntimeError(
                f"Shinyguitar settled H1 outside 20-cent source bound: {zone.name} ({cents:.2f})")
    return zones, included


def cpp_float(value: float) -> str:
    return f"{value:.9g}f"


def write_cpp(path: Path, zones: list[Zone], packed: bytes, ascii85: str) -> list[Path]:
    records = []
    for zone in zones:
        location = ("" if zone.physical_string_index is None else
                    f", {zone.physical_string_index}, {zone.captured_open_midi}, {zone.captured_fret}")
        records.append(
            "    {"
            f"Bank::{''.join(part.title() for part in zone.bank.split('_'))}, "
            f'"{zone.name}", {zone.low_key}, {zone.high_key}, {zone.root_midi}, '
            f"{cpp_float(zone.root_hz)}, {zone.sample_rate}, {zone.channels}, {zone.frames}u, "
            f"{zone.onset_frame}u, {zone.peak}, {zone.low_velocity}, {zone.high_velocity}, "
            f"{zone.round_robin}, {zone.packed_offset}u, {zone.packed_bytes}u, "
            f"0x{zone.decoded_hash:016x}ULL, 0x{zone.source_hash:016x}ULL, "
            f"{zone.terminal_fade_frames}u, {zone.end_jump}{location}}},"
        )
    chunks = [ascii85[index : index + 16384] for index in range(0, len(ascii85), 16384)]
    chunk_definitions: list[str] = []
    for index, chunk in enumerate(chunks):
        lines = [chunk[offset : offset + 120] for offset in range(0, len(chunk), 120)]
        chunk_definitions.append(
            f'extern const char kAscii85Chunk{index}[] = R"AC{index}(' + "\n"
            + "\n".join(lines)
            + f'\n)AC{index}";'
        )
    chunk_table = "\n".join(f"    kAscii85Chunk{index}," for index in range(len(chunks)))
    for index, chunk in enumerate(chunks):
        if f')AC{index}"' in chunk:
            raise RuntimeError("raw-string delimiter collision")

    path.parent.mkdir(parents=True, exist_ok=True)
    for stale in path.parent.glob(f"{path.stem}-*{path.suffix}"):
        stale.unlink()

    declarations = "\n".join(
        f"extern const char kAscii85Chunk{index}[];" for index in range(len(chunks)))
    inline_definitions = len(ascii85) <= CPP_PART_BYTES
    body = f'''// Generated by GenerateSampleBank.py; do not hand-edit.
#include "GeneratedBankData.h"

namespace acustra::dense::generated {{

const PackedZoneRecord kZones[] = {{
{chr(10).join(records)}
}};

const std::size_t kZoneCount = sizeof(kZones) / sizeof(kZones[0]);
const std::size_t kPackedByteCount = {len(packed)}u;
{chr(10).join(chunk_definitions) if inline_definitions else declarations}
const char* const kAscii85Chunks[] = {{
{chunk_table}
}};
const std::size_t kAscii85ChunkCount = sizeof(kAscii85Chunks) / sizeof(kAscii85Chunks[0]);
const std::size_t kAscii85CharacterCount = {len(ascii85)}u;

}} // namespace acustra::dense::generated
'''
    path.write_text(body, encoding="ascii")
    outputs = [path]
    if not inline_definitions:
        chunks_per_part = max(1, (CPP_PART_BYTES - 1024 * 1024) // 16384)
        for part, first in enumerate(range(0, len(chunk_definitions), chunks_per_part)):
            part_path = path.with_name(f"{path.stem}-{part}{path.suffix}")
            definitions = "\n".join(chunk_definitions[first : first + chunks_per_part])
            part_path.write_text(
                "// Generated by GenerateSampleBank.py; do not hand-edit.\n"
                '#include "GeneratedBankData.h"\n\n'
                "namespace acustra::dense::generated {\n\n"
                + definitions
                + "\n\n} // namespace acustra::dense::generated\n",
                encoding="ascii",
            )
            outputs.append(part_path)
    return outputs


def write_manifest(path: Path, zones: list[Zone], packed: bytes, sources: dict[str, object]) -> None:
    raw_bytes = sum(len(zone.pcm) * 2 for zone in zones)
    content = {
        "format": "Acustra dense bank Rice-delta v1 + fixed ASCII85",
        "blockFrames": BLOCK_FRAMES,
        "targetMidi": [TARGET_LOW, TARGET_HIGH],
        "zoneCount": len(zones),
        "rawPcmBytes": raw_bytes,
        "packedBytes": len(packed),
        "compressionRatio": len(packed) / raw_bytes,
        "sources": sources,
        "zones": [
            {
                "bank": zone.bank,
                "name": zone.name,
                "source": zone.path,
                "keyRange": [zone.low_key, zone.high_key],
                "rootMidi": zone.root_midi,
                "measuredRootHz": zone.root_hz,
                "tuningCentsFrom12Tet": 1200.0
                * math.log2(zone.root_hz / (440.0 * 2.0 ** ((zone.root_midi - 69) / 12.0))),
                "sampleRate": zone.sample_rate,
                "channels": zone.channels,
                "frames": zone.frames,
                "durationSeconds": zone.frames / zone.sample_rate,
                "onsetFrame": zone.onset_frame,
                "velocityRange": [zone.low_velocity, zone.high_velocity],
                "roundRobin": zone.round_robin + 1,
                "rootPlaybackStartFrame": max(0, zone.onset_frame - round(0.004 * zone.sample_rate)),
                "rootNoteOnLatencyMs": min(4.0, 1000.0 * zone.onset_frame / zone.sample_rate),
                "peakInt16": zone.peak,
                "decodedFnv1a64": f"{zone.decoded_hash:016x}",
                "sourceFnv1a64BeforeTerminalFade": f"{zone.source_hash:016x}",
                "terminalHalfCosineFadeFrames": zone.terminal_fade_frames,
                "terminalHalfCosineFadeMs": 1000.0 * zone.terminal_fade_frames / zone.sample_rate,
                "terminalMaxAdjacentJumpInt16": zone.end_jump,
                "packedOffset": zone.packed_offset,
                "packedBytes": zone.packed_bytes,
                **({} if zone.physical_string_index is None else {
                    "physicalStringIndex": zone.physical_string_index,
                    "capturedOpenMidi": zone.captured_open_midi,
                    "capturedFret": zone.captured_fret,
                }),
            }
            for zone in zones
        ],
    }
    path.write_text(json.dumps(content, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    base = Path(__file__).resolve().parent
    parser.add_argument(
        "--freepats",
        type=Path,
        required=True,
    )
    parser.add_argument(
        "--eastman",
        type=Path,
        required=True,
    )
    parser.add_argument(
        "--shiny",
        type=Path,
        required=True,
    )
    parser.add_argument(
        "--output", type=Path,
        default=base.parent / "Source/DSP/SampleBank/BankData.cpp")
    parser.add_argument(
        "--manifest", type=Path,
        default=base.parent / "Assets/SampleBank/manifest.json")
    args = parser.parse_args()

    shiny_zones, shiny_files = build_shiny(args.shiny)
    zones = build_nylon(args.freepats) + shiny_zones + build_eastman(args.eastman)
    if len([zone for zone in zones if zone.bank == "nylon"]) != 41:
        raise RuntimeError("expected all 41 FreePats zones serving MIDI 38-84")
    if len([zone for zone in zones if zone.bank == "steel_picked"]) != 272:
        raise RuntimeError("expected all 272 Shinyguitar acoustic-mic sustains")
    if len([zone for zone in zones if zone.bank == "steel_plucked"]) != 8:
        raise RuntimeError("expected all 8 Eastman finger-plucked anchors")

    payload = bytearray()
    exact_velocity_ranges: dict[tuple[object, ...], list[tuple[int, int]]] = {}
    for zone in zones:
        if (not 1 <= zone.low_velocity <= zone.high_velocity <= 127
                or not 0 <= zone.round_robin < 4):
            raise RuntimeError(f"invalid velocity/RR mapping: {zone.name}")
        location = (zone.physical_string_index, zone.captured_open_midi,
                    zone.captured_fret)
        if any(value is None for value in location):
            if not all(value is None for value in location):
                raise RuntimeError(f"partial captured string location: {zone.name}")
        elif (not 0 <= zone.physical_string_index < 6
              or not 0 <= zone.captured_open_midi <= 127
              or not 0 <= zone.captured_fret <= 20
              or zone.captured_open_midi + zone.captured_fret != zone.root_midi
              or not zone.low_key <= zone.root_midi <= zone.high_key):
            raise RuntimeError(f"invalid captured string location: {zone.name}")
        if zone.physical_string_index is not None:
            identity = (zone.bank, zone.physical_string_index,
                        zone.captured_open_midi, zone.captured_fret,
                        zone.round_robin)
            ranges = exact_velocity_ranges.setdefault(identity, [])
            if any(zone.low_velocity <= high and low <= zone.high_velocity
                   for low, high in ranges):
                raise RuntimeError(
                    f"overlapping captured velocity range: {zone.name}")
            ranges.append((zone.low_velocity, zone.high_velocity))
        encoded = encode_zone(zone)
        if decode_zone(encoded, zone.frames, zone.channels) != zone.pcm:
            raise RuntimeError(f"lossless round-trip failed: {zone.name}")
        zone.packed_offset = len(payload)
        zone.packed_bytes = len(encoded)
        zone.decoded_hash = fnv1a(zone.pcm)
        if not zone.source_hash:
            zone.source_hash = zone.decoded_hash
        zone.end_jump = terminal_max_jump(zone.pcm, zone.channels)
        if zone.end_jump > 64:
            raise RuntimeError(f"unsafe natural-end jump ({zone.end_jump} int16): {zone.name}")
        payload += encoded

    ascii85 = fixed_ascii85(bytes(payload))
    generated_paths = write_cpp(args.output, zones, bytes(payload), ascii85)
    sources = {
        "freepats": {
            "page": "https://freepats.zenvoid.org/Guitar/acoustic-guitar.html",
            "creator": "roberto@zenvoid.org for FreePats",
            "version": "2019-06-18",
            "archive": "SpanishClassicalGuitar-SFZ+FLAC-20190618.7z",
            "archiveSha256": "903916921a21662d2237ade7f0e98e55de93cb7b86da219e4e10f4ad385b8f5e",
            "license": "CC0-1.0",
            "policy": "full native-rate files for every SFZ region intersecting MIDI 38-84",
        },
        "eastman": {
            "page": "https://github.com/0x4D44/ferrosintesis/tree/810318c92e33e31b36638b0ffa7ffc834a2ae6a2/samples/acoustic-guitar-eastman-e1d",
            "sourceCommit": "810318c92e33e31b36638b0ffa7ffc834a2ae6a2",
            "checkedRevision": "94edbcfef226986d6ac28330020bc301fa5207d9",
            "pluckedSha256": sha256(args.eastman / "plucked.opus"),
            "license": "CC0-1.0",
            "policy": "8 finger-plucked 3.000 s native-rate stereo slices beginning 20 ms before each pinned onset; 60 ms terminal half-cosine de-click fade",
            "rootPolicy": "late-sustain H1 fits used for settled tuning while retaining recorded attack glide",
        },
        "shinyguitar": {
            "page": "https://github.com/sfzinstruments/karoryfer.shinyguitar",
            "sourceCommit": "57243cca85277dbcc120ce17c6178032f93c80f3",
            "license": "CC0-1.0",
            "fileCount": len(shiny_files),
            "aggregateSha256": aggregate_sha256(shiny_files, args.shiny),
            "aggregateMethod": "SHA-256 over each UTF-8 POSIX path relative to the repository root, one NUL byte, then file bytes; paths sorted bytewise",
            "policy": "all 272 acoustic-microphone (_1) sustain WAVs referenced by Programs/acoustic.sfz and intersecting MIDI 38-84; native full duration, PCM16 conversion without level normalisation, 60 ms terminal half-cosine de-click fade",
            "mappingPolicy": "exact SFZ key and velocity ranges; deterministic four-way round robin",
            "rootPolicy": "H1 spectral peak over a 2.000 s Hann window beginning 820 ms after each detected onset; boundary fits fall back to the median of valid RRs for the same root/layer; playback retains the captured attack glide",
        },
    }
    write_manifest(args.manifest, zones, bytes(payload), sources)
    raw_bytes = sum(len(zone.pcm) * 2 for zone in zones)
    print(
        f"wrote {len(zones)} zones, {raw_bytes / 1048576:.2f} MiB raw, "
        f"{len(payload) / 1048576:.2f} MiB packed ({len(payload) / raw_bytes:.1%}), "
        f"{len(generated_paths)} generated C++ files"
    )


if __name__ == "__main__":
    main()
