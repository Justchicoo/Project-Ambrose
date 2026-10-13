# Project Ambrose by Imjustchico
# Lets driver runs share the machine: each run holds the first free slot, a lock file in the driver folder that gives it a login port, a game port and a database prefix no other run uses, and every press or held key waits for the one machine-wide input turn, because a press borrows the real cursor and the foreground, and the run counts how long it waited for it.
import contextlib
import os
import threading
import time

from . import paths
from .errors import Refused, StepFailed

SLOTS = 4
PORT_STEP = 10
INPUT_WAIT = 600.0
INPUT_POLL = 0.05

_input_turns = threading.RLock()
_held = []
_depth = []
_waits = {"turns": 0, "waited_seconds": 0.0, "longest_wait_seconds": 0.0}


def lock(handle):
    try:
        if os.name == "nt":
            import msvcrt

            handle.seek(0)
            msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
        else:
            import fcntl

            fcntl.flock(handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        return True
    except OSError:
        return False


def unlock(handle):
    try:
        if os.name == "nt":
            import msvcrt

            handle.seek(0)
            msvcrt.locking(handle.fileno(), msvcrt.LK_UNLCK, 1)
        else:
            import fcntl

            fcntl.flock(handle.fileno(), fcntl.LOCK_UN)
    except OSError:
        pass


def open_lock(path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    return open(path, "a+b")


def take(folder=None, count=SLOTS, wanted=None):
    where = os.path.join(folder or paths.driver_folder(), "slots")
    for index in range(count) if wanted is None else (wanted,):
        handle = open_lock(os.path.join(where, f"slot{index}.lock"))
        if lock(handle):
            _held.append(handle)
            return index
        handle.close()
    if wanted is not None:
        raise Refused(f"driver slot {wanted} is taken by another run on this machine")
    raise Refused(f"all {count} driver slots are taken by other runs on this machine")


def release():
    while _held:
        handle = _held.pop()
        unlock(handle)
        handle.close()


def shifted(options, slot, port, game_port, prefix):
    if not slot:
        return options
    changed = dict(options)
    if changed["port"] == port:
        changed["port"] = port + PORT_STEP * slot
    if changed["game_port"] == game_port:
        changed["game_port"] = game_port + PORT_STEP * slot
    if changed["db_prefix"] == prefix:
        changed["db_prefix"] = f"{prefix}{slot}"
    return changed


def input_waits():
    return {"turns": _waits["turns"], "waited_seconds": round(_waits["waited_seconds"], 2),
            "longest_wait_seconds": round(_waits["longest_wait_seconds"], 2)}


@contextlib.contextmanager
def input_turn(folder=None, wait=INPUT_WAIT, poll=INPUT_POLL):
    with _input_turns:
        if _depth:
            _depth.append(True)
            try:
                yield
            finally:
                _depth.pop()
            return
        handle = open_lock(os.path.join(folder or paths.driver_folder(), "input.lock"))
        try:
            started = time.monotonic()
            deadline = started + wait
            while not lock(handle):
                if time.monotonic() > deadline:
                    raise StepFailed(f"another driver run held the keyboard and mouse for more than {wait:.0f}s")
                time.sleep(poll)
            waited = time.monotonic() - started
            _waits["turns"] += 1
            _waits["waited_seconds"] += waited
            _waits["longest_wait_seconds"] = max(_waits["longest_wait_seconds"], waited)
            _depth.append(True)
            try:
                yield
            finally:
                _depth.pop()
                unlock(handle)
        finally:
            handle.close()
