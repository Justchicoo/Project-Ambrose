# Project Ambrose by Imjustchico
# One run end to end: it drops and lets the server rebuild its own databases, starts the capture, the login server and its account, snapshots the install, starts the client through the launcher behind a guard that kills it on any connection off this machine, runs the scenario, then asks the client to quit or ends it outright when its own log says quitting would reach off the machine, stops everything in the order it started it, and writes the report whether the scenario passed or failed.
import os
import re
import secrets
import time

from . import install, paths, report, screens
from .capture import Capture
from .client import Client, prepare_process
from .database import Scratch
from .engine import Engine
from .logtail import read_lines
from .netguard import NetGuard, kill_leftovers
from .server import LoginServer

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
        self.prepared.append({"step": what, "ok": True, "result": str(value)})
        return value

    def failure(self, what, error):
        say(f"{what} failed: {error}")
        self.prepared.append({"step": what, "ok": False, "error": str(error)})

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
        client = Client(os.path.join(self.environment["binaries"], paths.program("launcher")),
                        os.path.join(self.folder, "client"), options["host"], options["port"],
                        self.references.window, client_dir=options.get("client"), locale=options.get("locale"),
                        install=self.environment.get("install"), revision=self.environment.get("revision"))
        store = screens.Store(self.references, options["refs"])
        engine = self.make_engine(client, server, store, variables, databases)
        guard = None
        before = {}
        try:
            self.note("the scratch databases", f"{databases.address} will hold {', '.join(sorted(databases.names.values()))}")
            self.note("dropped anything left from an earlier run", databases.drop())
            self.note("the capture", capture.start())
            self.cleanups.append(("stop the capture", capture.stop))
            before = install.snapshot(self.environment.get("install"))
            self.note("the install", f"{len(before)} file(s) under {self.environment.get('install')}")
            self.note("the login server", server.start(timeout=options["server_timeout"]))
            self.cleanups.append(("stop the login server", server.stop))
            self.note("the account", server.ensure_account(variables["user"], password))
            self.cleanups.append(("close the client", lambda: client.close(force=self.force_close)))
            self.note("the client", client.start(timeout=options["client_timeout"]))
            self.cleanups.append(("decide how the client is stopped", lambda: self.quit_safely(client)))
            self.note("the client window", f"{client.find_window(timeout=options['client_timeout']):#x} at "
                                           f"{self.references.window[0]}x{self.references.window[1]}")
            guard = NetGuard(client.pid, os.path.join(self.folder, "netguard.json"), started=started)
            guard.start()
            self.cleanups.append(("stop the guard", guard.stop))
            if options.get("background", True):
                self.note("the foreground", client.to_background())
            engine.run()
        except Exception as error:
            self.failed = str(error)
        finally:
            for name, stop in reversed(self.cleanups):
                try:
                    self.note(name, stop())
                except Exception as error:
                    self.failure(name, error)
            leftovers = kill_leftovers(started)
            if leftovers:
                self.failure("processes left running", ", ".join(leftovers))
            after = install.snapshot(self.environment.get("install"))
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
                "client_log": client.log.path,
                "steps": self.prepared + engine.steps,
                "screenshots": engine.screenshots,
                "recorded_lines": engine.notes,
                "capture": capture.facts(),
                "install_changes": install.diff(before, after),
                "netguard": guard.record() if guard else {"remotes": [], "violations": []},
                "leftover_processes": leftovers,
                "databases_after": remaining,
                "pending_allowed": self.scenario.pending_allowed,
                "server_log_allowed": self.scenario.server_log_allowed,
            }
            facts.update(self.extra(engine))
            written = report.build(facts, read_lines(server.log.path), read_lines(client.log.path, "latin-1"))
            path = report.write(self.folder, written)
        self.summarize(written, path)
        return 0 if written["clean"] else 1

    def make_engine(self, client, server, store, variables, databases):
        return Engine(self.scenario, client, server, store, self.shots, variables, databases)

    def quit_safely(self, client):
        rules = self.references.never_quit_after
        if not rules:
            return "the reference file names nothing that makes this client unsafe to be asked to quit"
        if not client.alive():
            return "the client has already stopped"
        client.log.poll()
        for rule in rules:
            if holds(client.log.lines, rule):
                self.force_close = True
                return f"the client is ended rather than asked to quit, because {rule['because']}"
        return "the client can be asked to quit, because nothing it has written leaves this machine on the way out"

    def extra(self, engine):
        return {}

    def summarize(self, written, path):
        say("-" * 70)
        say(f"run {self.run_id}: {written['result']} in {written['seconds']}s"
            + (f": {written['failed']}" if written["failed"] else ""))
        for check in written["checks"]:
            say(f"  {'pass' if check['ok'] else 'FAIL'}  {check['check']}: {check['detail']}")
        say(f"unhandled messages: {written['unhandled_messages'] or 'none'}")
        say(f"screenshots: {len(written['screenshots'])} in {self.shots}")
        say(f"report: {path}")
