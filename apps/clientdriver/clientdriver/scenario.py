# Project Ambrose by Imjustchico
# Scenarios are data: this loads one JSON file with the scenarios it includes, merges their settings and allow-lists, fills its variables, the install the driver found taken literally inside a pattern, and refuses a step whose action, keys, screen or target the driver does not know, a pattern that does not compile, a settle, hold or restart wait outside its bounds, a value kept under a name the run already uses, a seeded wizard's stat it does not carry or a negative one, more wizards without a first one, a patching mode other than off or default, a launch other than the launcher's console or its window, a launcher window opened beside the patching default or a companion, a step that reads or presses the launcher window in a scenario that does not open it or a restart in one that does, a companion without a wizard of its own, a step that drives or watches a client the run does not start, a statement meant for any database but the run's own, a watch that films too often or too long, a held key list that is empty or holds more than four keys, a listener without a name, an address, a port or the number of connections it should see, a wait on a listener the scenario does not name, or a comparison of shots no earlier step took, of a region that is not four edges in order, of an outcome other than differ or match or of colors read from anything but two shots on each side, before anything is started; it accepts the three purchased-emote masks for radial-page scenarios.
import json
import os
import re

from .errors import Refused

VARIABLE = re.compile(r"\{(\w+)\}")

ACTIONS = {
    "wait_server_log": (("pattern", "timeout"), ("from", "fail", "expect", "reject", "record", "keep")),
    "wait_client_log": (("pattern", "timeout"), ("from", "fail", "expect", "reject", "record", "keep")),
    "forbid_log": (("side", "pattern"), ()),
    "wait_screen": (("screens", "timeout"), ()),
    "wait_db": (("query", "timeout"), ("database", "expect", "record")),
    "db_exec": (("statement",), ("database",)),
    "submit_login": (("password",), ("user",)),
    "type": (("text",), ()),
    "char": (("code",), ()),
    "key": (("vk",), ()),
    "hold_key": (("vk", "seconds"), ("moves", "watch", "watch_every", "watch_after")),
    "click": (("target",), ("attempts", "clicks", "dwell", "dwell_step", "on_screen", "until", "watch", "watch_every", "watch_after")),
    "shot": ((), ("file", "settle")),
    "hover": (("target",), ("file", "settle")),
    "drag": (("target", "to"), ("dwell", "steps")),
    "server_command": (("command",), ("pattern", "timeout")),
    "game_command": (("command",), ("pattern", "timeout")),
    "go_to_npc": (("npc",), ("wizard", "distance", "timeout")),
    "stop_game_server": ((), ()),
    "start_game_server": ((), ("timeout",)),
    "wait_game_log": (("pattern", "timeout"), ("from", "fail", "expect", "reject", "record", "keep")),
    "kill_client": ((), ()),
    "play": ((), ()),
    "restart_client": ((), ("timeout",)),
    "wait_listener": (("listener", "timeout"), ()),
    "launcher_shows": (("patterns", "timeout"), ()),
    "launcher_press": (("control", "timeout"), ()),
    "compare_shots": (("first", "second", "region", "expect"), ("fraction",)),
    "compare_colors": (("first", "first_region", "second", "second_region"), ("degrees", "at_least")),
}
COMMON_KEYS = ("action", "name", "client")
CLIENTS = ("main", "companion")
CLIENT_ACTIONS = ("wait_client_log", "forbid_log", "wait_screen", "submit_login", "type", "char", "key", "hold_key", "click", "shot",
                  "kill_client", "restart_client", "wait_listener", "play", "hover")
ALLOW_LISTS = ("pending_allowed", "dropped_allowed", "server_log_allowed", "client_log_allowed")
TOP_LEVEL = ("title", "notes", "include", "requires", "server_settings", "game_settings", "wizard", "more_wizards", "companion", "variables", "expect", "steps",
             "patching", "launch", "listeners", "patch_config") + ALLOW_LISTS
COMPANION = ("wizard",)
PATCHING = ("off", "default")
LAUNCH = ("console", "window")
LAUNCHER_ACTIONS = ("launcher_shows", "launcher_press")
MAX_LAUNCHER_WAIT = 600
LISTENER_KEYS = ("name", "address", "port", "expect")
LISTENER_OPTIONAL = ("at_least",)
PATCH_CONFIG_KEYS = ("host", "port")
REQUIRES = ("client", "capture", "gameserver")
WIZARD = ("school", "zone", "first", "middle", "last")
WIZARD_STATS = ("overflow_xp", "secondary_school", "training_points", "gold", "health", "mana", "potion_charge", "potion_max", "arena_points", "level_locked",
                "purchased_custom_emotes_1", "purchased_custom_emotes_2", "purchased_custom_emotes_3")
SIDES = ("server", "game", "client")
OUTCOMES = ("pass", "failure")
MAX_HOLD_SECONDS = 30
MAX_SETTLE_SECONDS = 30
WATCH_EVERY = (0.1, 5)
MAX_WATCH_AFTER = 10
MAX_HELD_KEYS = 4
COMPARISONS = ("differ", "match")
MAX_HUE_DEGREES = 90
RUN_VARIABLES = ("user", "password", "wizard", "wizard_guid", "companion_user", "companion_password", "companion_wizard", "companion_wizard_guid",
                 "install", "window")
LITERAL_IN_PATTERNS = ("install",)
KEPT_NAME = re.compile(r"^\w+$")


def fill(value, variables, escape=()):
    if not isinstance(value, str):
        return value

    def replace(found):
        name = found.group(1)
        if name not in variables:
            raise Refused(f"the scenario uses the variable {{{name}}}, which the run does not set")
        return re.escape(str(variables[name])) if name in escape else str(variables[name])

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
        self.game_settings = list(document.get("game_settings") or [])
        self.wizard = document.get("wizard")
        self.more_wizards = list(document.get("more_wizards") or [])
        self.companion = dict(document["companion"]) if document.get("companion") else None
        self.variables = dict(document.get("variables") or {})
        for name in ALLOW_LISTS:
            setattr(self, name, list(document.get(name) or []))
        requires = document.get("requires") or {}
        self.needs_client = bool(requires.get("client", True))
        self.needs_capture = bool(requires.get("capture", True))
        self.needs_gameserver = bool(requires.get("gameserver", False))
        self.expect_failure = document.get("expect") == "failure"
        self.patching = document.get("patching", "off")
        self.launch = document.get("launch", "console")
        self.listeners = [dict(listener) for listener in document.get("listeners") or []]
        self.patch_config = dict(document["patch_config"]) if document.get("patch_config") else None

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
        return sorted({step[key] for step in self.steps_with_checks() for key in ("target", "to") if step.get(key)})

    def names_against(self, references):
        problems = []
        for name in self.screens_used():
            if name not in references.screens:
                problems.append(f"{self.name} waits on the screen {name}, which {references.path} does not describe")
        for name in self.targets_used():
            if name not in references.targets:
                problems.append(f"{self.name} presses the target {name}, which {references.path} does not describe")
        return problems


def _check_pattern(where, what, pattern):
    if not isinstance(pattern, str) or not pattern.strip():
        raise Refused(f"{where}: the {what} must be a pattern, not {pattern!r}")
    try:
        re.compile(pattern)
    except re.error as error:
        raise Refused(f"{where}: the {what}, /{pattern}/, is not a pattern: {error}")


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
    if action == "db_exec" and (step.get("database", "characters") not in ("login", "characters", "world") or not str(step["statement"]).strip()):
        raise Refused(f"{where} ({name}) runs one statement on the run's own login, characters or world database")
    if action in ("wait_server_log", "wait_client_log", "wait_screen", "wait_db", "wait_listener") and not isinstance(step["timeout"], (int, float)):
        raise Refused(f"{where} ({name}) needs a timeout in seconds")
    if action in LAUNCHER_ACTIONS and (isinstance(step["timeout"], bool) or not isinstance(step["timeout"], (int, float))
                                       or not 0 < step["timeout"] <= MAX_LAUNCHER_WAIT):
        raise Refused(f"{where} ({name}) may wait more than 0 and at most {MAX_LAUNCHER_WAIT} seconds on the launcher window")
    if action == "launcher_shows" and (not isinstance(step["patterns"], list) or not step["patterns"]):
        raise Refused(f"{where} ({name}) needs a list of patterns the launcher window should show")
    if action == "launcher_shows":
        for pattern in step["patterns"]:
            _check_pattern(f"{where} ({name})", "pattern", pattern)
    if action == "launcher_press" and (not isinstance(step["control"], str) or not step["control"].strip()):
        raise Refused(f"{where} ({name}) needs the accessible name of the control it presses")
    if action == "wait_screen" and (not isinstance(step["screens"], list) or not step["screens"]):
        raise Refused(f"{where} ({name}) needs a list of screens to wait for")
    if action == "hold_key" and (not isinstance(step["seconds"], (int, float)) or not 0 < step["seconds"] <= MAX_HOLD_SECONDS):
        raise Refused(f"{where} ({name}) needs to hold its key for more than 0 and at most {MAX_HOLD_SECONDS} seconds")
    if "keep" in step and (not isinstance(step["keep"], str) or not KEPT_NAME.match(step["keep"]) or step["keep"] in RUN_VARIABLES):
        raise Refused(f"{where} ({name}) keeps what it matched under a name of letters, digits and underscores that is not one of the run's own, {', '.join(RUN_VARIABLES)}")
    if action == "restart_client" and "timeout" in step and (not isinstance(step["timeout"], (int, float)) or isinstance(step["timeout"], bool) or not 0 < step["timeout"] <= 600):
        raise Refused(f"{where} ({name}) may wait more than 0 and at most 600 seconds for the client to come back")
    if action == "start_game_server" and "timeout" in step and (not isinstance(step["timeout"], (int, float)) or isinstance(step["timeout"], bool) or not 0 < step["timeout"] <= 600):
        raise Refused(f"{where} ({name}) may wait more than 0 and at most 600 seconds for the game server to come back")
    if action in ("shot", "hover") and "settle" in step and (not isinstance(step["settle"], (int, float)) or isinstance(step["settle"], bool) or not 0 <= step["settle"] <= MAX_SETTLE_SECONDS):
        raise Refused(f"{where} ({name}) may let the screen settle for 0 to {MAX_SETTLE_SECONDS} seconds before its shot")
    if "client" in step and step["client"] not in CLIENTS:
        raise Refused(f"{where} ({name}) drives the client {step['client']!r}; a run drives {' or '.join(CLIENTS)}")
    if "client" in step and action not in CLIENT_ACTIONS:
        raise Refused(f"{where} ({name}) names a client, but its {action} action drives none")
    if action == "hold_key" and isinstance(step["vk"], list) and not 0 < len(step["vk"]) <= MAX_HELD_KEYS:
        raise Refused(f"{where} ({name}) holds a list of 1 to {MAX_HELD_KEYS} keys at once")
    if "watch" in step and step["watch"] not in CLIENTS:
        raise Refused(f"{where} ({name}) watches the client {step['watch']!r}; a run drives {' or '.join(CLIENTS)}")
    if ("watch_every" in step or "watch_after" in step) and "watch" not in step:
        raise Refused(f"{where} ({name}) says how to watch but not which client it watches")
    if "watch_every" in step and (isinstance(step["watch_every"], bool) or not isinstance(step["watch_every"], (int, float))
                                  or not WATCH_EVERY[0] <= step["watch_every"] <= WATCH_EVERY[1]):
        raise Refused(f"{where} ({name}) films the watched client every {WATCH_EVERY[0]} to {WATCH_EVERY[1]} seconds")
    if "watch_after" in step and (isinstance(step["watch_after"], bool) or not isinstance(step["watch_after"], (int, float))
                                  or not 0 <= step["watch_after"] <= MAX_WATCH_AFTER):
        raise Refused(f"{where} ({name}) may go on watching for 0 to {MAX_WATCH_AFTER} seconds after the key is let go or the press is made")
    if action == "forbid_log" and step["side"] not in SIDES:
        raise Refused(f"{where} ({name}) must forbid a line on the {' or '.join(SIDES)} side")
    for key in ("pattern", "fail"):
        if key in step:
            _check_pattern(f"{where} ({name})", key, step[key])
    if action == "click" and isinstance(step.get("until"), dict):
        _check_step(path, index, dict(step["until"], name=f"{name}: the check that it took"))
    for key in ("region", "first_region", "second_region"):
        region = step.get(key)
        if action in ("compare_shots", "compare_colors") and key in ACTIONS[action][0] and (
                not isinstance(region, list) or len(region) != 4 or any(isinstance(value, bool) or not isinstance(value, int) or value < 0 for value in region)
                or not region[0] < region[2] or not region[1] < region[3]):
            raise Refused(f"{where} ({name}) needs a {key.replace('_', ' ')} of four pixel edges, [left, top, right, bottom], with left before right and top above bottom")
    if action == "compare_colors":
        for key in ("first", "second"):
            if not isinstance(step[key], list) or len(step[key]) != 2 or not all(isinstance(shot, str) for shot in step[key]):
                raise Refused(f"{where} ({name}) needs its {key} as the names of two shots, before and after the change whose color it reads")
        if "degrees" in step and (isinstance(step["degrees"], bool) or not isinstance(step["degrees"], (int, float)) or not 0 < step["degrees"] <= MAX_HUE_DEGREES):
            raise Refused(f"{where} ({name}) lets the two hues be more than 0 and at most {MAX_HUE_DEGREES} degrees apart")
        if "at_least" in step and (isinstance(step["at_least"], bool) or not isinstance(step["at_least"], int) or step["at_least"] < 1):
            raise Refused(f"{where} ({name}) needs at_least as a count of one or more changed colored pixels")
    if action == "compare_shots":
        if step["expect"] not in COMPARISONS:
            raise Refused(f"{where} ({name}) expects the two shots to {' or '.join(COMPARISONS)}, not {step['expect']!r}")
        if "fraction" in step and (isinstance(step["fraction"], bool) or not isinstance(step["fraction"], (int, float)) or not 0 < step["fraction"] <= 1):
            raise Refused(f"{where} ({name}) needs a fraction of matching pixels more than 0 and at most 1")


def _check_wizard(path, wizard, what):
    if not isinstance(wizard, dict):
        raise Refused(f"{path}: {what} must be an object")
    missing = [key for key in WIZARD if key not in wizard]
    if missing:
        raise Refused(f"{path}: {what} needs {', '.join(missing)}")
    stats = wizard.get("stats")
    if stats is not None:
        if not isinstance(stats, dict):
            raise Refused(f"{path}: {what}'s stats must be an object")
        for key, value in stats.items():
            if key not in WIZARD_STATS:
                raise Refused(f"{path}: {what}'s stats have {key!r}, which a wizard does not carry; it carries {', '.join(WIZARD_STATS)}")
            if isinstance(value, bool) or not isinstance(value, (int, float)) or value < 0:
                raise Refused(f"{path}: {what}'s {key} must be a number of zero or more")


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
    wizard = document.get("wizard")
    if wizard is not None:
        _check_wizard(path, wizard, "the wizard")
        if not (document.get("requires") or {}).get("gameserver"):
            raise Refused(f"{path} seeds a wizard but does not require the game server it enters the world on")
    more = document.get("more_wizards")
    if more is not None:
        if not isinstance(more, list) or not more:
            raise Refused(f"{path}: more_wizards must be a list of one or more wizards")
        if wizard is None:
            raise Refused(f"{path} seeds more wizards but not the first wizard they join")
        for index, other in enumerate(more):
            _check_wizard(path, other, f"wizard {index + 2}")
    companion = document.get("companion")
    if companion is not None:
        if not isinstance(companion, dict) or sorted(companion) != sorted(COMPANION):
            raise Refused(f"{path}: the companion needs exactly {', '.join(COMPANION)}")
        _check_wizard(path, companion["wizard"], "the companion's wizard")
        if not (document.get("requires") or {}).get("gameserver"):
            raise Refused(f"{path} starts a companion but does not require the game server its wizard enters the world on")
    if document.get("patching", "off") not in PATCHING:
        raise Refused(f"{path} asks for patching {document['patching']!r}; a scenario runs the client with patching {' or '.join(PATCHING)}")
    if document.get("launch", "console") not in LAUNCH:
        raise Refused(f"{path} asks for launch {document['launch']!r}; a scenario starts the launcher as a {' or a '.join(LAUNCH)}")
    listeners = document.get("listeners")
    if listeners is not None:
        if not isinstance(listeners, list):
            raise Refused(f"{path}: listeners must be a list")
        seen = []
        for listener in listeners:
            if not isinstance(listener, dict) or not set(LISTENER_KEYS) <= set(listener) or not set(listener) <= set(LISTENER_KEYS + LISTENER_OPTIONAL):
                raise Refused(f"{path}: each listener needs {', '.join(LISTENER_KEYS)} and may say {', '.join(LISTENER_OPTIONAL)}")
            if not isinstance(listener.get("at_least", False), bool):
                raise Refused(f"{path}: listener {listener['name']!r} says at_least with true or false")
            port, expect = listener["port"], listener["expect"]
            if isinstance(port, bool) or not isinstance(port, int) or not 0 < port < 65536:
                raise Refused(f"{path}: listener {listener['name']!r} needs a port from 1 to 65535")
            if isinstance(expect, bool) or not isinstance(expect, int) or expect < 0:
                raise Refused(f"{path}: listener {listener['name']!r} needs the number of connections it should see, zero or more")
            where = (listener["address"], port)
            if listener["name"] in [name for name, _ in seen] or where in [place for _, place in seen]:
                raise Refused(f"{path} names listener {listener['name']!r} or its address and port twice")
            seen.append((listener["name"], where))
    patch_config = document.get("patch_config")
    if patch_config is not None:
        if not isinstance(patch_config, dict) or sorted(patch_config) != sorted(PATCH_CONFIG_KEYS):
            raise Refused(f"{path}: patch_config needs exactly {', '.join(PATCH_CONFIG_KEYS)}")
        port = patch_config["port"]
        if isinstance(port, bool) or not isinstance(port, int) or not 0 < port < 65536:
            raise Refused(f"{path}: patch_config needs a port from 1 to 65535")
        if not isinstance(patch_config["host"], str) or not patch_config["host"].strip():
            raise Refused(f"{path}: patch_config needs a host")
    named_listeners = [listener.get("name") for listener in document.get("listeners") or [] if isinstance(listener, dict)]
    for index, step in enumerate(document.get("steps") or []):
        if isinstance(step, dict) and step.get("action") == "wait_listener" and step.get("listener") not in named_listeners:
            raise Refused(f"{path} step {index + 1} waits on listener {step.get('listener')!r}, which the scenario does not name")
    if document.get("expect", "pass") not in OUTCOMES:
        raise Refused(f"{path} expects {document['expect']!r}; a scenario expects {' or '.join(OUTCOMES)}")
    for key in ALLOW_LISTS:
        entries = document.get(key)
        if entries is None:
            continue
        if not isinstance(entries, list):
            raise Refused(f"{path}: {key} must be a list of patterns")
        for entry in entries:
            _check_pattern(f"{os.path.basename(path)} {key}", "entry", entry)
    named = []
    shots = []
    for index, step in enumerate(document.get("steps") or []):
        _check_step(path, index, step)
        name = step.get("name") or step["action"]
        if name in named:
            raise Refused(f"{path} has two steps named {name!r}, and a step's name is how its failure and its screenshot are read")
        named.append(name)
        if step["action"] in ("compare_shots", "compare_colors"):
            for key in ("first", "second"):
                for shot in (step[key] if step["action"] == "compare_colors" else [step[key]]):
                    if shot not in shots:
                        raise Refused(f"{path} step {index + 1} ({name}) compares the shot {shot!r}, which no earlier shot step takes")
        if step["action"] == "shot":
            if (step.get("file") or name) in shots:
                raise Refused(f"{path} step {index + 1} ({name}) takes the shot {step.get('file') or name!r} a second time, so a comparison could not tell them apart")
            shots.append(step.get("file") or name)


def _check_launch(scenario):
    if scenario.launch == "window":
        if scenario.patching == "default":
            raise Refused(f"{scenario.path} opens the launcher window and follows the client's patching default, but only the launcher's "
                          "console prepares the command that default is run from")
        if scenario.companion is not None:
            raise Refused(f"{scenario.path} opens the launcher window and starts a companion, but a run opens one launcher window")
    for index, step in enumerate(scenario.steps):
        action = step["action"]
        if scenario.launch != "window" and action in LAUNCHER_ACTIONS:
            raise Refused(f"{scenario.path} step {index + 1} ({step.get('name') or action}) reads or presses the launcher window, "
                          "but the scenario does not open it with launch window")
        if scenario.launch == "window" and action == "restart_client":
            raise Refused(f"{scenario.path} step {index + 1} ({step.get('name') or action}) restarts the client, "
                          "but a client started from the launcher window is started again only by its Play")


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
    for name in ALLOW_LISTS:
        kept = getattr(base, name)
        setattr(scenario, name, kept + [value for value in getattr(scenario, name) if value not in kept])
    scenario.variables = dict(base.variables, **scenario.variables)
    scenario.needs_client = base.needs_client or scenario.needs_client
    scenario.needs_capture = base.needs_capture or scenario.needs_capture
    scenario.needs_gameserver = base.needs_gameserver or scenario.needs_gameserver
    scenario.game_settings = base.game_settings + [value for value in scenario.game_settings if value not in base.game_settings]
    scenario.wizard = scenario.wizard or base.wizard
    scenario.more_wizards = scenario.more_wizards or base.more_wizards
    scenario.companion = scenario.companion or base.companion
    scenario.expect_failure = base.expect_failure or scenario.expect_failure
    scenario.notes = base.notes + scenario.notes
    scenario.listeners = base.listeners + scenario.listeners
    scenario.patching = scenario.patching if scenario.patching != "off" else base.patching
    scenario.launch = scenario.launch if scenario.launch != "console" else base.launch
    scenario.patch_config = scenario.patch_config or base.patch_config
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
    _check_launch(scenario)
    if scenario.companion is None:
        for index, step in enumerate(scenario.steps):
            if step.get("client") == "companion" or step.get("watch") == "companion":
                raise Refused(f"{scenario.path} step {index + 1} ({step.get('name') or step['action']}) drives the companion client, "
                              "but the scenario starts no companion")
    return scenario
