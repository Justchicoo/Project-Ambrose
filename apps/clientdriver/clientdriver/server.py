# Project Ambrose by Imjustchico
# Runs the scratch login server a scenario drives: its own port, its own databases and its own logs inside the run folder, started from the shipped defaults with every difference passed as a command-line override, with its account created over its own console and its shutdown waited for.
import os
import re
import shutil
import subprocess

from .errors import StepFailed
from .logtail import LogTail

NO_WINDOW = 0x08000000
READY = r"loginserver ready"
GENERATED = ("# Project Ambrose by Imjustchico\n"
             "# Written by the client driver for one run; every other value comes from loginserver.conf.dist beside it.\n")


class LoginServer:
    def __init__(self, program, defaults, folder, host, port, databases, settings=()):
        self.program = program
        self.defaults = defaults
        self.folder = folder
        self.host = host
        self.port = int(port)
        self.databases = databases
        self.settings = list(settings)
        self.config = os.path.join(folder, "loginserver.conf")
        self.console_path = os.path.join(folder, "console.txt")
        self.process = None
        self.console = LogTail(self.console_path)
        self.log = LogTail(os.path.join(folder, "Login.log"))
        self._output = None

    def overrides(self):
        return [
            f"LogsDir={self.folder}",
            f"BindIP={self.host}",
            f"LoginServerPort={self.port}",
            f"LoginDatabaseInfo={self.databases.info('login')}",
            f"CharacterDatabaseInfo={self.databases.info('characters')}",
            "Console.Enable=1",
            "Console.Colors=0",
        ] + self.settings

    def prepare(self):
        os.makedirs(self.folder, exist_ok=True)
        shutil.copyfile(self.defaults, self.config + ".dist")
        with open(self.config, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(GENERATED)
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
        self._output = open(self.console_path, "wb")
        self.process = subprocess.Popen(self.command(), cwd=self.folder, stdin=subprocess.PIPE, stdout=self._output,
                                        stderr=subprocess.STDOUT, creationflags=NO_WINDOW)
        self.console.wait(READY, timeout, fail=r"\bFATAL\b|^ERROR\s*:", alive=self.alive)
        self.log.mark()
        return f"ready on {self.host}:{self.port} as process {self.process.pid}"

    def alive(self):
        return self.process is not None and self.process.poll() is None

    def send(self, line):
        if not self.alive():
            raise StepFailed("the login server is not running, so it cannot be given a command")
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
            from .netguard import kill_tree

            kill_tree(self.process.pid)
            self.process.wait(10)
            how = "a kill, because it ignored the shutdown command"
        self._close()
        return f"stopped by {how} with {self.process.returncode}"

    def _close(self):
        if self._output:
            self._output.close()
            self._output = None
