# Project Ambrose by Imjustchico
# A play session for a person rather than a scenario: it keeps its own databases from one session to the next, starts the login server, loads the zone rows into an empty world, starts the game server and has the login server reload the name tables and creation rows the game server wrote, makes sure of the player's account when one is named, starts the client through the launcher, and stays up until stop is typed, Ctrl+C is pressed or another `play --stop` asks it to, while a second `play` against a running session only starts another client.
import json
import os
import subprocess
import threading
import time

from . import paths, zones
from .database import Scratch, hold_wsl
from .errors import Refused, StepFailed
from .server import GameServer, LoginServer

PREFIX = "ambrose_driver_play"
NO_WINDOW = 0x08000000


def say(message):
    print(f"{time.strftime('%H:%M:%S')} play: {message}", flush=True)


def folder():
    return os.path.join(paths.driver_folder(), "play")


def state_path(where=None):
    return os.path.join(where or folder(), "session.json")


def stop_path(where=None):
    return os.path.join(where or folder(), "stop")


def read_state(where=None):
    try:
        with open(state_path(where), "r", encoding="utf-8") as handle:
            return json.load(handle)
    except (OSError, ValueError):
        return None


def answers(host, port, timeout=2.0):
    import socket

    try:
        with socket.create_connection((host, int(port)), timeout):
            return True
    except OSError:
        return False


def running(where=None, probe=answers):
    state = read_state(where)
    if state and probe(state["host"], state["port"]):
        return state
    return None


def ask_to_stop(where=None, timeout=120, probe=answers, sleep=time.sleep):
    state = running(where, probe)
    if state is None:
        return "no play session is running"
    with open(stop_path(where), "w", encoding="utf-8") as handle:
        handle.write("stop\n")
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if read_state(where) is None:
            return f"the play session on {state['host']}:{state['port']} stopped"
        sleep(0.5)
    raise StepFailed(f"the play session on {state['host']}:{state['port']} did not stop within {timeout}s")


def launch_client(binaries, host, port, client_dir=None, locale=None, window=None, start=subprocess.Popen):
    command = [os.path.join(binaries, paths.program("launcher")), "--host", host, "--port", str(port)]
    if window:
        command += ["--window", window]
    if client_dir:
        command += ["--client", client_dir]
    if locale:
        command += ["--locale", locale]
    finished = start(command, stdin=subprocess.DEVNULL, creationflags=NO_WINDOW)
    return f"the launcher is starting the client as process {finished.pid}"


class PlaySession:
    def __init__(self, options, environment, where=None, login_class=LoginServer, game_class=GameServer, databases=None, launch=launch_client):
        self.options = options
        self.environment = environment
        self.where = where or folder()
        self.databases = databases or Scratch(options["db_host"], options["db_port"], options["db_user"], options["db_password"], PREFIX)
        binaries = environment["binaries"]
        self.login = login_class(os.path.join(binaries, paths.program("loginserver")), environment["server_defaults"],
                                 os.path.join(self.where, "login"), options["host"], options["port"], self.databases,
                                 settings=list(options.get("set") or []))
        self.game = game_class(os.path.join(binaries, paths.program("gameserver")), os.path.join(binaries, "gameserver.conf.dist"),
                               os.path.join(self.where, "game"), options["host"], options["game_port"], self.databases,
                               settings=list(options.get("game_set") or []))
        self.launch = launch
        self.stopping = threading.Event()

    def note(self, what, value):
        say(f"{what}: {value}")
        return value

    def zone_rows(self):
        held = self.databases.value("world", "SELECT COUNT(*) FROM zone_template")
        if held and int(held) > 0:
            return f"the world database already holds {int(held)} zone(s)"
        sql, how = zones.ensure(self.environment["binaries"], self.environment.get("install"), self.environment.get("revision"),
                                self.databases.info("world"))
        self.note("the zone rows", how)
        return self.databases.apply_sql("world", sql)

    def start(self):
        options = self.options
        os.makedirs(self.where, exist_ok=True)
        if os.path.exists(stop_path(self.where)):
            os.remove(stop_path(self.where))
        self.note("the databases", f"{self.databases.address} keeps {', '.join(sorted(self.databases.names.values()))} between sessions")
        self.note("the login server", self.login.start(timeout=options["server_timeout"]))
        if options.get("user"):
            if not options.get("password"):
                raise Refused(f"the account {options['user']} needs a password: pass --password-env naming a variable that holds it, or start play from a terminal to be asked")
            self.note("the account", self.login.ensure_account(options["user"], options["password"]))
        self.note("the world database", self.zone_rows())
        self.note("the game server", self.game.start(timeout=options["server_timeout"]))
        self.note("the login server's name tables", self.login.reload("names"))
        self.note("the login server's creation rows", self.login.reload("creation"))
        with open(state_path(self.where), "w", encoding="utf-8") as handle:
            json.dump({"host": options["host"], "port": options["port"], "game_port": options["game_port"], "started": time.time()}, handle)

    def watch_console(self, read=input):
        def listen():
            while not self.stopping.is_set():
                try:
                    line = read()
                except (EOFError, OSError):
                    return
                if line.strip().lower() in ("stop", "quit", "exit"):
                    self.stopping.set()
                    return

        threading.Thread(target=listen, daemon=True).start()

    def wait(self, tick=1.0):
        while not self.stopping.is_set():
            if os.path.exists(stop_path(self.where)):
                return "another play --stop asked it to"
            if not self.login.alive():
                return "the login server stopped by itself"
            if not self.game.alive():
                return "the game server stopped by itself"
            self.stopping.wait(tick)
        return "stop was typed"

    def stop(self):
        for what, server in (("the game server", self.game), ("the login server", self.login)):
            try:
                self.note(what, server.stop())
            except Exception as error:
                say(f"stopping {what} failed: {error}")
        for path in (state_path(self.where), stop_path(self.where)):
            if os.path.exists(path):
                os.remove(path)

    def execute(self, client=True, read=input):
        try:
            self.start()
            if client:
                self.note("the client", self.launch(self.environment["binaries"], self.options["host"], self.options["port"],
                                                    self.options.get("client"), self.options.get("locale"), self.options.get("window")))
            say("the servers stay up until you type stop here, press Ctrl+C, or run play --stop")
            self.watch_console(read)
            why = self.wait()
            say(f"stopping, because {why}")
            return 0
        except KeyboardInterrupt:
            say("stopping, because Ctrl+C was pressed")
            return 0
        except (StepFailed, Refused) as error:
            say(f"could not start: {error}")
            return 1
        finally:
            self.stop()


def play(options, environment, client=True):
    state = running()
    if state is not None:
        say(f"a play session is already up on {state['host']}:{state['port']}")
        if client:
            say(launch_client(environment["binaries"], state["host"], state["port"], options.get("client"), options.get("locale"), options.get("window")))
        return 0
    hold_wsl(options.get("wsl"))
    scratch = Scratch(options["db_host"], options["db_port"], options["db_user"], options["db_password"], PREFIX)
    if not scratch.answers() and options.get("wsl"):
        say(f"the database server {scratch.start_in_wsl(options['wsl'])}")
    return PlaySession(options, environment, databases=scratch).execute(client=client)
