# Project Ambrose by Imjustchico
# The ports a scenario asks to watch: each listener binds its own address and port for the whole run, accepts every connection made to it and closes it at once, and records who connected and when, so a run can show that nothing reached a port the client should never reach, or that something did reach one it should; a port that is already taken is a refusal before the client starts rather than a listener that watches nothing.
import socket
import threading
import time

from .errors import StepFailed

ACCEPT_WAIT = 0.2


class PortListener(threading.Thread):
    def __init__(self, label, address, port, expect=0, at_least=False):
        super().__init__(daemon=True)
        self.label = label
        self.address = address
        self.port = int(port)
        self.expect = int(expect)
        self.at_least = bool(at_least)
        self.connections = []
        self.failed = None
        self._halt = threading.Event()
        self._socket = None

    def open(self):
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            server.bind((self.address, self.port))
            server.listen(16)
        except OSError as error:
            server.close()
            raise StepFailed(f"{self.label} cannot listen on {self.address}:{self.port}: {error}")
        server.settimeout(ACCEPT_WAIT)
        self.port = server.getsockname()[1]
        self._socket = server
        self.start()
        return f"{self.label} listening on {self.address}:{self.port}"

    def run(self):
        while not self._halt.is_set():
            try:
                connection, peer = self._socket.accept()
            except socket.timeout:
                continue
            except OSError as error:
                if not self._halt.is_set():
                    self.failed = f"it stopped accepting: {error}"
                return
            self.connections.append({"time": time.strftime("%H:%M:%S"), "peer": f"{peer[0]}:{peer[1]}"})
            connection.close()

    def stop(self):
        self._halt.set()
        if self._socket is not None:
            self._socket.close()
        if self.is_alive():
            self.join(2)
        return f"{self.label} on {self.address}:{self.port} saw {len(self.connections)} connection(s)"

    def record(self):
        return {"name": self.label, "address": self.address, "port": self.port, "expect": self.expect, "at_least": self.at_least,
                "connections": list(self.connections), "failed": self.failed}
