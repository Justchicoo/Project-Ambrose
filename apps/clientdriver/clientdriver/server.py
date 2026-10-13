# Project Ambrose by Imjustchico
# Runs the scratch servers a scenario or a dev run drives, on Windows or Linux: the login server, and the game server when the scenario enters the world, each on its own port with the run's databases and its own logs inside the run folder, started from the shipped defaults with every difference passed as a command-line override, with its admin API on a port of the run's own when asked for and its health read there, the login server's account created over its own console and given the security level asked for and its world rows reloaded there once the game server has written them, the game server announcing its realm at the run's own address, a connection to either server's port read up to the SessionOffer frame the server sends first, and each shutdown waited for, with the process killed by its id when it ignores the shutdown command.
import importlib.util
import json
import os
import re
import shutil
import socket
import struct
import subprocess
import sys
import time
import urllib.error
import urllib.request

from .errors import StepFailed
from .logtail import LogTail

NO_WINDOW = 0x08000000
FRAME_MAGIC = 0xF00D
FRAME_PREFIX = struct.Struct("<HH")
LONG_FRAME = 0x8000
SESSION_OFFER_OPCODE = 0
SESSION_OFFER_BODY = 14
DIRECT = urllib.request.build_opener(urllib.request.ProxyHandler({}))


def window_flags(platform=None):
    return NO_WINDOW if (platform or sys.platform) == "win32" else 0


def receive(connection, count, what):
    data = b""
    while len(data) < count:
        try:
            part = connection.recv(count - len(data))
        except OSError as error:
            raise StepFailed(f"{what} was cut off after {len(data)} of {count} byte(s): {error}")
        if not part:
            raise StepFailed(f"{what} ended after {len(data)} of {count} byte(s)")
        data += part
    return data


def read_session_offer(connection, what):
    magic, length = FRAME_PREFIX.unpack(receive(connection, FRAME_PREFIX.size, what))
    if magic != FRAME_MAGIC:
        raise StepFailed(f"{what} began with {magic:#06x} rather than the frame magic {FRAME_MAGIC:#06x}")
    if length >= LONG_FRAME:
        raise StepFailed(f"{what} began with a long frame, and a SessionOffer is always short")
    frame = receive(connection, length, what)
    if length < 4 + SESSION_OFFER_BODY + 1 or frame[0] != 1 or frame[1] != SESSION_OFFER_OPCODE:
        raise StepFailed(f"{what} began with a frame that is not a SessionOffer: control {frame[0] if frame else None}, opcode {frame[1] if len(frame) > 1 else None}, {length} byte(s)")
    session = struct.unpack_from("<H", frame, 4)[0]
    if session == 0:
        raise StepFailed(f"{what} offered session 0, which no client can accept")
    return session


class LoginServer:
    WHAT = "the login server"
    READY = r"loginserver ready"
    LOG_FILE = "Login.log"
    CONFIG = "loginserver.conf"
    ADMIN_OFF = ()

    def __init__(self, program, defaults, folder, host, port, databases, settings=(), admin_port=None, environment=None):
        self.program = program
        self.defaults = defaults
        self.folder = folder
        self.host = host
        self.port = int(port)
        self.databases = databases
        self.settings = list(settings)
        self.admin_port = None if admin_port is None else int(admin_port)
        self.environment = environment
        self.token_path = os.path.join(folder, "admin.token")
        self.config = os.path.join(folder, self.CONFIG)
        self.console_path = os.path.join(folder, "console.txt")
        self.process = None
        self.console = LogTail(self.console_path)
        self.log = LogTail(os.path.join(folder, self.LOG_FILE))
        self._output = None

    def shared_overrides(self):
        return [
            f"LogsDir={self.folder}",
            f"BindIP={self.host}",
            f"LoginDatabaseInfo={self.databases.info('login')}",
            f"CharacterDatabaseInfo={self.databases.info('characters')}",
            f"WorldDatabaseInfo={self.databases.info('world')}",
            "Console.Enable=1",
            "Console.Colors=0",
        ]

    def admin_overrides(self):
        if self.admin_port is None:
            return list(self.ADMIN_OFF)
        return ["Admin.Enable=1", f"Admin.BindIP={self.host}", f"Admin.Port={self.admin_port}", f"Admin.TokenFile={self.token_path}"]

    def overrides(self):
        return self.shared_overrides() + [f"LoginServerPort={self.port}"] + self.admin_overrides() + self.settings

    def prepare(self):
        os.makedirs(self.folder, exist_ok=True)
        shutil.copyfile(self.defaults, self.config + ".dist")
        with open(self.config, "w", encoding="utf-8", newline="\n") as handle:
            handle.write("# Project Ambrose by Imjustchico\n"
                         f"# Written by the client driver for one run; every other value comes from {self.CONFIG}.dist beside it.\n")
        return self.config

    def command(self):
        arguments = [self.program, "-c", self.config]
        for override in self.overrides():
            arguments += ["--set", override]
        return arguments

    def start(self, timeout=300):
        self.prepare()
        if not os.path.isfile(self.program):
            raise StepFailed(f"{self.program} is missing; build the server first")
        self.log.begin()
        self._output = open(self.console_path, "wb")
        self.process = subprocess.Popen(self.command(), cwd=self.folder, stdin=subprocess.PIPE, stdout=self._output,
                                        stderr=subprocess.STDOUT, creationflags=window_flags(), env=self.environment)
        self.console.wait(self.READY, timeout, fail=r"\bFATAL\b|^ERROR\s*:", alive=self.alive)
        self.log.mark()
        return f"ready on {self.host}:{self.port} as process {self.process.pid}"

    def alive(self):
        return self.process is not None and self.process.poll() is None

    def send(self, line):
        if not self.alive():
            raise StepFailed(f"{self.WHAT} is not running, so it cannot be given a command")
        self.process.stdin.write((line + "\n").encode("utf-8"))
        self.process.stdin.flush()

    def ensure_account(self, user, password, timeout=60):
        self.send(f"account create {user} {password}")
        answer = self.console.wait(
            rf"Account {re.escape(user)} (created with id \d+|already exists|was not created: .*)", timeout, alive=self.alive)
        said = answer.group(1)
        if said.startswith("created"):
            return f"{user} {said}"
        if said.startswith("was not created"):
            raise StepFailed(f"the account {user} {said}")
        self.send(f"account set password {user} {password}")
        changed = self.console.wait(rf"Password of {re.escape(user)} (changed|not changed: .*)", timeout, alive=self.alive)
        if changed.group(1) != "changed":
            raise StepFailed(f"the account {user} exists and its password could not be set: {changed.group(1)}")
        return f"{user} already existed, and its password was set again"

    def set_gm_level(self, user, level, timeout=60):
        self.send(f"account set gmlevel {user} {int(level)}")
        answer = self.console.wait(rf"Security level of {re.escape(user)} (set to \d+|not changed: .*)", timeout, alive=self.alive)
        if not answer.group(1).startswith("set to"):
            raise StepFailed(f"the account {user}'s security level was {answer.group(1)}")
        return f"{user}'s security level {answer.group(1)}"

    def reload(self, target, timeout=60):
        self.send(f"reload {target}")
        answer = self.console.wait(rf"\b{re.escape(target)} (is now generation \d+|was not reloaded.*)", timeout, alive=self.alive)
        if not answer.group(1).startswith("is now"):
            raise StepFailed(f"{self.WHAT} could not reload {target}: {answer.group(1)}")
        return f"{target} {answer.group(1)}"

    def stop(self, timeout=60):
        if self.process is None:
            return "it was never started"
        if not self.alive():
            self._close()
            return f"it had already stopped with {self.process.returncode}"
        try:
            self.send("shutdown")
        except (StepFailed, OSError):
            pass
        try:
            self.process.wait(timeout)
            how = "the shutdown command"
        except subprocess.TimeoutExpired:
            self.kill()
            self.process.wait(10)
            how = f"a kill of process {self.process.pid}, because it ignored the shutdown command"
        self._close()
        return f"stopped by {how} with {self.process.returncode}"

    def kill(self):
        if importlib.util.find_spec("psutil") is None:
            self.process.kill()
            return
        from .netguard import kill_tree

        kill_tree(self.process.pid)

    def health(self, timeout=30, opener=DIRECT.open):
        if self.admin_port is None:
            raise StepFailed(f"{self.WHAT} was started without its admin API")
        address = f"http://{self.host}:{self.admin_port}/api/health"
        deadline = time.monotonic() + timeout
        seen = "no answer"
        while True:
            try:
                with opener(address, timeout=5) as answer:
                    body = json.loads(answer.read().decode("utf-8"))
                if body.get("state") == "running":
                    return f"{address} answered {body.get('app')} running for {body.get('uptime')}s at revision {body.get('revision')}"
                seen = f"state {body.get('state')}"
            except (OSError, ValueError, urllib.error.URLError) as error:
                seen = str(error)
            if not self.alive() or time.monotonic() > deadline:
                raise StepFailed(f"{address} never said {self.WHAT} was running: {seen}")
            time.sleep(0.5)

    def session_offer(self, timeout=10, connect=socket.create_connection):
        what = f"the first bytes from {self.WHAT} on {self.host}:{self.port}"
        try:
            connection = connect((self.host, self.port), timeout)
        except OSError as error:
            raise StepFailed(f"{self.WHAT} did not accept a connection on {self.host}:{self.port}: {error}")
        with connection:
            connection.settimeout(timeout)
            session = read_session_offer(connection, what)
        return f"{self.host}:{self.port} accepted a connection and offered session {session}"

    def _close(self):
        if self._output:
            self._output.close()
            self._output = None


class GameServer(LoginServer):
    WHAT = "the game server"
    READY = r"gameserver ready"
    LOG_FILE = "Server.log"
    CONFIG = "gameserver.conf"
    ADMIN_OFF = ("Admin.Enable=0",)

    def overrides(self):
        return self.shared_overrides() + [f"WorldServerPort={self.port}", f"Realm.Address={self.host}"] + self.admin_overrides() + self.settings
