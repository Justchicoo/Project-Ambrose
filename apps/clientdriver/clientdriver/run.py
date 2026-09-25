# Project Ambrose by Imjustchico
# One run end to end: it drops and lets the server rebuild its own databases, starts the capture, the login server and its account, and for a scenario that enters the world loads the zone rows into its world database, starts the game server, which announces its realm to the login server, and seeds the scenario's wizard, then snapshots the install, starts the client through the launcher and guards it from the moment it exists against any connection off this machine, runs the scenario, then asks the client to quit or ends it outright when its own log says quitting would reach off the machine, stops everything in the order it started it with the guard watching until last, and writes the report over both servers' logs whether the scenario passed or failed.
import os
import re
import secrets
import time

from . import install, paths, report, screens, zones
from .capture import Capture
from .client import Client, prepare_process
from .database import Scratch
from .engine import Engine
from .logtail import read_lines
from .netguard import NetGuard, kill_leftovers
from .server import GameServer, LoginServer

USER = "clientdriver"


def stamp():
    return time.strftime("%Y%m%d-%H%M%S")


def say(message):
    print(f"{time.strftime('%H:%M:%S')} {message}", flush=True)


def holds(lines, rule):
    after = re.compile(rule["after"])
    undone = re.compile(rule["undone_by"]) if rule.get("undone_by") else None
    at = None
    for index, line in enumerate(lines):
        if after.search(line):
            at = index
    if at is None:
        return False
    return undone is None or not any(undone.search(line) for line in lines[at + 1:])


class Run:
    def __init__(self, options, scenario, references, environment):
        self.options = options
        self.scenario = scenario
        self.references = references
        self.environment = environment
        self.run_id = stamp()
        self.folder = os.path.join(options["runs"], self.run_id)
        self.shots = os.path.join(self.folder, "shots")
        self.cleanups = []
        self.prepared = []
        self.failed = None
        self.force_close = False

    def note(self, what, value):
        say(f"{what}: {value}")
        self.prepared.append({"step": what, "ok": True, "result": str(value), "stage": "driver"})
        return value

    def failure(self, what, error):
        say(f"{what} failed: {error}")
        self.prepared.append({"step": what, "ok": False, "error": str(error), "stage": "driver"})

    def stopping(self, what, stop):
        try:
            self.note(what, stop())
        except Exception as error:
            self.failure(what, error)

    def execute(self):
        options = self.options
        os.makedirs(self.shots, exist_ok=True)
        prepare_process()
        started = time.time()
        clock = time.monotonic()
        password = secrets.token_urlsafe(12)
        variables = dict(self.scenario.variables, user=options.get("user") or USER, password=password,
                         wrongpassword=password + "-wrong", host=options["host"], port=str(options["port"]),
                         revision=self.environment.get("revision") or "", run_id=self.run_id)
        databases = Scratch(options["db_host"], options["db_port"], options["db_user"], options["db_password"],
                            options["db_prefix"])
        capture = Capture(self.environment.get("tshark") if options.get("capture", True) and self.scenario.needs_capture else None,
                          os.path.join(self.folder, "login.pcapng"), options["port"])
        server = LoginServer(os.path.join(self.environment["binaries"], paths.program("loginserver")),
                             self.environment["server_defaults"], os.path.join(self.folder, "server"),
                             options["host"], options["port"], databases,
                             settings=list(self.scenario.server_settings) + list(options.get("set") or []))
        game = None
        if self.scenario.needs_gameserver:
            game = GameServer(os.path.join(self.environment["binaries"], paths.program("gameserver")),
                              os.path.join(self.environment["binaries"], "gameserver.conf.dist"), os.path.join(self.folder, "game"),
                              options["host"], options["game_port"], databases, settings=list(self.scenario.game_settings))
        client = Client(os.path.join(self.environment["binaries"], paths.program("launcher")),
                        os.path.join(self.folder, "client"), options["host"], options["port"],
                        self.references.window, client_dir=options.get("client"), locale=options.get("locale"),
                        install=self.environment.get("install"), revision=self.environment.get("revision"))
        store = screens.Store(self.references, options["refs"])
        engine = self.make_engine(client, server, store, variables, databases)
        engine.game = game
        guard = None
        before = {}
        try:
            self.note("the scratch databases", f"{databases.address} will hold {', '.join(sorted(databases.names.values()))}")
            self.note("dropped anything left from an earlier run", databases.drop())
            self.cleanups.append(("stop the capture", capture.stop))
            self.note("the capture", capture.start())
            before = install.snapshot(self.environment.get("install"))
            self.note("the install", f"{len(before)} file(s) under {self.environment.get('install')}")
            self.cleanups.append(("stop the login server", server.stop))
            self.note("the login server", server.start(timeout=options["server_timeout"]))
            self.note("the account", server.ensure_account(variables["user"], password))
            if game is not None:
                sql, how = zones.ensure(self.environment["binaries"], self.environment.get("install"), self.environment.get("revision"))
                self.note("the zone rows", how)
                self.note("the world database", databases.apply_sql("world", sql))
                self.cleanups.append(("stop the game server", game.stop))
                self.note("the game server", game.start(timeout=options["server_timeout"]))
                wizard = self.scenario.wizard
                if wizard and options.get("wizard_from"):
                    wizard = Scratch.read_wizard(options["wizard_from"], options.get("wizard_guid") or 1)
                    self.note("the wizard copied", f"wizard {options.get('wizard_guid') or 1} of {options['wizard_from'].split(';')[-1]}, read and left as it was")
                if wizard:
                    guid, name = databases.seed_character(variables["user"], wizard)
                    variables["wizard"] = name
                    variables["wizard_guid"] = str(guid)
                    self.note("the wizard", f"{name}, guid {guid}, in {wizard['zone']}")
            self.cleanups.append(("close the client", lambda: client.close(force=self.force_close)))
            self.note("the client", client.start(timeout=options["client_timeout"]))
            guard = NetGuard(client.pids, os.path.join(self.folder, "netguard.json"), started=started)
            guard.start()
            self.cleanups.append(("decide how the client is stopped", lambda: self.quit_safely(client)))
            self.note("the client window", f"{client.find_window(timeout=options['client_timeout']):#x} at "
                                           f"{self.references.window[0]}x{self.references.window[1]}")
            if options.get("background", True):
                self.note("the foreground", client.to_background())
            engine.run()
        except Exception as error:
            self.failed = str(error)
        finally:
            for name, stop in reversed(self.cleanups):
                self.stopping(name, stop)
            if guard is not None:
                self.stopping("stop the guard", guard.stop)
            leftovers = kill_leftovers(started, guard.known if guard else ())
            if leftovers:
                self.failure("processes left running", ", ".join(leftovers))
            after = install.snapshot(self.environment.get("install"))
            client_changes = install.split(install.diff(before, after), self.references.client_writes)
            try:
                self.note("dropped the scratch databases", databases.drop())
                remaining = databases.existing()
            except Exception as error:
                self.failure("dropping the scratch databases", error)
                remaining = ["unknown"]
            facts = {
                "run_id": self.run_id,
                "scenario": self.scenario.path,
                "title": self.scenario.title,
                "label": self.options.get("label"),
                "result": "FAILED" if self.failed else "ok",
                "failed": self.failed,
                "expect_failure": self.scenario.expect_failure,
                "seconds": round(time.monotonic() - clock, 1),
                "notes": self.scenario.notes,
                "launcher": {"install": client.install, "revision": client.revision, "run_folder": client.run_folder,
                             "command": client.command, "frames_from": client.frame_source},
                "references": {"path": self.references.path, "key": self.references.key, "folder": options["refs"]},
                "server_command": " ".join(server.command()),
                "server_log": server.log.path,
                "game_command": " ".join(game.command()) if game else None,
                "game_log": game.log.path if game else None,
                "client_log": client.log.path,
                "steps": self.prepared + engine.steps,
                "screenshots": engine.screenshots,
                "recorded_lines": engine.notes,
                "capture": capture.facts(),
                "needs_client": self.scenario.needs_client,
                "install": self.environment.get("install"),
                "install_files": len(before),
                "install_changes": client_changes[1],
                "client_writes": client_changes[0],
                "netguard": guard.record() if guard else None,
                "leftover_processes": leftovers,
                "databases_after": remaining,
                "pending_allowed": self.scenario.pending_allowed,
                "dropped_allowed": self.scenario.dropped_allowed,
                "server_log_allowed": self.scenario.server_log_allowed,
                "client_log_allowed": self.scenario.client_log_allowed,
            }
            facts.update(self.extra(engine))
            written, path = self.record(facts, server, client, game)
        self.summarize(written, path)
        return 0 if written["clean"] else 1

    def record(self, facts, server, client, game=None):
        try:
            server_lines = read_lines(server.log.path) + (read_lines(game.log.path) if game else [])
            written = report.build(facts, server_lines, read_lines(client.log.path, "latin-1"))
        except Exception as error:
            self.failure("build the report", error)
            written = dict(facts, clean=False, result="FAILED",
                           failed=self.failed or f"the report could not be built: {error}",
                           steps=list(facts.get("steps") or []) + self.prepared[-1:],
                           checks=[{"check": "the report was built", "ok": False, "detail": str(error)}])
        try:
            return written, report.write(self.folder, written)
        except Exception as error:
            self.failure("write the report", error)
            return written, f"none could be written: {error}"

    def make_engine(self, client, server, store, variables, databases):
        return Engine(self.scenario, client, server, store, self.shots, variables, databases)

    def quit_safely(self, client):
        rules = self.references.never_quit_after
        if not rules:
            return "the reference file names nothing that makes this client unsafe to be asked to quit"
        if not client.alive():
            return "the client has already stopped"
        client.log.poll()
        lines = [line for line in read_lines(client.log.path, client.log.encoding) if line.strip()] or \
            [line for line in client.log.lines if line.strip()]
        if not lines:
            self.force_close = True
            return (f"the client is ended rather than asked to quit, because {client.log.name} yielded no line to judge it "
                    "by, and a client that cannot be judged may be one a quit would take off this machine")
        for rule in rules:
            if holds(lines, rule):
                self.force_close = True
                return f"the client is ended rather than asked to quit, because {rule['because']}"
        return "the client can be asked to quit, because nothing it has written leaves this machine on the way out"

    def extra(self, engine):
        return {}

    def summarize(self, written, path):
        say("-" * 70)
        say(f"run {self.run_id}: {written.get('result')} in {written.get('seconds')}s"
            + (f": {written['failed']}" if written.get("failed") else ""))
        for check in written.get("checks") or []:
            say(f"  {'pass' if check['ok'] else 'FAIL'}  {check['check']}: {check['detail']}")
        say(f"unhandled messages: {written.get('unhandled_messages') or 'none'}")
        say(f"screenshots: {len(written.get('screenshots') or [])} in {self.shots}")
        say(f"report: {path}")
