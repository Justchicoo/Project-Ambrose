# Project Ambrose by Imjustchico
# Scenarios are data: this loads one JSON file with the scenarios it includes, merges their settings and allow-lists, fills its variables, and refuses a step whose action, keys, screen or target the driver does not know before anything is started.
import json
import os
import re

from .errors import Refused

VARIABLE = re.compile(r"\{(\w+)\}")

ACTIONS = {
    "wait_server_log": (("pattern", "timeout"), ("from", "fail", "expect", "reject", "record")),
    "wait_client_log": (("pattern", "timeout"), ("from", "fail", "expect", "reject", "record")),
    "forbid_log": (("side", "pattern"), ()),
    "wait_screen": (("screens", "timeout"), ()),
    "wait_db": (("query", "timeout"), ("database", "expect", "record")),
    "submit_login": (("password",), ("user",)),
    "type": (("text",), ()),
    "char": (("code",), ()),
    "key": (("vk",), ()),
    "click": (("target",), ("attempts", "dwell", "dwell_step", "on_screen", "until")),
    "shot": ((), ("file",)),
    "server_command": (("command",), ("pattern", "timeout")),
}
COMMON_KEYS = ("action", "name")
TOP_LEVEL = ("title", "notes", "include", "requires", "server_settings", "variables", "pending_allowed",
             "server_log_allowed", "expect", "steps")
REQUIRES = ("client", "capture")
SIDES = ("server", "client")
OUTCOMES = ("pass", "failure")


def fill(value, variables):
    if not isinstance(value, str):
        return value

    def replace(found):
        name = found.group(1)
        if name not in variables:
            raise Refused(f"the scenario uses the variable {{{name}}}, which the run does not set")
        return str(variables[name])

    return VARIABLE.sub(replace, value)


def variables_used(value, found=None):
    found = set() if found is None else found
    if isinstance(value, str):
        found.update(VARIABLE.findall(value))
    elif isinstance(value, dict):
        for item in value.values():
            variables_used(item, found)
    elif isinstance(value, list):
        for item in value:
            variables_used(item, found)
    return found


class Scenario:
    def __init__(self, path, document):
        self.path = path
        self.title = str(document.get("title", "")).strip()
        self.notes = list(document.get("notes") or [])
        self.steps = list(document.get("steps") or [])
        self.server_settings = list(document.get("server_settings") or [])
        self.variables = dict(document.get("variables") or {})
        self.pending_allowed = list(document.get("pending_allowed") or [])
        self.server_log_allowed = list(document.get("server_log_allowed") or [])
        requires = document.get("requires") or {}
        self.needs_client = bool(requires.get("client", True))
        self.needs_capture = bool(requires.get("capture", True))
        self.expect_failure = document.get("expect") == "failure"

    @property
    def name(self):
        return os.path.basename(self.path)

    def steps_with_checks(self):
        for step in self.steps:
            yield step
            if isinstance(step.get("until"), dict):
                yield step["until"]

    def screens_used(self):
        used = set()
        for step in self.steps_with_checks():
            for name in step.get("screens") or []:
                used.add(name)
            if step.get("on_screen"):
                used.add(step["on_screen"])
        return sorted(used)

    def targets_used(self):
        return sorted({step["target"] for step in self.steps_with_checks() if step.get("target")})

    def names_against(self, references):
        problems = []
        for name in self.screens_used():
            if name not in references.screens:
                problems.append(f"{self.name} waits on the screen {name}, which {references.path} does not describe")
        for name in self.targets_used():
            if name not in references.targets:
                problems.append(f"{self.name} presses the target {name}, which {references.path} does not describe")
        return problems


def _check_step(path, index, step):
    where = f"{os.path.basename(path)} step {index + 1}"
    if not isinstance(step, dict):
        raise Refused(f"{where} is not a step object")
    action = step.get("action")
    if action not in ACTIONS:
        raise Refused(f"{where} has the unknown action {action!r}; the driver knows {', '.join(sorted(ACTIONS))}")
    name = step.get("name") or action
    required, optional = ACTIONS[action]
    for key in required:
        if key not in step:
            raise Refused(f"{where} ({name}) needs a {key}")
    allowed = set(required) | set(optional) | set(COMMON_KEYS)
    for key in step:
        if key not in allowed:
            raise Refused(f"{where} ({name}) has the key {key!r}, which its {action} action does not take")
    if action in ("wait_server_log", "wait_client_log", "wait_screen", "wait_db") and not isinstance(step["timeout"], (int, float)):
        raise Refused(f"{where} ({name}) needs a timeout in seconds")
    if action == "wait_screen" and (not isinstance(step["screens"], list) or not step["screens"]):
        raise Refused(f"{where} ({name}) needs a list of screens to wait for")
    if action == "forbid_log" and step["side"] not in SIDES:
        raise Refused(f"{where} ({name}) must forbid a line on the {' or '.join(SIDES)} side")
    if action == "click" and isinstance(step.get("until"), dict):
        _check_step(path, index, dict(step["until"], name=f"{name}: the check that it took"))


def _check_document(path, document):
    if not isinstance(document, dict):
        raise Refused(f"{path} does not hold a scenario object")
    for key in document:
        if key not in TOP_LEVEL:
            raise Refused(f"{path} has the key {key!r}, which a scenario does not take")
    if not str(document.get("title", "")).strip():
        raise Refused(f"{path} needs a title saying what it checks")
    for key in document.get("requires") or {}:
        if key not in REQUIRES:
            raise Refused(f"{path} requires {key!r}, which the driver does not know")
    if document.get("expect", "pass") not in OUTCOMES:
        raise Refused(f"{path} expects {document['expect']!r}; a scenario expects {' or '.join(OUTCOMES)}")
    named = []
    for index, step in enumerate(document.get("steps") or []):
        _check_step(path, index, step)
        name = step.get("name") or step["action"]
        if name in named:
            raise Refused(f"{path} has two steps named {name!r}, and a step's name is how its failure and its screenshot are read")
        named.append(name)


def resolve(path, search):
    if os.path.isabs(path) and os.path.isfile(path):
        return path
    for folder in search:
        candidate = os.path.join(folder, path)
        if os.path.isfile(candidate):
            return candidate
    raise Refused(f"there is no scenario {path} in {', '.join(search) or 'the folders searched'}")


def _merge(base, scenario):
    scenario.steps = base.steps + scenario.steps
    scenario.server_settings = base.server_settings + [value for value in scenario.server_settings if value not in base.server_settings]
    scenario.pending_allowed = base.pending_allowed + [value for value in scenario.pending_allowed if value not in base.pending_allowed]
    scenario.server_log_allowed = base.server_log_allowed + [value for value in scenario.server_log_allowed if value not in base.server_log_allowed]
    scenario.variables = dict(base.variables, **scenario.variables)
    scenario.needs_client = base.needs_client or scenario.needs_client
    scenario.needs_capture = base.needs_capture or scenario.needs_capture
    scenario.expect_failure = base.expect_failure or scenario.expect_failure
    scenario.notes = base.notes + scenario.notes
    names = [step.get("name") or step["action"] for step in scenario.steps]
    repeated = sorted({name for name in names if names.count(name) > 1})
    if repeated:
        raise Refused(f"{scenario.path} and the scenarios it includes both hold a step named {', '.join(repeated)}")
    return scenario


def load(path, search=(), seen=()):
    resolved = os.path.abspath(resolve(path, list(search)))
    if resolved in seen:
        chain = " includes ".join(os.path.basename(step) for step in seen + (resolved,))
        raise Refused(f"the scenarios include each other: {chain}")
    try:
        with open(resolved, "r", encoding="utf-8") as handle:
            document = json.load(handle)
    except OSError as error:
        raise Refused(f"{resolved} cannot be read: {error}")
    except ValueError as error:
        raise Refused(f"{resolved} is not valid JSON: {error}")
    _check_document(resolved, document)
    scenario = Scenario(resolved, document)
    included = document.get("include")
    if included:
        base = load(included, search=list(search) + [os.path.dirname(resolved)], seen=seen + (resolved,))
        scenario = _merge(base, scenario)
    return scenario
