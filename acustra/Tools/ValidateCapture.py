#!/usr/bin/env python3
"""Validate an Acustra commissioned-capture delivery without modifying it.

The capture directory must contain ``capture.csv`` and a non-empty UTF-8
``session_metadata.md``; every WAV below it must be an accepted export named by
the manifest.  By default the manifest must cover the standard-tuning pilot
frets; ``--full`` requires every fret from 0 through 20.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import shutil
import struct
import sys
import tempfile
import wave
from dataclasses import dataclass
from pathlib import Path, PurePosixPath, PureWindowsPath


HEADER = (
    "sustain_path",
    "string_index",
    "open_midi",
    "fret",
    "dynamic",
    "round_robin",
)
SESSION_METADATA = "session_metadata.md"
STANDARD_OPEN_MIDI = (40, 45, 50, 55, 59, 64)
PILOT_FRETS = (0, 5, 12, 17, 20)
FULL_FRETS = tuple(range(21))
SAMPLE_RATE = 96_000
CHANNELS = 2
SAMPLE_WIDTH = 3
FRAME_BYTES = CHANNELS * SAMPLE_WIDTH
MIN_SUSTAIN_SECONDS = 10.0
MAX_SUSTAIN_SECONDS = 14.0


@dataclass(frozen=True)
class Capture:
    sustain_path: str
    string_index: int
    open_midi: int
    fret: int
    dynamic: int
    round_robin: int

    @property
    def identity(self) -> tuple[int, int, int, int]:
        return (self.string_index, self.fret, self.dynamic, self.round_robin)


@dataclass(frozen=True)
class WavReport:
    pcm_sha256: str


@dataclass(frozen=True)
class PcmFormat:
    format_tag: int
    channels: int
    sample_rate: int
    byte_rate: int
    block_align: int
    bits_per_sample: int


def safe_relative_wav(value: str) -> bool:
    if not value or "\0" in value or "\\" in value:
        return False
    parts = value.split("/")
    if any(part in ("", ".", "..") for part in parts):
        return False
    posix = PurePosixPath(value)
    windows = PureWindowsPath(value)
    return (not posix.is_absolute()
            and not windows.is_absolute()
            and not windows.drive
            and posix.suffix.lower() == ".wav")


def expected_identities(frets: tuple[int, ...]) -> set[tuple[int, int, int, int]]:
    return {
        (string_index, fret, dynamic, round_robin)
        for string_index in range(6)
        for fret in frets
        for dynamic in range(1, 5)
        for round_robin in range(1, 5)
    }


def summarize_identities(identities: set[tuple[int, int, int, int]]) -> str:
    ordered = sorted(identities)
    shown = ", ".join(map(str, ordered[:12]))
    if len(ordered) > 12:
        shown += f", ... ({len(ordered)} total)"
    return shown


def read_manifest(path: Path) -> tuple[list[Capture], list[str]]:
    errors: list[str] = []
    captures: list[Capture] = []
    try:
        with path.open("r", encoding="utf-8", newline="") as source:
            reader = csv.DictReader(source)
            if tuple(reader.fieldnames or ()) != HEADER:
                return [], [
                    f"{path}: header must be exactly {','.join(HEADER)}"
                ]
            for line, row in enumerate(reader, 2):
                if None in row or any(row[name] is None for name in HEADER):
                    errors.append(f"{path}:{line}: expected exactly {len(HEADER)} fields")
                    continue
                sustain = row["sustain_path"]
                if not safe_relative_wav(sustain):
                    errors.append(f"{path}:{line}: unsafe sustain_path {sustain!r}")
                try:
                    values = [
                        int(row[name])
                        for name in ("string_index", "open_midi", "fret",
                                     "dynamic", "round_robin")
                    ]
                except ValueError:
                    errors.append(f"{path}:{line}: mapping fields must be decimal integers")
                    continue
                string_index, open_midi, fret, dynamic, round_robin = values
                if not 0 <= string_index < len(STANDARD_OPEN_MIDI):
                    errors.append(f"{path}:{line}: string_index must be 0..5")
                elif open_midi != STANDARD_OPEN_MIDI[string_index]:
                    errors.append(
                        f"{path}:{line}: string {string_index} must use standard-tuning "
                        f"open MIDI {STANDARD_OPEN_MIDI[string_index]}, not {open_midi}")
                if not 0 <= fret <= 20:
                    errors.append(f"{path}:{line}: fret must be 0..20")
                if not 1 <= dynamic <= 4:
                    errors.append(f"{path}:{line}: dynamic must be 1..4")
                if not 1 <= round_robin <= 4:
                    errors.append(f"{path}:{line}: round_robin must be 1..4")
                if safe_relative_wav(sustain):
                    captures.append(Capture(
                        sustain, string_index, open_midi, fret, dynamic,
                        round_robin))
    except (OSError, UnicodeError, csv.Error) as error:
        errors.append(f"{path}: cannot read manifest: {error}")
    return captures, errors


def validate_mapping(captures: list[Capture], frets: tuple[int, ...]) -> list[str]:
    errors: list[str] = []
    identities: dict[tuple[int, int, int, int], int] = {}
    paths: dict[str, str] = {}
    for row, capture in enumerate(captures, 2):
        if capture.identity in identities:
            errors.append(
                f"capture.csv:{row}: duplicate identity {capture.identity}; "
                f"first seen on line {identities[capture.identity]}")
        else:
            identities[capture.identity] = row
        relative = capture.sustain_path
        if relative in paths:
            errors.append(
                f"capture.csv:{row}: sustain path {relative!r} is already used by "
                f"{paths[relative]}")
        else:
            paths[relative] = f"line {row} sustain"

    expected = expected_identities(frets)
    actual = set(identities)
    if missing := expected - actual:
        errors.append(f"capture.csv: missing identities: {summarize_identities(missing)}")
    if extra := actual - expected:
        errors.append(f"capture.csv: unexpected identities: {summarize_identities(extra)}")
    return errors


def validate_session_metadata(root: Path) -> list[str]:
    try:
        root = root.resolve(strict=True)
    except OSError as error:
        return [f"capture directory is missing: {error}"]
    path = root / SESSION_METADATA
    try:
        path = path.resolve(strict=True)
        path.relative_to(root)
    except (OSError, ValueError) as error:
        return [f"{SESSION_METADATA} escapes the capture directory or is missing: {error}"]
    if not path.is_file():
        return [f"{SESSION_METADATA} is not a regular file"]
    try:
        content = path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as error:
        return [f"{SESSION_METADATA} must be readable UTF-8: {error}"]
    if not content.strip():
        return [f"{SESSION_METADATA} must not be empty"]
    return []


def parse_riff(path: Path) -> tuple[PcmFormat | None, int | None, list[str]]:
    """Return the fmt fields and data size while checking the RIFF envelope."""
    errors: list[str] = []
    pcm_format: PcmFormat | None = None
    format_seen = False
    data_size: int | None = None
    try:
        file_size = path.stat().st_size
        with path.open("rb") as source:
            header = source.read(12)
            if len(header) != 12 or header[:4] != b"RIFF" or header[8:] != b"WAVE":
                return None, None, [f"{path}: not a classic RIFF/WAVE file"]
            riff_end = 8 + struct.unpack_from("<I", header, 4)[0]
            if riff_end != file_size:
                errors.append(
                    f"{path}: RIFF size declares {riff_end} bytes, file has {file_size}")
            limit = min(riff_end, file_size)
            position = 12
            while position < limit:
                if position + 8 > limit:
                    errors.append(f"{path}: truncated RIFF chunk header at byte {position}")
                    break
                source.seek(position)
                chunk_header = source.read(8)
                chunk_id = chunk_header[:4]
                chunk_size = struct.unpack_from("<I", chunk_header, 4)[0]
                content = position + 8
                chunk_end = content + chunk_size
                padded_end = chunk_end + (chunk_size & 1)
                if padded_end > limit:
                    errors.append(f"{path}: truncated {chunk_id!r} chunk")
                    break
                if chunk_id == b"fmt ":
                    if format_seen:
                        errors.append(f"{path}: multiple fmt chunks")
                    elif chunk_size < 16:
                        errors.append(f"{path}: fmt chunk is shorter than 16 bytes")
                    else:
                        source.seek(content)
                        pcm_format = PcmFormat(*struct.unpack("<HHIIHH", source.read(16)))
                    format_seen = True
                elif chunk_id == b"data":
                    if data_size is not None:
                        errors.append(f"{path}: multiple data chunks")
                    else:
                        data_size = chunk_size
                position = padded_end
    except OSError as error:
        errors.append(f"{path}: cannot read RIFF structure: {error}")
    if not format_seen:
        errors.append(f"{path}: missing fmt chunk")
    if data_size is None:
        errors.append(f"{path}: missing data chunk")
    return pcm_format, data_size, errors


def has_rail_sample(payload: bytes) -> bool:
    for rail in (b"\xff\xff\x7f", b"\x00\x00\x80"):
        position = payload.find(rail)
        while position >= 0:
            if position % SAMPLE_WIDTH == 0:
                return True
            position = payload.find(rail, position + 1)
    return False


def channel_is_silent(payload: bytes, channel: int) -> bool:
    first = channel * SAMPLE_WIDTH
    for byte in range(first, first + SAMPLE_WIDTH):
        lane = payload[byte::FRAME_BYTES]
        if lane.count(0) != len(lane):
            return False
    return True


def channels_are_identical(payload: bytes) -> bool:
    return all(
        payload[byte::FRAME_BYTES] == payload[byte + SAMPLE_WIDTH::FRAME_BYTES]
        for byte in range(SAMPLE_WIDTH)
    )


def inspect_wav(path: Path) -> tuple[WavReport | None, list[str]]:
    errors: list[str] = []
    pcm_format, declared_data_size, riff_errors = parse_riff(path)
    errors.extend(riff_errors)
    expected_format = PcmFormat(
        1, CHANNELS, SAMPLE_RATE, SAMPLE_RATE * FRAME_BYTES,
        FRAME_BYTES, SAMPLE_WIDTH * 8)
    if pcm_format is not None and pcm_format != expected_format:
        errors.append(
            f"{path}: fmt must be PCM 1, 2 channels, 96000 Hz, 576000 B/s, "
            f"6-byte frames and 24 bits; got {pcm_format}")

    try:
        with wave.open(str(path), "rb") as source:
            channels = source.getnchannels()
            sample_width = source.getsampwidth()
            sample_rate = source.getframerate()
            frames = source.getnframes()
            if source.getcomptype() != "NONE":
                errors.append(f"{path}: WAV must be uncompressed PCM")
            if channels != CHANNELS:
                errors.append(f"{path}: channel count must be 2, not {channels}")
            if sample_width != SAMPLE_WIDTH:
                errors.append(f"{path}: sample width must be 24-bit, not {sample_width * 8}-bit")
            if sample_rate != SAMPLE_RATE:
                errors.append(f"{path}: sample rate must be 96000 Hz, not {sample_rate}")
            frame_bytes = channels * sample_width
            payload = source.readframes(frames)
            trailing = source.readframes(1)
    except (OSError, EOFError, wave.Error) as error:
        errors.append(f"{path}: cannot decode WAV to EOF: {error}")
        return None, errors

    expected_bytes = frames * frame_bytes
    if len(payload) != expected_bytes or trailing:
        errors.append(
            f"{path}: PCM data is not exactly {frames} complete frames "
            f"({len(payload)} bytes read, expected {expected_bytes})")
    if declared_data_size is not None:
        if declared_data_size % max(1, frame_bytes):
            errors.append(f"{path}: data chunk is not a whole number of frames")
        if declared_data_size != len(payload):
            errors.append(
                f"{path}: data chunk declares {declared_data_size} bytes, "
                f"decoder returned {len(payload)}")
    if frames <= 0:
        errors.append(f"{path}: WAV contains no audio frames")
    if sample_rate > 0:
        duration = frames / sample_rate
        if not MIN_SUSTAIN_SECONDS <= duration <= MAX_SUSTAIN_SECONDS:
            errors.append(
                f"{path}: sustain file duration must be 10.0..14.0 s, "
                f"got {duration:.6f} s")

    if channels == CHANNELS and sample_width == SAMPLE_WIDTH and len(payload) % FRAME_BYTES == 0:
        if channel_is_silent(payload, 0):
            errors.append(f"{path}: close channel is digitally silent")
        if channel_is_silent(payload, 1):
            errors.append(f"{path}: room channel is digitally silent")
        if channels_are_identical(payload):
            errors.append(f"{path}: close and room channels are bit-identical")
        if has_rail_sample(payload):
            errors.append(f"{path}: PCM contains a signed 24-bit rail sample")

    return WavReport(hashlib.sha256(payload).hexdigest()), errors


def duplicate_pcm_errors(reports: list[tuple[str, WavReport]]) -> list[str]:
    errors: list[str] = []
    by_hash: dict[str, str] = {}
    for relative, report in reports:
        if previous := by_hash.get(report.pcm_sha256):
            errors.append(
                f"duplicate sustain PCM payload {report.pcm_sha256}: "
                f"{previous!r} and {relative!r}")
        else:
            by_hash[report.pcm_sha256] = relative
    return errors


def validate_session(
    root: Path,
    frets: tuple[int, ...],
) -> tuple[list[str], list[tuple[str, WavReport]], int]:
    errors: list[str] = []
    try:
        root = root.resolve(strict=True)
    except OSError as error:
        return [f"{root}: capture directory does not exist: {error}"], [], 0
    if not root.is_dir():
        return [f"{root}: capture path is not a directory"], [], 0

    errors.extend(validate_session_metadata(root))

    manifest = root / "capture.csv"
    try:
        manifest = manifest.resolve(strict=True)
        manifest.relative_to(root)
    except (OSError, ValueError) as error:
        return errors + [
            f"capture.csv escapes the capture directory or is missing: {error}"
        ], [], 0
    if not manifest.is_file():
        return errors + ["capture.csv is not a regular file"], [], 0

    captures, manifest_errors = read_manifest(manifest)
    errors.extend(manifest_errors)
    errors.extend(validate_mapping(captures, frets))
    listed = {
        capture.sustain_path
        for capture in captures
    }
    try:
        actual = {
            path.relative_to(root).as_posix()
            for path in root.rglob("*")
            if path.is_file() and path.suffix.lower() == ".wav"
        }
    except OSError as error:
        errors.append(f"{root}: cannot enumerate WAV files: {error}")
        actual = set()
    if unlisted := actual - listed:
        errors.append(f"unlisted WAV files: {', '.join(sorted(unlisted))}")
    if missing := listed - actual:
        errors.append(f"listed WAV files not found: {', '.join(sorted(missing))}")

    resolved_paths: dict[Path, str] = {}
    sustain_reports: list[tuple[str, WavReport]] = []
    for capture in sorted(captures, key=lambda item: item.identity):
        relative = capture.sustain_path
        candidate = root / PurePosixPath(relative)
        try:
            resolved = candidate.resolve(strict=True)
            resolved.relative_to(root)
        except (OSError, ValueError) as error:
            errors.append(f"{relative!r}: path escapes capture directory or is missing: {error}")
            continue
        if not resolved.is_file():
            errors.append(f"{relative!r}: capture path is not a regular file")
            continue
        if resolved in resolved_paths:
            errors.append(
                f"{relative!r}: resolves to the same file as {resolved_paths[resolved]!r}")
            continue
        resolved_paths[resolved] = relative
        report, wav_errors = inspect_wav(resolved)
        errors.extend(wav_errors)
        if report is not None:
            sustain_reports.append((relative, report))

    errors.extend(duplicate_pcm_errors(sustain_reports))
    return errors, sustain_reports, len(captures)


def write_test_wav(path: Path, sample_rate: int = SAMPLE_RATE,
                   rail: bool = False) -> None:
    frames = sample_rate * 10
    ordinary = b"\x01\x00\x00\x02\x00\x00"
    first = b"\xff\xff\x7f\x02\x00\x00" if rail else ordinary
    payload = first + ordinary * (frames - 1)
    with wave.open(str(path), "wb") as output:
        output.setnchannels(CHANNELS)
        output.setsampwidth(SAMPLE_WIDTH)
        output.setframerate(sample_rate)
        output.writeframes(payload)


def patch_test_fmt(path: Path, *, byte_rate: int | None = None,
                   block_align: int | None = None,
                   bits_per_sample: int | None = None) -> None:
    payload = bytearray(path.read_bytes())
    content = payload.index(b"fmt ") + 8
    if byte_rate is not None:
        struct.pack_into("<I", payload, content + 8, byte_rate)
    if block_align is not None:
        struct.pack_into("<H", payload, content + 12, block_align)
    if bits_per_sample is not None:
        struct.pack_into("<H", payload, content + 14, bits_per_sample)
    path.write_bytes(payload)


def self_test() -> int:
    try:
        with tempfile.TemporaryDirectory(prefix="acustra-capture-validator-") as temporary:
            root = Path(temporary)
            valid = root / "valid.wav"
            duplicate = root / "duplicate.wav"
            bad_rate = root / "bad-rate.wav"
            bad_bits = root / "bad-bits.wav"
            bad_layout = root / "bad-layout.wav"
            rail = root / "rail.wav"
            metadata = root / SESSION_METADATA
            metadata.write_text("# Session metadata\n\nTest fixture.\n", encoding="utf-8")
            if validate_session_metadata(root):
                raise AssertionError("valid session metadata was rejected")
            metadata.write_text(" \n", encoding="utf-8")
            if not any("must not be empty" in error
                       for error in validate_session_metadata(root)):
                raise AssertionError("empty session metadata was not rejected")
            metadata.write_bytes(b"\xff")
            if not any("readable UTF-8" in error
                       for error in validate_session_metadata(root)):
                raise AssertionError("invalid UTF-8 session metadata was not rejected")

            manifest = root / "capture.csv"
            rows = [",".join(HEADER)]
            for string_index, fret, dynamic, round_robin in sorted(
                    expected_identities(PILOT_FRETS)):
                rows.append(",".join(map(str, (
                    f"takes/s{string_index}_f{fret}_d{dynamic}_r{round_robin}.wav",
                    string_index, STANDARD_OPEN_MIDI[string_index], fret,
                    dynamic, round_robin))))
            manifest.write_text("\n".join(rows) + "\n", encoding="utf-8")
            captures, manifest_errors = read_manifest(manifest)
            if manifest_errors or validate_mapping(captures, PILOT_FRETS):
                raise AssertionError("valid pilot manifest mapping was rejected")
            if not any("missing identities" in error
                       for error in validate_mapping(captures[:-1], PILOT_FRETS)):
                raise AssertionError("missing pilot identity was not rejected")
            if not any("duplicate identity" in error
                       for error in validate_mapping(captures + captures[:1], PILOT_FRETS)):
                raise AssertionError("duplicate pilot identity was not rejected")

            write_test_wav(valid)
            shutil.copyfile(valid, duplicate)
            write_test_wav(bad_rate, sample_rate=44_100)
            shutil.copyfile(valid, bad_bits)
            patch_test_fmt(bad_bits, bits_per_sample=20)
            shutil.copyfile(valid, bad_layout)
            patch_test_fmt(bad_layout, byte_rate=1, block_align=1)
            write_test_wav(rail, rail=True)

            valid_report, valid_errors = inspect_wav(valid)
            if valid_errors or valid_report is None:
                raise AssertionError(f"valid fixture failed: {valid_errors}")
            duplicate_report, duplicate_errors = inspect_wav(duplicate)
            if duplicate_errors or duplicate_report is None:
                raise AssertionError(f"duplicate fixture failed inspection: {duplicate_errors}")
            found_duplicates = duplicate_pcm_errors([
                ("valid.wav", valid_report),
                ("duplicate.wav", duplicate_report),
            ])
            if len(found_duplicates) != 1:
                raise AssertionError("duplicate PCM payload was not rejected")

            _, bad_rate_errors = inspect_wav(bad_rate)
            if not any("sample rate must be 96000" in error for error in bad_rate_errors):
                raise AssertionError("bad sample rate was not rejected")
            _, bad_bits_errors = inspect_wav(bad_bits)
            if not any("24 bits" in error for error in bad_bits_errors):
                raise AssertionError("20-bit declaration was not rejected")
            _, bad_layout_errors = inspect_wav(bad_layout)
            if not any("6-byte frames" in error for error in bad_layout_errors):
                raise AssertionError("malformed PCM layout was not rejected")
            _, rail_errors = inspect_wav(rail)
            if not any("rail sample" in error for error in rail_errors):
                raise AssertionError("24-bit rail sample was not rejected")
    except (AssertionError, OSError, wave.Error) as error:
        print(f"self-test failed: {error}", file=sys.stderr)
        return 1
    print("self-test passed: metadata, mapping, format, duplicate and rail cases")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture_directory", nargs="?", type=Path)
    parser.add_argument(
        "--full", action="store_true",
        help="require every standard-tuning fret 0..20 instead of the pilot grid")
    parser.add_argument(
        "--self-test", action="store_true",
        help="run the dependency-free internal check and exit")
    args = parser.parse_args()
    if args.self_test:
        if args.capture_directory is not None or args.full:
            parser.error("--self-test cannot be combined with a capture directory or --full")
        return self_test()
    if args.capture_directory is None:
        parser.error("capture_directory is required unless --self-test is used")

    frets = FULL_FRETS if args.full else PILOT_FRETS
    errors, reports, capture_count = validate_session(args.capture_directory, frets)
    if errors:
        print(f"capture validation failed with {len(errors)} error(s):", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    mode = "full" if args.full else "pilot"
    print(
        f"validated {mode} capture: {capture_count} accepted takes, "
        f"{capture_count} WAV files")
    for relative, report in sorted(reports):
        print(f"{report.pcm_sha256}  {relative}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
