# Project Ambrose by Imjustchico
# Reads a log another process keeps open and appends to, with a cursor that only walks forward, so a wait sees only what arrived after the last match and two logins can wait on the same line.
import os
import re
import time

from .errors import StepFailed

POLL_INTERVAL = 0.05


class LogTail:
    def __init__(self, path, encoding="utf-8", interval=POLL_INTERVAL):
        self.path = path
        self.encoding = encoding
        self.interval = interval
        self.position = 0
        self.partial = b""
        self.lines = []
        self.cursor = 0

    @property
    def name(self):
        return os.path.basename(self.path)

    def poll(self):
        try:
            handle = open(self.path, "rb")
        except (FileNotFoundError, NotADirectoryError, PermissionError, OSError):
            return []
        with handle:
            handle.seek(0, os.SEEK_END)
            if handle.tell() < self.position:
                self.position = 0
                self.partial = b""
                self.lines = []
                self.cursor = 0
            handle.seek(self.position)
            data = handle.read()
            self.position = handle.tell()
        if not data:
            return []
        parts = (self.partial + data).split(b"\n")
        self.partial = parts.pop()
        arrived = [part.rstrip(b"\r").decode(self.encoding, "replace") for part in parts]
        self.lines.extend(arrived)
        return arrived

    def mark(self):
        self.poll()
        self.cursor = len(self.lines)
        return self.cursor

    def matching(self, pattern, since=0):
        expression = re.compile(pattern)
        self.poll()
        return [line for line in self.lines[since:] if expression.search(line)]

    def wait(self, pattern, timeout, since=None, fail=None, alive=None, advance=True):
        expression = re.compile(pattern)
        refusal = re.compile(fail) if fail else None
        index = self.cursor if since is None else since
        deadline = time.monotonic() + timeout
        while True:
            self.poll()
            while index < len(self.lines):
                line = self.lines[index]
                index += 1
                if refusal and refusal.search(line):
                    if advance:
                        self.cursor = index
                    raise StepFailed(f"{self.name} said: {line.strip()}")
                found = expression.search(line)
                if found:
                    if advance:
                        self.cursor = index
                    return found
            if alive is not None and not alive():
                raise StepFailed(f"nothing is writing {self.name} any more, and /{pattern}/ never appeared in it")
            if time.monotonic() > deadline:
                raise StepFailed(f"timed out after {timeout}s waiting for /{pattern}/ in {self.name}")
            time.sleep(self.interval)


def read_lines(path, encoding="utf-8"):
    try:
        with open(path, "rb") as handle:
            raw = handle.read()
    except OSError:
        return []
    return raw.decode(encoding, "replace").replace("\r\n", "\n").split("\n")
