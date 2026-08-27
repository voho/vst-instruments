#!/usr/bin/env python3
"""Serve only a prepared blind pack's public files, without directory lists."""

from __future__ import annotations

import argparse
import functools
import hashlib
import json
import mimetypes
import os
import re
import stat
import types
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import quote, unquote_to_bytes, urlsplit


SHA256_PATTERN = re.compile(r"[0-9a-fA-F]{64}\Z")


def _duplicates_forbidden(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _validated_pack(pack_directory: Path,
                    expected_fingerprint: str) -> dict[str, str]:
    pack = pack_directory.resolve()
    if not isinstance(expected_fingerprint, str) or not SHA256_PATTERN.fullmatch(
            expected_fingerprint):
        raise ValueError("expected fingerprint must be exactly 64 hexadecimal digits")
    expected = expected_fingerprint.lower()
    key_path = pack / "private" / "answer-key.json"
    score_path = pack / "score.py"
    if key_path.is_symlink() or score_path.is_symlink():
        raise ValueError("prepared key/scorer must not be a symlink")
    try:
        key = json.loads(
            key_path.read_bytes().decode("utf-8"),
            object_pairs_hook=_duplicates_forbidden)
        implementation = key["implementation"]
        hashes = [
            key["source_manifest"]["sha256"],
            implementation["preparer_sha256"],
            implementation["runner_sha256"],
            implementation["scorer_sha256"],
            implementation["server_sha256"],
        ]
        key_fingerprint = key["study_fingerprint"]
    except (OSError, UnicodeError, json.JSONDecodeError, KeyError, TypeError) as error:
        raise ValueError(f"could not read the prepared answer key: {error}") from error
    if (not isinstance(key_fingerprint, str)
            or not SHA256_PATTERN.fullmatch(key_fingerprint)
            or any(not isinstance(value, str)
                   or not SHA256_PATTERN.fullmatch(value) for value in hashes)):
        raise ValueError("prepared answer key contains an invalid fingerprint/hash")
    fingerprint = hashlib.sha256(
        ":".join(value.lower() for value in hashes).encode("ascii")).hexdigest()
    if key_fingerprint.lower() != expected or fingerprint != expected:
        raise ValueError("prepared pack does not match the external expected fingerprint")
    try:
        scorer_bytes = score_path.read_bytes()
    except OSError as error:
        raise ValueError(f"could not read the prepared scorer: {error}") from error
    if hashlib.sha256(scorer_bytes).hexdigest() != hashes[3].lower():
        raise ValueError("prepared scorer SHA-256 does not match the answer key")
    scorer = types.ModuleType(f"_electry_frozen_score_{expected}")
    scorer.__file__ = str(score_path)
    try:
        exec(compile(scorer_bytes, str(score_path), "exec"), scorer.__dict__)
        _, _, frozen_pack = scorer._validate_key(key_path, expected)
    except (OSError, SyntaxError, ValueError) as error:
        raise ValueError(f"prepared pack validation failed: {error}") from error
    files = frozen_pack.get("public", {}).get("files")
    if (not isinstance(files, dict) or not files
            or any(not isinstance(path, str) or path.startswith("/")
                   or ".." in path.split("/")
                   or not isinstance(digest, str)
                   or not re.fullmatch(r"[0-9a-f]{64}", digest)
                   for path, digest in files.items())):
        raise ValueError("prepared scorer returned an invalid public-file index")
    return files


class NoListingHandler(BaseHTTPRequestHandler):
    def __init__(self, *args, directory: str,
                 expected_files: dict[str, str], **kwargs):
        self.directory = Path(directory)
        self.expected_files = expected_files
        super().__init__(*args, **kwargs)

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        super().end_headers()

    def _relative_path(self) -> str | None:
        raw_path = urlsplit(self.path).path
        try:
            decoded = unquote_to_bytes(raw_path).decode("utf-8")
        except (UnicodeDecodeError, ValueError):
            return None
        if quote(decoded, safe="/-._~") != raw_path:
            return None
        if decoded == "/":
            return "index.html"
        if (not decoded.startswith("/") or decoded.endswith("/")
                or "//" in decoded or "\\" in decoded):
            return None
        relative = decoded[1:]
        if any(part in ("", ".", "..") for part in relative.split("/")):
            return None
        return relative if relative in self.expected_files else None

    def _snapshot(self, relative: str) -> bytes | None:
        path = self.directory / relative
        if (path.is_symlink()
                or any(parent.is_symlink()
                       for parent in path.parents if parent != self.directory)
                or not path.is_file()):
            return None
        flags = os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0)
        try:
            descriptor = os.open(path, flags)
            with os.fdopen(descriptor, "rb") as source:
                if not stat.S_ISREG(os.fstat(source.fileno()).st_mode):
                    return None
                data = source.read()
        except OSError:
            return None
        if hashlib.sha256(data).hexdigest() != self.expected_files[relative]:
            return None
        return data

    def _range(self, size: int) -> tuple[int, int] | None | bool:
        header = self.headers.get("Range")
        if header is None:
            return None
        if not header.startswith("bytes=") or "," in header:
            return False
        value = header[6:]
        try:
            start_text, end_text = value.split("-", 1)
            if not start_text:
                suffix = int(end_text)
                if suffix <= 0 or size == 0:
                    return False
                return max(0, size - suffix), size - 1
            start = int(start_text)
            end = size - 1 if not end_text else min(int(end_text), size - 1)
        except (TypeError, ValueError):
            return False
        if start < 0 or start >= size or end < start:
            return False
        return start, end

    def _serve(self, send_body: bool) -> None:
        relative = self._relative_path()
        if relative is None:
            self.send_error(404)
            return
        data = self._snapshot(relative)
        if data is None:
            self.send_error(409, "Frozen public file changed")
            return
        selected = self._range(len(data))
        if selected is False:
            self.send_response(416)
            self.send_header("Content-Range", f"bytes */{len(data)}")
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        if selected is None:
            status = 200
            body = data
        else:
            status = 206
            start, end = selected
            body = data[start:end + 1]
        self.send_response(status)
        content_type = mimetypes.guess_type(relative)[0] or "application/octet-stream"
        self.send_header("Content-Type", content_type)
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("Content-Length", str(len(body)))
        if selected is not None:
            self.send_header("Content-Range", f"bytes {start}-{end}/{len(data)}")
        self.end_headers()
        if send_body:
            self.wfile.write(body)

    def do_GET(self):
        self._serve(True)

    def do_HEAD(self):
        self._serve(False)


def make_server(pack_directory: Path, host: str, port: int,
                expected_fingerprint: str) -> ThreadingHTTPServer:
    expected_files = _validated_pack(pack_directory, expected_fingerprint)
    public = pack_directory.resolve() / "public"
    if not public.is_dir():
        raise ValueError(f"missing prepared public directory: {public}")
    handler = functools.partial(
        NoListingHandler, directory=str(public), expected_files=expected_files)
    return ThreadingHTTPServer((host, port), handler)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bind", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument(
        "--expected-fingerprint", required=True,
        help="externally recorded pre-listening 64-hex study fingerprint")
    arguments = parser.parse_args()
    try:
        server = make_server(
            Path(__file__).resolve().parent, arguments.bind, arguments.port,
            arguments.expected_fingerprint)
    except ValueError as error:
        parser.error(str(error))
    print(f"Serving blinded sessions on http://{arguments.bind}:{server.server_port}/")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
