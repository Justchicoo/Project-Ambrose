# Project Ambrose by Imjustchico
# Runs a scenario's steps: every step waits on a server line, a client line, a screen or a database row within its own timeout, a press is retried until the check that proves it took passes and fails when the window never became the active one, the waiting between attempts is done with the window released rather than held, and the frame after each step is kept so a step that changed the screen always leaves a screenshot behind; a shot may first let the screen settle, for a window a key opens, a restart asks the client to quit and starts it again under the same guard, for a scenario that logs a wizard in twice, and a log wait can keep what it matched for a later step to expect.
import os
import re
import time

from . import screens
from .errors import StepFailed
from .scenario import fill

MAX_DWELL = 0.5


def answered(said, wanted):
    if wanted is None:
        return said is not None and str(said) != ""
    return str(said) == str(wanted)


SETTLE_SECONDS = 0.3
MOVE_MARGIN = 0.2


def held_key_moved(still, held, margin=MOVE_MARGIN):
    return held <= still - margin


class Engine:
    def __init__(self, scenario, client, server, store, shots, variables, databases=None):
        self.scenario = scenario
        self.client = client
        self.server = server
        self.game = None
        self.store = store
        self.shots = shots
        self.variables = variables
        self.databases = databases
        self.steps = []
        self.screenshots = []
        self.notes = []
        self.taken = 0
        self.previous = None
        self.current = None
        self.restart = None

    def fill(self, value):
        return fill(value, self.variables)

    def run(self):
        for step in self.scenario.steps:
            self.execute(step)
        return self.steps

    def execute(self, step):
        name = step.get("name", step["action"])
        started = time.monotonic()
        record = {"step": name, "action": step["action"], "stage": "scenario"}
        self.steps.append(record)
        self.current = None
        try:
            record["result"] = self.perform(step)
            record["ok"] = True
        except Exception as error:
            record["ok"] = False
            record["error"] = str(error)
            record["seconds"] = round(time.monotonic() - started, 2)
            record["screen"] = self.look(name, forced=True)
            raise
        record["seconds"] = round(time.monotonic() - started, 2)
        record["screen"] = self.look(name)
        return record["result"]

    def perform(self, step):
        action = getattr(self, "act_" + step["action"], None)
        if action is None:
            raise StepFailed(f"the driver has no action named {step['action']!r}")
        return action(step)

    def look(self, name, forced=False):
        if self.client is None:
            return {}
        picture = self.current
        if picture is None:
            try:
                picture = self.client.frame()
            except Exception as error:
                return {"note": f"no frame could be taken: {error}"}
        changed, scored = screens.changed(self.previous, picture)
        self.previous = picture
        looked = {"changed": bool(changed), "fraction": (scored or {}).get("fraction")}
        if self.shot_of(name):
            looked["shot"] = self.shot_of(name)
        elif changed or forced:
            looked["shot"] = self.shot(("fail-" if forced else "") + name, picture)
        return looked

    def shot_of(self, name):
        for taken in reversed(self.screenshots):
            if taken.get("step") == name:
                return taken["shot"]
        return None

    def shot(self, name, picture=None):
        self.taken += 1
        base = f"{self.taken:02d}-{re.sub(r'[^a-z0-9]+', '-', str(name).lower()).strip('-')}.png"
        path = os.path.join(self.shots, base)
        try:
            self.client.screenshot(path, picture)
        except Exception as error:
            self.taken -= 1
            self.notes.append({"note": f"no screenshot for {name}: {error}"})
            return None
        self.screenshots.append({"shot": base, "step": name, "client_holds_the_foreground": self.client.is_foreground()})
        return base

    def act_shot(self, step):
        name = step.get("file") or step.get("name", "shot")
        if step.get("settle"):
            time.sleep(float(step["settle"]))
        picture = self.client.frame()
        self.current = picture
        taken = self.shot(name, picture)
        if not taken:
            raise StepFailed(f"the screenshot this step asks for could not be written: {self.notes[-1]['note']}")
        self.screenshots[-1]["step"] = step.get("name", name)
        return taken

    def act_wait_server_log(self, step):
        return self.wait_log(self.server.log, step, self.server.alive)

    def act_wait_game_log(self, step):
        if self.game is None:
            raise StepFailed("the scenario waits on the game server's log, but it does not require the game server")
        return self.wait_log(self.game.log, step, self.game.alive)

    def act_wait_client_log(self, step):
        return self.wait_log(self.client.log, step, self.client.alive)

    def wait_log(self, tail, step, alive):
        from_start = step.get("from") == "start"
        found = tail.wait(self.fill(step["pattern"]), step["timeout"], since=0 if from_start else None,
                          fail=self.fill(step["fail"]) if step.get("fail") else None, alive=alive, advance=not from_start)
        said = found.group(1) if found.re.groups else found.group(0)
        line = found.string.strip()
        if "expect" in step and said != self.fill(step["expect"]):
            raise StepFailed(f"the line says {said!r} where the step expects {self.fill(step['expect'])!r}: {line}")
        if "reject" in step and said == self.fill(step["reject"]):
            raise StepFailed(f"the line says {said!r}, which the step rejects: {line}")
        if step.get("record"):
            self.notes.append({"step": step.get("name"), "line": line})
        if step.get("keep"):
            self.variables[step["keep"]] = said
        return line

    def act_forbid_log(self, step):
        tail = self.server.log if step["side"] == "server" else self.client.log
        said = tail.matching(self.fill(step["pattern"]), since=0)
        if said:
            raise StepFailed(f"{tail.name} holds {len(said)} line(s) the step forbids: {said[0].strip()}")
        return f"nothing in {tail.name} matches /{self.fill(step['pattern'])}/"

    def act_wait_screen(self, step):
        return self.wait_screen(step["screens"], step["timeout"])

    def wait_screen(self, names, timeout, poll=0.25):
        deadline = time.monotonic() + timeout
        scored = {}
        while True:
            if not self.client.alive():
                raise StepFailed(f"the client stopped while the driver waited for the screen {'/'.join(names)}")
            picture = self.client.frame()
            self.current = picture
            found, scored = self.store.identify(picture, names)
            if found:
                return f"{found} is on the screen ({scored[found]['fraction']} of its pixels match)"
            if time.monotonic() > deadline:
                raise StepFailed(f"the screen {'/'.join(names)} did not appear within {timeout}s; "
                                 f"the closest match was {scored}")
            time.sleep(poll)

    def act_wait_db(self, step):
        if self.databases is None:
            raise StepFailed("this run has no database to read")
        kind = step.get("database", "characters")
        query = self.fill(step["query"])
        wanted = self.fill(step["expect"]) if "expect" in step else None
        deadline = time.monotonic() + step["timeout"]
        while True:
            said, refused = self.ask(kind, query)
            if refused is None and answered(said, wanted):
                if step.get("record"):
                    self.notes.append({"step": step.get("name"), "query": query, "answer": str(said)})
                return f"{kind} answered {said!r}"
            if time.monotonic() > deadline:
                if refused is not None:
                    raise StepFailed(f"the {kind} database never answered within {step['timeout']}s: {query}: {refused}")
                raise StepFailed(f"the {kind} database answered {said!r} where the step expects "
                                 f"{'an answer' if wanted is None else repr(wanted)} within {step['timeout']}s: {query}")
            time.sleep(0.25)

    def ask(self, kind, query):
        try:
            return self.databases.value(kind, query), None
        except Exception as error:
            return None, error

    def act_submit_login(self, step):
        user = self.fill(step.get("user", "{user}"))
        self.client.type(user)
        self.client.post_char(9)
        self.client.type(self.fill(step["password"]))
        self.client.post_char(13)
        return f"typed {user} and submitted the login window"

    def act_type(self, step):
        self.client.type(self.fill(step["text"]))
        return f"typed {len(self.fill(step['text']))} character(s)"

    def act_char(self, step):
        self.client.post_char(step["code"])
        return f"posted character {step['code']}"

    def act_key(self, step):
        virtual_key = step["vk"] if isinstance(step["vk"], int) else int(str(step["vk"]), 0)
        self.client.key(virtual_key)
        return f"posted the key {virtual_key:#x}"

    def act_restart_client(self, step):
        if self.restart is None:
            raise StepFailed("this run has no client it can start again")
        return self.restart(float(step.get("timeout", 180)))

    def act_hold_key(self, step):
        virtual_key = step["vk"] if isinstance(step["vk"], int) else int(str(step["vk"]), 0)
        seconds = float(step["seconds"])
        before = self.client.frame()
        time.sleep(seconds)
        idle = self.client.frame()
        self.client.key(virtual_key, hold=seconds)
        time.sleep(SETTLE_SECONDS)
        after = self.client.frame()
        self.current = after
        still = screens.matching(before, idle)
        held = screens.matching(idle, after)
        said = (f"held the key {virtual_key:#x} for {seconds}s: {held:.3f} of the frame stayed the same while it was held, "
                f"against {still:.3f} over the same time with no key")
        if step.get("moves") and not held_key_moved(still, held):
            raise StepFailed(f"{said}, which is not the view moving")
        return said

    def act_server_command(self, step):
        return self.console_command(self.server, step)

    def act_game_command(self, step):
        if self.game is None:
            raise StepFailed("the scenario gives the game server a command, but it does not require the game server")
        return self.console_command(self.game, step)

    def console_command(self, server, step):
        server.send(self.fill(step["command"]))
        if step.get("pattern"):
            found = server.console.wait(self.fill(step["pattern"]), step.get("timeout", 30), alive=server.alive)
            return found.group(0).strip()
        return f"sent {server.WHAT} the console command {self.fill(step['command'])!r}"

    def act_click(self, step):
        target = step["target"]
        x, y = self.store.references.target_of(target)
        attempts = int(step.get("attempts", 1))
        until = step.get("until")
        name = step.get("name", target)
        dwell = min(step.get("dwell", 0.35), MAX_DWELL)
        last = None
        said = "nothing was pressed"
        for attempt in range(attempts):
            if attempt:
                time.sleep(step.get("dwell_step", 0.3) * attempt)
            if step.get("on_screen") and not self.on_screen(step["on_screen"]):
                raise StepFailed(f"the {step['on_screen']} screen is no longer there, so {target} was not pressed")
            said, active = self.client.click(x, y, dwell=dwell)
            self.current = None
            if until:
                try:
                    self.perform(dict(until, name=f"{name}: the check that it took"))
                    return f"pressed {target}: {said}, and it took after {attempt + 1} attempt(s)"
                except StepFailed as error:
                    last = error
            elif active:
                return f"pressed {target}: {said}"
            else:
                last = StepFailed("the client's window never became the active one, so its interface dropped the press")
        raise StepFailed(f"{attempts} press(es) on {target} did not take, the last of them {said}: {last}")

    def on_screen(self, name):
        picture = self.client.frame()
        self.current = picture
        found, _scored = self.store.identify(picture, [name])
        return found == name
