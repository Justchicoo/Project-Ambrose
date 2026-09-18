# Project Ambrose by Imjustchico
# Captures the run's loopback traffic with tshark and finalizes the file the only way tshark does, a console control event on its own hidden console, so the capture is readable even when a run fails, and ends what it started when tshark never reports itself capturing.
import os
import re
import subprocess
import sys
import time

from .logtail import LogTail

NO_WINDOW = 0x08000000
LOOPBACK_DEVICE = r"\Device\NPF_Loopback"
FRAME_COUNT = re.compile(r"\|\s*0\.0+\s*<>\s*\S+\s*\|\s*(\d+)")
ATTACH = ("import ctypes\n"
          "kernel = ctypes.windll.kernel32\n"
          "kernel.FreeConsole()\n"
          "kernel.AttachConsole({pid}) and kernel.GenerateConsoleCtrlEvent(1, 0)\n")


class Capture:
    def __init__(self, tshark, path, port, device=LOOPBACK_DEVICE):
        self.tshark = tshark
        self.path = path
        self.port = port
        self.device = device
        self.process = None
        self.frames = None
        self.note = None
        self._errors = None

    def start(self, timeout=30):
        if not self.tshark:
            self.note = "tshark is not installed, so no capture was taken"
            return self.note
        os.makedirs(os.path.dirname(self.path), exist_ok=True)
        self._errors = open(self.path + ".tshark.txt", "wb")
        command = [self.tshark, "-i", self.device, "-f", f"tcp port {self.port}", "-w", self.path]
        self.process = subprocess.Popen(command, stdout=self._errors, stderr=subprocess.STDOUT, creationflags=NO_WINDOW)
        tail = LogTail(self.path + ".tshark.txt")
        try:
            tail.wait(r"Capturing on", timeout, alive=lambda: self.process.poll() is None)
        except Exception as error:
            ended = self.end()
            self.close()
            self.process = None
            self.note = f"tshark did not start, and what had started was stopped by {ended}: {error}: {' '.join(tail.lines)}"
            return self.note
        self.note = f"capturing loopback tcp port {self.port}"
        return self.note

    def end(self):
        if not self.process or self.process.poll() is not None:
            return "nothing, because it had already stopped"
        try:
            subprocess.run([sys.executable, "-c", ATTACH.format(pid=self.process.pid)], creationflags=NO_WINDOW, timeout=15)
        except (OSError, subprocess.SubprocessError):
            pass
        try:
            self.process.wait(15)
            return "a console control event"
        except subprocess.TimeoutExpired:
            from .netguard import kill_tree

            kill_tree(self.process.pid)
            return "a kill, so the capture may be cut short"

    def close(self):
        if self._errors:
            self._errors.close()
            self._errors = None

    def stop(self):
        if not self.process or self.process.poll() is not None:
            self.close()
            return self.note or "it was never started"
        time.sleep(1.0)
        how = self.end()
        self.close()
        self.frames = self.count_frames()
        self.note = f"tshark stopped by {how}; {self.frames} frame(s) in {os.path.basename(self.path)}"
        return self.note

    def count_frames(self):
        try:
            finished = subprocess.run([self.tshark, "-r", self.path, "-q", "-z", "io,stat,0"], capture_output=True,
                                      text=True, timeout=120, creationflags=NO_WINDOW)
        except (OSError, subprocess.SubprocessError):
            return None
        found = FRAME_COUNT.search(finished.stdout or "")
        return int(found.group(1)) if found else None

    def facts(self):
        return {"path": self.path if self.process else None, "frames": self.frames, "note": self.note}
