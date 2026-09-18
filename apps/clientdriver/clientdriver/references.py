# Project Ambrose by Imjustchico
# The reference file: which screen each crop of the client's window identifies, where each press lands, the client revision, window size and interface scale they were measured at, so a run refuses references that do not belong to the client in front of it, and what the client must not have last written in its own log when it is asked to quit.
import json
import os
import re

from .errors import Refused
from . import screens

WINDOW = re.compile(r"^(\d{1,5})x(\d{1,5})$")


def parse_window(text):
    found = WINDOW.match(str(text).strip())
    if not found:
        raise Refused(f"'{text}' is not a window size such as 1280x720")
    return (int(found.group(1)), int(found.group(2)))


class References:
    def __init__(self, path, document):
        self.path = path
        key = document.get("key")
        if not isinstance(key, dict):
            raise Refused(f"{path} has no key naming the revision, window size and interface scale it was measured at")
        for field in ("revision", "window", "ui_scale"):
            if not str(key.get(field, "")).strip():
                raise Refused(f"{path}: the key needs a {field}")
        self.revision = str(key["revision"]).strip()
        self.window = parse_window(key["window"])
        self.ui_scale = str(key["ui_scale"]).strip()
        match = document.get("match") or {}
        self.tolerance = int(match.get("tolerance", screens.DEFAULT_TOLERANCE))
        self.fraction = float(match.get("fraction", screens.DEFAULT_FRACTION))
        if not 0 <= self.tolerance <= 255 or not 0 < self.fraction <= 1:
            raise Refused(f"{path}: a tolerance of {self.tolerance} and a fraction of {self.fraction} cannot decide a match")
        self.screens = {}
        for name, entry in (document.get("screens") or {}).items():
            self.screens[name] = self._crop(name, entry)
        self.targets = {}
        for name, entry in (document.get("targets") or {}).items():
            self.targets[name] = self._target(name, entry)
        if not self.screens:
            raise Refused(f"{path} names no screens")
        self.never_quit_after = [self._quit_rule(entry) for entry in (document.get("never_quit_after") or [])]

    def _crop(self, name, entry):
        box = entry.get("crop") if isinstance(entry, dict) else None
        if not isinstance(box, list) or len(box) != 4 or not all(isinstance(value, int) for value in box):
            raise Refused(f"{self.path}: the screen {name} needs a crop of four whole numbers")
        left, top, right, bottom = box
        width, height = self.window
        if not 0 <= left < right <= width or not 0 <= top < bottom <= height:
            raise Refused(f"{self.path}: the crop {tuple(box)} of the screen {name} does not lie inside a {width}x{height} window")
        if not str(entry.get("shows", "")).strip():
            raise Refused(f"{self.path}: the screen {name} needs a line saying what its crop shows")
        return tuple(box)

    def _target(self, name, entry):
        at = entry.get("at") if isinstance(entry, dict) else None
        if not isinstance(at, list) or len(at) != 2 or not all(isinstance(value, int) for value in at):
            raise Refused(f"{self.path}: the target {name} needs a point of two whole numbers")
        width, height = self.window
        if not 0 <= at[0] < width or not 0 <= at[1] < height:
            raise Refused(f"{self.path}: the target {name} at {tuple(at)} does not lie inside a {width}x{height} window")
        if not str(entry.get("presses", "")).strip():
            raise Refused(f"{self.path}: the target {name} needs a line saying what it presses")
        return tuple(at)

    def _quit_rule(self, entry):
        if not isinstance(entry, dict) or not str(entry.get("after", "")).strip():
            raise Refused(f"{self.path}: a rule about quitting needs the line the client must not have written last")
        if not str(entry.get("because", "")).strip():
            raise Refused(f"{self.path}: the rule about /{entry['after']}/ needs a line saying why")
        rule = {"after": entry["after"], "undone_by": entry.get("undone_by") or "", "because": entry["because"]}
        for field in ("after", "undone_by"):
            if rule[field]:
                try:
                    re.compile(rule[field])
                except re.error as error:
                    raise Refused(f"{self.path}: the rule's {field}, /{rule[field]}/, is not a pattern: {error}")
        return rule

    @property
    def key(self):
        return f"{self.revision} at {self.window[0]}x{self.window[1]} with interface scale {self.ui_scale}"

    @property
    def folder_name(self):
        plain = re.sub(r"[^A-Za-z0-9._-]+", "-", f"{self.revision}-{self.window[0]}x{self.window[1]}-ui-{self.ui_scale}")
        return plain.strip("-")

    def crop_of(self, name):
        if name not in self.screens:
            raise Refused(f"{self.path} describes no screen named {name}")
        return self.screens[name]

    def target_of(self, name):
        if name not in self.targets:
            raise Refused(f"{self.path} describes no target named {name}")
        return self.targets[name]

    def matches(self, revision):
        if revision and revision != self.revision:
            return False, f"the references were measured on {self.key}, and the install in use is {revision}"
        return True, ""

    def crop_file(self, folder, name):
        return os.path.join(folder, self.folder_name, name + ".png")

    def missing_crops(self, folder, names):
        return sorted(name for name in names if not os.path.isfile(self.crop_file(folder, name)))


def load(path):
    try:
        with open(path, "r", encoding="utf-8") as handle:
            document = json.load(handle)
    except OSError as error:
        raise Refused(f"{path} cannot be read: {error}")
    except ValueError as error:
        raise Refused(f"{path} is not valid JSON: {error}")
    if not isinstance(document, dict):
        raise Refused(f"{path} does not hold a reference object")
    return References(path, document)
