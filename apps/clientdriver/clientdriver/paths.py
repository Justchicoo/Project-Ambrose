# Project Ambrose by Imjustchico
# Finds everything a run needs on this machine: the repository, the built server and launcher, the shipped login server defaults, tshark, and the Ambrose data folder that holds the run output and the reference crops, none of which lie in the repository.
import os
import shutil
import sys

PACKAGE = os.path.dirname(os.path.abspath(__file__))
APP = os.path.dirname(PACKAGE)
REPOSITORY = os.path.dirname(os.path.dirname(APP))
SCENARIOS = os.path.join(APP, "scenarios")
REFERENCES = os.path.join(APP, "references.json")
BINARIES = ("loginserver", "launcher")
TSHARK_PLACES = (r"C:\Program Files\Wireshark\tshark.exe", r"C:\Program Files (x86)\Wireshark\tshark.exe")


def program(name):
    return name + ".exe" if sys.platform == "win32" else name


def data_folder():
    if sys.platform == "win32":
        local = os.environ.get("LOCALAPPDATA") or os.path.expanduser("~")
        return os.path.join(local, "ProjectAmbrose")
    share = os.environ.get("XDG_DATA_HOME")
    if share and os.path.isabs(share):
        return os.path.join(share, "project-ambrose")
    return os.path.join(os.path.expanduser("~"), ".local", "share", "project-ambrose")


def driver_folder():
    return os.path.join(data_folder(), "clientdriver")


def run_root():
    return os.path.join(driver_folder(), "runs")


def refs_root():
    return os.path.join(driver_folder(), "refs")


def holds_binaries(folder):
    return bool(folder) and all(os.path.isfile(os.path.join(folder, program(name))) for name in BINARIES)


def binary_candidates():
    build = os.path.join(REPOSITORY, "build")
    found = []
    for folder in sorted(os.listdir(build)) if os.path.isdir(build) else []:
        for configuration in ("RelWithDebInfo", "Debug"):
            found.append(os.path.join(build, folder, "bin", configuration))
        found.append(os.path.join(build, folder, "bin"))
    found.append(os.path.join(data_folder(), "Play", "server", "bin"))
    return found


def find_binaries(named=None):
    if named:
        folder = os.path.abspath(named)
        if holds_binaries(folder):
            return folder, ""
        return None, f"{folder} does not hold {' and '.join(program(name) for name in BINARIES)}"
    found = [folder for folder in binary_candidates() if holds_binaries(folder)]
    if not found:
        return None, (f"{' and '.join(program(name) for name in BINARIES)} are not built; build the server or name their folder with --binaries")
    found.sort(key=lambda folder: os.path.getmtime(os.path.join(folder, program("loginserver"))), reverse=True)
    return found[0], ""


def server_defaults(binaries):
    beside = os.path.join(binaries or "", "loginserver.conf.dist")
    if os.path.isfile(beside):
        return beside
    shipped = os.path.join(REPOSITORY, "src", "server", "apps", "loginserver", "loginserver.conf.dist")
    return shipped if os.path.isfile(shipped) else None


def tshark():
    found = shutil.which("tshark")
    if found:
        return found
    for candidate in TSHARK_PLACES:
        if os.path.isfile(candidate):
            return candidate
    return None
