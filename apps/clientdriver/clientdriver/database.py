# Project Ambrose by Imjustchico
# The scratch databases a run owns: it touches only names starting with ambrose_driver_, lets the server's own updater create them, reads a value for an assertion, and drops them when the run ends.
import socket
import subprocess
import time

from .errors import Refused, StepFailed

PREFIX = "ambrose_driver_"
KINDS = ("login", "characters", "world")
NO_WINDOW = 0x08000000


def checked_name(name):
    if not name.startswith(PREFIX) or len(name) <= len(PREFIX):
        raise Refused(f"the driver may only use databases named {PREFIX}<something>, not {name!r}")
    if not all(character.isalnum() or character == "_" for character in name):
        raise Refused(f"{name!r} is not a plain database name")
    return name


class Scratch:
    def __init__(self, host, port, user, password, prefix=PREFIX + "run"):
        self.host = host
        self.port = int(port)
        self.user = user
        self.password = password
        self.names = {kind: checked_name(f"{prefix}_{kind}") for kind in KINDS}

    @property
    def address(self):
        return f"{self.host}:{self.port}"

    def info(self, kind):
        return f"{self.host};{self.port};{self.user};{self.password};{self.names[kind]}"

    def answers(self, timeout=2.0):
        try:
            with socket.create_connection((self.host, self.port), timeout):
                return True
        except OSError:
            return False

    def start_in_wsl(self, distribution, timeout=90):
        if self.answers():
            return "it was already answering"
        command = ["wsl.exe", "-d", distribution, "-u", "root", "--", "bash", "-c", "service mariadb start"]
        try:
            subprocess.run(command, capture_output=True, timeout=180, creationflags=NO_WINDOW)
        except (OSError, subprocess.SubprocessError) as error:
            raise StepFailed(f"MariaDB does not answer on {self.host}:{self.port} and WSL could not start it: {error}")
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self.answers():
                return f"started in the WSL distribution {distribution}"
            time.sleep(1.0)
        raise StepFailed(f"MariaDB did not come up on {self.host}:{self.port} within {timeout}s")

    def _connect(self, database=None):
        import pymysql

        return pymysql.connect(host=self.host, port=self.port, user=self.user, password=self.password,
                               database=database, connect_timeout=10)

    def existing(self):
        connection = self._connect()
        try:
            with connection.cursor() as cursor:
                cursor.execute("SHOW DATABASES")
                return sorted(row[0] for row in cursor.fetchall() if row[0] in self.names.values())
        finally:
            connection.close()

    def drop(self):
        connection = self._connect()
        try:
            with connection.cursor() as cursor:
                for name in self.names.values():
                    cursor.execute(f"DROP DATABASE IF EXISTS `{checked_name(name)}`")
            connection.commit()
        finally:
            connection.close()
        return "dropped " + ", ".join(sorted(self.names.values()))

    def value(self, kind, query):
        if kind not in self.names:
            raise Refused(f"the driver has no {kind} database; it has {', '.join(self.names)}")
        connection = self._connect(self.names[kind])
        try:
            with connection.cursor() as cursor:
                cursor.execute(query)
                row = cursor.fetchone()
                return None if row is None else row[0]
        finally:
            connection.close()
