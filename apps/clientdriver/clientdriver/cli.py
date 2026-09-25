# Project Ambrose by Imjustchico
# The driver's command line: run a scenario against the install named by --client or AMBROSE_CLIENT_DIR, say whether a run is possible on this machine and skip with 77 when it is not, rebuild the reference crops from a live client, or list the scenarios.
import argparse
import os
import sys

from . import paths, preflight, references as refs, scenario as scenarios
from .errors import Refused

OK = 0
FAILED = 1
BAD_USAGE = 2
SKIP = 77
DEFAULT_SCENARIO = "login-to-charselect.json"
DEFAULT_HOST = "127.0.0.2"
DEFAULT_PORT = 12100
DEFAULT_GAME_PORT = 12433
DEFAULT_DB = ("127.0.0.1", 3307, "ambrose", "ambrose", "ambrose_driver_run")


def add_common(parser):
    parser.add_argument("--scenario", default=DEFAULT_SCENARIO, help="scenario file, by name in apps/clientdriver/scenarios or by path")
    parser.add_argument("--binaries", help="folder holding the built loginserver and launcher")
    parser.add_argument("--client", help="the Wizard101 install to use, as the launcher's --client; without it and without AMBROSE_CLIENT_DIR no client is driven and the run skips")
    parser.add_argument("--locale", help="client locale, as the launcher's --locale")
    parser.add_argument("--host", default=DEFAULT_HOST, help="address the login server binds and the client connects to")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="login server port")
    parser.add_argument("--game-port", type=int, default=DEFAULT_GAME_PORT, help="game server port, for a scenario that enters the world")
    parser.add_argument("--db-host", default=DEFAULT_DB[0], help="host of the scratch database server")
    parser.add_argument("--db-port", type=int, default=DEFAULT_DB[1], help="port of the scratch database server")
    parser.add_argument("--db-user", default=DEFAULT_DB[2], help="user on the scratch database server")
    parser.add_argument("--db-password", default=DEFAULT_DB[3], help="password on the scratch database server")
    parser.add_argument("--db-prefix", default=DEFAULT_DB[4], help="name prefix of the databases the run owns; it must start with ambrose_driver_")
    parser.add_argument("--wsl-distro", help="WSL distribution whose MariaDB service to start when nothing answers")
    parser.add_argument("--references", default=paths.REFERENCES, help="the reference file describing the screens and presses")
    parser.add_argument("--refs", default=paths.refs_root(), help="folder holding the reference crops, which are never committed")
    parser.add_argument("--runs", default=paths.run_root(), help="folder the run writes its report, screenshots and logs into")
    parser.add_argument("--no-capture", dest="capture", action="store_false", help="do not capture the loopback traffic")
    parser.add_argument("--server-timeout", type=float, default=300, help="seconds to wait for the login server to report itself ready")
    parser.add_argument("--client-timeout", type=float, default=180, help="seconds to wait for the client and its window")


def options_of(args, need_crops=True):
    return {
        "binaries": args.binaries,
        "client": args.client or os.environ.get("AMBROSE_CLIENT_DIR") or None,
        "locale": args.locale,
        "host": args.host,
        "port": args.port,
        "game_port": args.game_port,
        "window": None,
        "db_host": args.db_host,
        "db_port": args.db_port,
        "db_user": args.db_user,
        "db_password": args.db_password,
        "db_prefix": args.db_prefix,
        "wsl": args.wsl_distro,
        "refs": args.refs,
        "runs": args.runs,
        "capture": args.capture,
        "need_crops": need_crops,
        "server_timeout": args.server_timeout,
        "client_timeout": args.client_timeout,
        "set": list(getattr(args, "set", None) or []),
        "label": getattr(args, "label", None),
        "user": getattr(args, "user", None),
        "wizard_from": getattr(args, "wizard_from", None),
        "wizard_guid": getattr(args, "wizard_guid", None),
        "background": getattr(args, "background", True),
        "replace": getattr(args, "replace", False),
    }


def load(args):
    scenario = scenarios.load(args.scenario, search=(paths.SCENARIOS, os.getcwd()))
    references = refs.load(args.references)
    options = options_of(args, need_crops=args.command != "capture-refs")
    options["window"] = f"{references.window[0]}x{references.window[1]}"
    return scenario, references, options


def start_database(options):
    from .database import Scratch

    scratch = Scratch(options["db_host"], options["db_port"], options["db_user"], options["db_password"], options["db_prefix"])
    if scratch.answers() or not options.get("wsl"):
        return None
    return scratch.start_in_wsl(options["wsl"])


def look(args):
    scenario, references, options = load(args)
    said = start_database(options)
    if said:
        print(f"clientdriver: the scratch database server {said}")
    environment = preflight.probe(scenario, references, options)
    return scenario, references, options, environment, preflight.missing(scenario, environment, options)


def command_check(args):
    _scenario, references, _options, environment, gaps = look(args)
    if gaps:
        print("clientdriver: skipped: " + "; ".join(gaps))
        return SKIP
    print(f"clientdriver: a run is possible: the install {environment['install']} ({environment['revision']}), "
          f"the programs in {environment['binaries']}, the database on {environment['database']}, "
          f"the references {references.key}"
          + (f", and tshark at {environment['tshark']}" if environment.get("capture") else ", without a capture"))
    return OK


def command_run(args):
    scenario, references, options, environment, gaps = look(args)
    if gaps:
        print("clientdriver: skipped: " + "; ".join(gaps))
        return SKIP
    from .run import Run

    return Run(options, scenario, references, environment).execute()


def command_capture_refs(args):
    scenario, references, options, environment, gaps = look(args)
    if gaps:
        print("clientdriver: skipped: " + "; ".join(gaps))
        return SKIP
    from .refscapture import CaptureRun

    return CaptureRun(options, scenario, references, environment).execute()


def command_scenarios(args):
    folder = paths.SCENARIOS
    for name in sorted(os.listdir(folder)):
        if not name.endswith(".json"):
            continue
        try:
            scenario = scenarios.load(name, search=(folder,))
        except Refused as error:
            print(f"{name}: {error}")
            continue
        print(f"{name}: {scenario.title}")
        print(f"  {len(scenario.steps)} step(s), " + ("needs the client" if scenario.needs_client else "needs no client")
              + (" and a capture" if scenario.needs_capture else "")
              + (f", screens {', '.join(scenario.screens_used())}" if scenario.screens_used() else ""))
    return OK


def build_parser():
    parser = argparse.ArgumentParser(prog="drive.py", description="Drives the user's own Wizard101 client against a scratch Ambrose login server.")
    commands = parser.add_subparsers(dest="command", required=True)
    run = commands.add_parser("run", help="run one scenario end to end")
    add_common(run)
    run.add_argument("--set", action="append", help="one more login server option, as Key=Value; may repeat")
    run.add_argument("--label", help="what this run checks, so a milestone can cite its run id")
    run.add_argument("--user", help="account name the run creates and logs in with")
    run.add_argument("--wizard-from", help="a characters database, host;port;user;password;database, to copy the scenario's wizard from instead of the one it describes; it is only read")
    run.add_argument("--wizard-guid", type=int, help="the wizard --wizard-from copies (default 1)")
    run.add_argument("--foreground", dest="background", action="store_false", help="leave the client window in front instead of at the bottom")
    check = commands.add_parser("check", help="say whether a run is possible on this machine, and exit 77 when it is not")
    add_common(check)
    capture = commands.add_parser("capture-refs", help="rebuild the reference crops for the install in front of the driver")
    add_common(capture)
    capture.add_argument("--set", action="append", help="one more login server option, as Key=Value; may repeat")
    capture.add_argument("--user", help="account name the run creates and logs in with")
    capture.add_argument("--foreground", dest="background", action="store_false", help="leave the client window in front instead of at the bottom")
    capture.add_argument("--replace", action="store_true", help="take every new crop, even one that does not look like the crop it replaces")
    commands.add_parser("scenarios", help="list the scenarios and what each one needs")
    return parser


def main(argv=None):
    parser = build_parser()
    args = parser.parse_args(argv)
    handlers = {"run": command_run, "check": command_check, "capture-refs": command_capture_refs, "scenarios": command_scenarios}
    try:
        return handlers[args.command](args)
    except Refused as error:
        print(f"clientdriver: {error}", file=sys.stderr)
        return BAD_USAGE
    except KeyboardInterrupt:
        print("clientdriver: stopped", file=sys.stderr)
        return FAILED
