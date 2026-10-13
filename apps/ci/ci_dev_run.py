# Project Ambrose by Imjustchico
# Plans and runs a change's dev run, which boots what the change touches from the binaries a Linux leg just built: select maps a push's or pull request's changed paths to the targets to boot, the panel with its supervisor and patchserver, the login server and the game server, every target for a change to the build, the shared server code or this tool, and writes them as step outputs, and tells the leg selection whether a change has a target it boots so the Linux GCC leg is built for it, panel checks the built supervisor, patchserver and gameserver are there, builds the panel, rebuilds the supervisor in its build folder so the page its panel listener serves is that panel rather than the placeholder the leg compiled in before the panel existed, and runs the Playwright end-to-end specs against them, printing each failed spec's page snapshot into the log, failing on a missing binary rather than letting every spec skip, and servers boots the built login server, and the game server attached to it, through the client driver's server runner with no client install against scratch databases on the AMBROSE_TEST_DB server, checks each reaches ready, updates its databases, answers its admin API's health and its console and offers a SessionOffer on its port, then stops each by its process and drops the databases.
import argparse
import os
import secrets
import shutil
import socket
import subprocess
import sys
import tempfile
from collections import OrderedDict
from types import SimpleNamespace

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

EVERYTHING = (".github/workflows/core-build.yml", "apps/ci/ci_dev_run.py", "CMakeLists.txt", "CMakePresets.json", "cmake/", "vcpkg.json",
              "src/common/", "src/server/shared/", "src/server/database/")
TARGETS = OrderedDict([
    ("panel", ("apps/dashboard/", "packages/ui/", "design/", "tests/e2e/", "playwright.config.ts", "package.json", "package-lock.json",
               "src/server/apps/supervisor/", "src/server/apps/patchserver/")),
    ("loginserver", ("src/server/apps/loginserver/", "data/sql/", "apps/clientdriver/clientdriver/server.py", "apps/clientdriver/clientdriver/database.py")),
    ("gameserver", ("src/server/apps/gameserver/", "src/server/game/", "src/server/scripts/", "modules/", "data/", "apps/clientdriver/clientdriver/server.py",
                    "apps/clientdriver/clientdriver/database.py")),
])
BOOTED = ("panel", "loginserver", "gameserver")
SERVER_TARGETS = ("loginserver", "gameserver")
CLIENTDRIVER = os.path.join(ROOT, "apps", "clientdriver")
SCRATCH_PREFIX = "ambrose_driver_devrun"
SERVER_HOST = "127.0.0.1"
REALM = "AmbroseDevRun"
ACCOUNT = "devrun"
HEADLESS = ("ClientDir=", "TypeDumpPath=", "Setup.Mode=off")
CLEARED = ("AMBROSE_CLIENT_DIR", "AMBROSE_TYPE_DUMP_PATH", "AMBROSE_SETUP_MODE")
DATABASE_KINDS = (("login", "login"), ("characters", "characters"), ("world", "world"))
READY_TIMEOUT = 240
NO_INSTALL = "No Wizard101 install is in use"
TAIL_LINES = 40
PANEL_PROGRAMS = ("supervisor", "patchserver", "gameserver")
PANEL_ENVIRONMENT = {"supervisor": "AMBROSE_SUPERVISOR", "patchserver": "AMBROSE_PANEL_APP", "gameserver": "AMBROSE_GAMESERVER"}
DASHBOARD = os.path.join("apps", "dashboard", "dist", "index.html")
EMBEDDING = "supervisor"
RESULTS = "test-results"
FAILURE_PAGE = "error-context.md"


def matches(path, entry):
    return path.startswith(entry) if entry.endswith("/") else path == entry


def targets_for(paths):
    if paths is None:
        return list(TARGETS), ["every target: the change could not be diffed"]
    reasons = []
    widened = sorted({path for path in paths if any(matches(path, entry) for entry in EVERYTHING)})
    if widened:
        return list(TARGETS), [f"every target: {widened[0]} is shared by every app" + (f" (and {len(widened) - 1} more)" if len(widened) > 1 else "")]
    chosen = []
    for target, entries in TARGETS.items():
        hits = sorted(path for path in paths if any(matches(path, entry) for entry in entries))
        if hits:
            chosen.append(target)
            reasons.append(f"{target}: {hits[0]}" + (f" and {len(hits) - 1} more" if len(hits) > 1 else ""))
    if not chosen:
        reasons.append("no target: the change touches nothing a dev run boots")
    return chosen, reasons


def boots(paths):
    return any(target in BOOTED for target in targets_for(paths)[0])


def plan(event, before, after, git):
    if event == "pull_request":
        return targets_for(git.changed_paths(before, after, merge_base=True))
    if event == "push":
        return targets_for(git.changed_paths(before, after))
    chosen, _ = targets_for(None)
    return chosen, [f"every target: a {event or 'local'} run boots everything"]


def output_lines(chosen):
    return [f"targets={' '.join(chosen)}"] + [f"{target}={'true' if target in chosen else 'false'}" for target in TARGETS]


def summary_text(chosen, reasons):
    lines = [f"- {reason}" for reason in reasons]
    lines += [f"- {target}: selected, but not booted in CI yet" for target in chosen if target not in BOOTED]
    return "### Dev run\n\n" + "\n".join(lines) + "\n"


def select(arguments, environment, git):
    if arguments.from_github_env:
        event = environment.get("EVENT_NAME", "")
        before = environment.get("PR_BASE" if event == "pull_request" else "PUSH_BEFORE", "")
        after = environment.get("PR_HEAD" if event == "pull_request" else "PUSH_AFTER", "")
    else:
        event, before, after = arguments.event, arguments.before, arguments.after
    chosen, reasons = plan(event, before, after, git)
    print(summary_text(chosen, reasons), end="")
    if environment.get("GITHUB_OUTPUT"):
        with open(environment["GITHUB_OUTPUT"], "a", encoding="utf-8") as handle:
            handle.write("\n".join(output_lines(chosen)) + "\n")
    if environment.get("GITHUB_STEP_SUMMARY"):
        with open(environment["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as handle:
            handle.write(summary_text(chosen, reasons))
    return 0


def program(folder, name):
    return os.path.abspath(os.path.join(folder, name + (".exe" if os.name == "nt" else "")))


def panel_environment(binaries, environment):
    prepared = dict(environment)
    prepared.setdefault("CI", "1")
    for name in PANEL_PROGRAMS:
        prepared[PANEL_ENVIRONMENT[name]] = program(binaries, name)
    return prepared


def panel_commands(npm, npx, cmake, build_dir, config):
    return [[npm, "run", "build", "--workspace", "apps/dashboard"], [cmake, "--build", build_dir, "--config", config, "--target", EMBEDDING],
            [npx, "playwright", "test", "--project", "e2e"]]


def panel(arguments, environment, runner=subprocess.run, which=shutil.which):
    binaries = os.path.join(ROOT, arguments.binaries) if not os.path.isabs(arguments.binaries) else arguments.binaries
    missing = [program(binaries, name) for name in PANEL_PROGRAMS if not os.path.isfile(program(binaries, name))]
    if missing:
        print("the dev run needs the built " + ", ".join(missing) + "; build the leg first", file=sys.stderr)
        return 1
    build_dir = os.path.join(ROOT, arguments.build_dir) if not os.path.isabs(arguments.build_dir) else arguments.build_dir
    if not os.path.isdir(build_dir):
        print(f"the dev run needs the configured build folder {build_dir} to compile the panel into the supervisor", file=sys.stderr)
        return 1
    npm, npx, cmake = which("npm"), which("npx"), which("cmake")
    if not npm or not npx or not cmake:
        print("the dev run needs npm and npx on the PATH, with the workspace installed by npm ci, and cmake", file=sys.stderr)
        return 1
    prepared = panel_environment(binaries, environment)
    for command in panel_commands(npm, npx, cmake, build_dir, arguments.config):
        if command[0] == cmake and not os.path.isfile(os.path.join(ROOT, DASHBOARD)):
            print(f"the panel build left no {DASHBOARD}", file=sys.stderr)
            return 1
        print("+ " + " ".join(command), flush=True)
        if runner(command, cwd=ROOT, env=prepared).returncode != 0:
            if command[0] == npx:
                print_failure_pages(os.path.join(ROOT, RESULTS))
            return 1
    return 0


def print_failure_pages(results):
    pages = sorted(os.path.join(folder, name) for folder, _, names in os.walk(results) for name in names if name == FAILURE_PAGE)
    for page in pages:
        print(f"--- {os.path.relpath(page, results)}", flush=True)
        with open(page, "r", encoding="utf-8", errors="replace") as handle:
            print(handle.read(), flush=True)


def database_address(environment):
    parts = environment.get("AMBROSE_TEST_DB", "").split(";")
    if len(parts) < 5 or not parts[0] or not parts[1].isdigit():
        return None
    return parts[0], int(parts[1]), parts[2], parts[3]


def free_port(host=SERVER_HOST):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.bind((host, 0))
        return probe.getsockname()[1]


def server_targets(text):
    named = text.split()
    unknown = [name for name in named if name not in TARGETS]
    if unknown:
        raise ValueError("unknown target(s): " + ", ".join(unknown))
    chosen = [name for name in SERVER_TARGETS if name in named]
    if "gameserver" in chosen and "loginserver" not in chosen:
        chosen.insert(0, "loginserver")
    return chosen


def server_environment(environment, work):
    prepared = {key: value for key, value in environment.items() if key not in CLEARED}
    data = os.path.join(work, "data")
    prepared["XDG_DATA_HOME"] = data
    prepared["LOCALAPPDATA"] = data
    return prepared


def updated(server, databases, kind, label):
    said = server.console.matching(rf"Applied \d+ update\(s\) to the {label} database|The {label} database is up to date", since=server.console.start)
    if not said:
        raise RuntimeError(f"{server.WHAT} never said it updated the {label} database")
    applied = databases.value(kind, "SELECT COUNT(*) FROM updates")
    if not applied:
        raise RuntimeError(f"{databases.names[kind]} has no row in its updates table")
    return f"{said[-1].split('] ')[-1].strip()}; {databases.names[kind]} records {applied} applied file(s)"


def headless(server):
    if not server.console.matching(NO_INSTALL, since=server.console.start):
        raise RuntimeError(f"{server.WHAT} did not say it runs without an install")
    return "it runs with no Wizard101 install, as asked"


def console_answers(server, command, pattern, timeout=60):
    server.send(command)
    return server.console.wait(pattern, timeout, alive=server.alive).group(0)


def account_created(login, databases, password):
    said = login.ensure_account(ACCOUNT, password)
    return f"{said}; {databases.names['login']} holds it as id {databases.account_id(ACCOUNT)}"


def realm_attached(game, databases, port, timeout=60):
    game.console.wait(rf"Realm {REALM} at {SERVER_HOST}:{port} says it is alive", timeout, since=game.console.start, alive=game.alive, advance=False)
    beat = databases.value("login", f"SELECT last_heartbeat FROM realmlist WHERE name = '{REALM}'")
    if not beat:
        raise RuntimeError(f"realm {REALM} has no heartbeat in {databases.names['login']}")
    return f"realm {REALM} at {SERVER_HOST}:{port} beat into {databases.names['login']} at {beat}"


class ServerRun:
    def __init__(self, binaries, targets, address, environment, work, driver):
        self.binaries = binaries
        self.targets = targets
        self.address = address
        self.environment = environment
        self.work = work
        self.driver = driver
        self.results = []
        self.started = []

    def check(self, what, action):
        try:
            result = action()
        except Exception as error:
            self.results.append((what, False, str(error)))
            print(f"FAIL {what}: {error}", flush=True)
            return False
        self.results.append((what, True, str(result)))
        print(f"ok   {what}: {result}", flush=True)
        return True

    def make(self, kind, databases, settings):
        program = os.path.join(self.binaries, self.driver.paths.program(kind))
        defaults = os.path.join(self.binaries, kind + ".conf.dist")
        maker = self.driver.LoginServer if kind == "loginserver" else self.driver.GameServer
        return maker(program, defaults, os.path.join(self.work, kind), SERVER_HOST, free_port(), databases, settings=settings,
                     admin_port=free_port(), environment=server_environment(self.environment, self.work))

    def boot(self, server, databases, kinds):
        if not self.check(f"{server.WHAT} ready", lambda: server.start(timeout=READY_TIMEOUT)):
            return False
        self.check(f"{server.WHAT} runs headless", lambda: headless(server))
        for kind, label in kinds:
            self.check(f"{server.WHAT} updated the {label} database", lambda kind=kind, label=label: updated(server, databases, kind, label))
        self.check(f"{server.WHAT}'s admin API health", lambda: server.health())
        self.check(f"{server.WHAT} offers a session", lambda: server.session_offer())
        return True

    def run(self):
        host, port, user, password = self.address
        databases = self.driver.Scratch(host, port, user, password, SCRATCH_PREFIX)
        if not self.check("the scratch databases", lambda: f"{databases.address} will hold {', '.join(sorted(databases.names.values()))}; " + databases.drop()):
            return self.results
        try:
            login = self.make("loginserver", databases, list(HEADLESS))
            self.started.append(login)
            if self.boot(login, databases, DATABASE_KINDS):
                self.check("the login server's console", lambda: account_created(login, databases, secrets.token_urlsafe(12)))
                if "gameserver" in self.targets:
                    game = self.make("gameserver", databases, list(HEADLESS) + [f"Realm.Name={REALM}"])
                    self.started.append(game)
                    if self.boot(game, databases, DATABASE_KINDS):
                        self.check("the game server's console", lambda: console_answers(game, "server info", r"The world has ticked \d+ time\(s\) and holds \d+ session\(s\)"))
                        self.check("the game server attached to the login database", lambda: realm_attached(game, databases, game.port))
        finally:
            for server in reversed(self.started):
                self.check(f"{server.WHAT} stopped", lambda server=server: self.stopped(server))
            self.check("the scratch databases dropped", databases.drop)
        return self.results

    @staticmethod
    def stopped(server):
        said = server.stop()
        if server.alive():
            raise RuntimeError(f"process {server.process.pid} is still running: {said}")
        return said


def load_driver():
    if CLIENTDRIVER not in sys.path:
        sys.path.insert(0, CLIENTDRIVER)
    from clientdriver import database, paths, server

    return SimpleNamespace(LoginServer=server.LoginServer, GameServer=server.GameServer, Scratch=database.Scratch, paths=paths)


def tail(path, count=TAIL_LINES):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            return handle.read().splitlines()[-count:]
    except OSError:
        return []


def servers_summary(results):
    lines = [f"- {'ok' if ok else 'FAILED'}: {what}: {said}" for what, ok, said in results]
    return "### Dev run of the servers\n\n" + "\n".join(lines) + "\n"


def servers(arguments, environment, driver=None, work=None):
    binaries = os.path.join(ROOT, arguments.binaries) if not os.path.isabs(arguments.binaries) else arguments.binaries
    try:
        targets = server_targets(arguments.targets)
    except ValueError as error:
        print(f"the dev run cannot boot {arguments.targets!r}: {error}", file=sys.stderr)
        return 1
    if not targets:
        print(f"no server target is named in {arguments.targets!r}, so nothing is booted")
        return 0
    needed = [path for name in targets for path in (program(binaries, name), os.path.join(binaries, name + ".conf.dist"))]
    missing = [path for path in needed if not os.path.isfile(path)]
    if missing:
        print("the dev run needs the built " + ", ".join(missing) + "; build the leg first", file=sys.stderr)
        return 1
    address = database_address(environment)
    if address is None:
        print("the dev run needs AMBROSE_TEST_DB as host;port;user;password;database for the scratch databases", file=sys.stderr)
        return 1
    driver = driver or load_driver()
    work = work or tempfile.mkdtemp(prefix="ambrose-devrun-")
    print(f"booting {' and '.join(targets)} from {binaries} in {work}", flush=True)
    results = ServerRun(binaries, targets, address, environment, work, driver).run()
    failed = [what for what, ok, _ in results if not ok]
    if failed:
        for name in targets:
            for line in tail(os.path.join(work, name, "console.txt")):
                print(f"{name}| {line}")
    if environment.get("GITHUB_STEP_SUMMARY"):
        with open(environment["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as handle:
            handle.write(servers_summary(results))
    return 1 if failed else 0


def main(argv=None, environment=None, git=None, runner=subprocess.run, which=shutil.which, driver=None):
    parser = argparse.ArgumentParser(description="Project Ambrose dev run")
    commands = parser.add_subparsers(dest="command", required=True)
    chooser = commands.add_parser("select", help="map a change's paths to the targets its dev run boots")
    chooser.add_argument("--from-github-env", action="store_true", help="read the event and range from GitHub variables and write step outputs")
    chooser.add_argument("--event", default="", help="push or pull_request, for a dry run; anything else boots every target")
    chooser.add_argument("--before", default="", help="the range start, for a dry run")
    chooser.add_argument("--after", default="", help="the range end, for a dry run")
    runner_parser = commands.add_parser("panel", help="run the panel's end-to-end specs against built binaries")
    runner_parser.add_argument("--binaries", required=True, help="the folder holding the built supervisor, patchserver and gameserver")
    runner_parser.add_argument("--build-dir", required=True, help="the configured build folder those binaries came from, where the supervisor is relinked with the panel just built")
    runner_parser.add_argument("--config", required=True, help="the build configuration those binaries came from, such as Debug")
    booter = commands.add_parser("servers", help="boot the built login and game servers with no client install and check they serve")
    booter.add_argument("--binaries", required=True, help="the folder holding the built loginserver and gameserver")
    booter.add_argument("--targets", default="loginserver gameserver", help="the targets select chose, space separated; the game server also boots the login server")
    arguments = parser.parse_args(argv)
    environment = dict(os.environ) if environment is None else environment
    if arguments.command == "select":
        if git is None:
            from ci_select_legs import Git
            git = Git()
        return select(arguments, environment, git)
    if arguments.command == "servers":
        return servers(arguments, environment, driver)
    return panel(arguments, environment, runner, which)


if __name__ == "__main__":
    sys.exit(main())
