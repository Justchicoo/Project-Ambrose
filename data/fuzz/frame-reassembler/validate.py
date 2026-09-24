# Project Ambrose by Imjustchico
# Validates synthetic frame-reassembler seeds against the repository's length and boundary contract, reading a long frame's declared length as the body alone, which is the FrameLimits default.

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
MANIFEST = ROOT / "manifest.json"
MAGIC = b"\x0d\xf0"
LONG_MARKER = 0x8000


def fail(message: str) -> None:
    raise ValueError(message)


def classify(data: bytes) -> str:
    if len(data) < 2 or data[:2] != MAGIC:
        return "bad magic"
    if len(data) < 4:
        return "incomplete"
    length = int.from_bytes(data[2:4], "little")
    long_frame = length == LONG_MARKER
    header_offset = 8 if long_frame else 4
    if len(data) < header_offset + 1:
        return "incomplete"
    control = data[header_offset]
    if control > 1:
        return "bad control flag"
    minimum = 4 + (0 if control else 4) + 1
    if not long_frame:
        if length < minimum:
            return "bad length"
        if len(data) < 4 + length:
            return "incomplete"
        payload = data[header_offset + 4:4 + length - 1]
    else:
        declared = int.from_bytes(data[4:8], "little")
        if declared > 0x100000:
            return "too large"
        total = 8 + minimum + declared
        if len(data) < total:
            return "incomplete"
        payload = data[header_offset + 4:total - 1]
    if control:
        return "valid control"
    if len(payload) < 4:
        return "bad DML length"
    dml_length = int.from_bytes(payload[2:4], "little")
    if dml_length < 4 or dml_length > len(payload):
        return "bad DML length"
    return "valid DML"


def main() -> int:
    try:
        payload = json.loads(MANIFEST.read_text(encoding="utf-8"))
        if not isinstance(payload, dict) or payload.get("version") != 1:
            fail("manifest must be version 1")
        seeds = payload.get("seeds")
        if not isinstance(seeds, list) or not seeds:
            fail("manifest seeds must be a non-empty list")
        seen: set[str] = set()
        for entry in seeds:
            if not isinstance(entry, dict):
                fail("each seed entry must be an object")
            filename = entry.get("file")
            expected = entry.get("result")
            if not isinstance(filename, str) or not filename or filename in seen:
                fail("seed filenames must be unique non-empty strings")
            if not isinstance(expected, str) or not expected:
                fail(f"{filename}: result must be a non-empty string")
            seen.add(filename)
            path = ROOT / filename
            data = path.read_bytes()
            actual = classify(data)
            if actual != expected:
                fail(f"{filename}: expected {expected!r}, got {actual!r}")
        files = {path.name for path in ROOT.glob("*.bin")}
        if files != seen:
            fail(f"manifest/files mismatch: manifest={sorted(seen)}, files={sorted(files)}")
        print(f"C-64 corpus: {len(seeds)} frame seeds valid")
        return 0
    except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as exc:
        print(f"C-64 corpus: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
