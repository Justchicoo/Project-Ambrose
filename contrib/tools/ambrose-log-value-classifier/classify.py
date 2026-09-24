# Project Ambrose by Imjustchico
# C-29 standalone classifier for safe value ranges in Ambrose log lines.
import argparse
import json
import re
import sys
from dataclasses import asdict, dataclass
from typing import Iterable


LEVELS = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"}
TOKEN_PATTERNS = (
    ("timestamp", re.compile(r"(?P<value>\d{2}:\d{2}:\d{2}\.\d{3})")),
    ("level", re.compile(r"(?P<value>TRACE|DEBUG|INFO|WARN|ERROR|FATAL)")),
    ("category", re.compile(r"\[(?P<value>[^\]]+)\]")),
    ("session", re.compile(r"(?P<value>Session \d+)\b", re.IGNORECASE)),
    ("message", re.compile(r"(?P<value>MSG_[A-Z][A-Z0-9_]*)\b")),
    ("account", re.compile(r"(?P<value>account \d+)\b", re.IGNORECASE)),
    ("address", re.compile(r"(?P<value>\d{1,3}(?:\.\d{1,3}){3}(?::\d{1,5})?)")),
    ("address", re.compile(r"(?P<value>\[[0-9A-Fa-f:]+\](?::\d{1,5})?)")),
    ("quoted", re.compile(r"(?P<value>\"[^\"\n]{1,120}\")")),
    ("path", re.compile(r"(?P<value>(?:[A-Za-z]:)?(?:[\\/][A-Za-z0-9_.-]+){2,})")),
    ("path", re.compile(r"(?P<value>[A-Za-z0-9_.-]+\.(?:conf|log|json|xml|dist|wad|lang))\b")),
    ("measure", re.compile(r"(?P<value>\d+(?:\.\d+)? ?(?:ms|us|ns|s|KiB|MiB|GiB|B|%))(?![A-Za-z])")),
)
NUMBER_PATTERN = re.compile(r"(?P<value>\b\d+\b)")


@dataclass(frozen=True)
class Span:
    value_class: str
    text: str
    start: int
    end: int


def byte_offset(text: str, character_offset: int) -> int:
    return len(text[:character_offset].encode("utf-8"))


def classify(line: str) -> list[Span]:
    candidates: list[tuple[int, int, str, str]] = []
    for value_class, pattern in TOKEN_PATTERNS:
        for match in pattern.finditer(line):
            value = match.group("value")
            if value_class == "level" and value not in LEVELS:
                continue
            candidates.append((match.start("value"), match.end("value"), value_class, value))

    occupied: list[tuple[int, int]] = []
    spans: list[Span] = []
    for start, end, value_class, value in sorted(candidates, key=lambda item: (item[0], -(item[1] - item[0]))):
        if any(start < occupied_end and end > occupied_start for occupied_start, occupied_end in occupied):
            continue
        occupied.append((start, end))
        spans.append(Span(value_class, value, byte_offset(line, start), byte_offset(line, end)))

    for match in NUMBER_PATTERN.finditer(line):
        start, end = match.span("value")
        if any(start < occupied_end and end > occupied_start for occupied_start, occupied_end in occupied):
            continue
        spans.append(Span("number", match.group("value"), byte_offset(line, start), byte_offset(line, end)))
        occupied.append((start, end))

    return sorted(spans, key=lambda span: span.start)


def check_spans(line: str, spans: Iterable[Span]) -> None:
    encoded = line.encode("utf-8")
    for span in spans:
        actual = encoded[span.start:span.end].decode("utf-8")
        if actual != span.text:
            raise ValueError(f"{span.value_class} span {span.start}:{span.end} does not match {span.text!r}")


def golden() -> None:
    cases = [
        (
            "21:12:55.502 INFO  [server.loginserver] Session 3 authenticated as account 17",
            ["timestamp", "level", "category", "session", "account"],
        ),
        (
            "21:12:54.102 INFO  [server.config    ] Configuration loaded from gameserver.conf",
            ["timestamp", "level", "category", "path"],
        ),
        (
            "21:12:57.884 INFO  [network.opcode   ] Session 3 sent MSG_CREATECHARACTER, which the login server does not handle yet",
            ["timestamp", "level", "category", "session", "message"],
        ),
        (
            "21:12:58.104 WARN  [network.opcode   ] Session 3 dropped malformed message: café",
            ["timestamp", "level", "category", "session"],
        ),
        (
            "21:12:54.310 INFO  [server.loginserver] Login listener ready on port 12000",
            ["timestamp", "level", "category", "number"],
        ),
        (
            "21:12:54.502 INFO  [server.loginserver] Session 3 from 127.0.0.1:12000 authenticated in 184 ms",
            ["timestamp", "level", "category", "session", "address", "measure"],
        ),
        (
            "21:12:55.006 INFO  [server.config    ] Read \"config.xml\" from /opt/ambrose/etc/gameserver.conf",
            ["timestamp", "level", "category", "quoted", "path"],
        ),
        (
            "21:12:55.410 WARN  [sql.driver       ] Pool at 87% after 2.5 s waiting on [fe80::1]:3306",
            ["timestamp", "level", "category", "measure", "measure", "address"],
        ),
    ]
    for line, expected in cases:
        spans = classify(line)
        check_spans(line, spans)
        actual = [span.value_class for span in spans]
        if actual != expected:
            raise ValueError(f"expected {expected}, got {actual} for {line!r}")
    print(f"C-29 golden cases: {len(cases)} passed")


def read_lines(args: argparse.Namespace) -> list[str]:
    if args.golden:
        return []
    if args.line is not None:
        return [args.line]
    if args.stdin:
        return [line.rstrip("\r\n") for line in sys.stdin]
    raise ValueError("provide a line, --stdin, or --golden")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Classify safe value ranges in Ambrose log lines.")
    parser.add_argument("line", nargs="?")
    parser.add_argument("--stdin", action="store_true")
    parser.add_argument("--golden", action="store_true")
    args = parser.parse_args(argv)
    try:
        if args.golden:
            golden()
            return 0
        output = []
        for line in read_lines(args):
            spans = classify(line)
            check_spans(line, spans)
            output.append({"line": line, "spans": [asdict(span) for span in spans]})
        for record in output:
            print(json.dumps(record, ensure_ascii=False))
    except (ValueError, UnicodeError) as exc:
        print(f"C-29 classifier: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
