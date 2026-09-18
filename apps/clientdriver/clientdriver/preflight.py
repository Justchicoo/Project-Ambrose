# Project Ambrose by Imjustchico
# Decides whether a run is possible on this machine, by asking the launcher for the install, looking for the built programs, the scratch database, the capture and the reference crops, and naming in one line everything that is missing.
import importlib.util
import os
import re
import socket
import subprocess
import sys
import tempfile

from . import paths

PACKAGES = (("win32gui", "pywin32"), ("PIL", "pillow"), ("psutil", "psutil"), ("pymysql", "pymysql"),
            ("windows_capture", "windows-capture"), ("numpy", "numpy"))
INSTALL_LINE = re.compile(r"^launcher: install (.*) \((.*)\)$")
LOOPBACK_DEVICE = "NPF_Loopback"
NO_WINDOW = 0x08000000 if sys.platform == "win32" else 0


def creation_flags():
    return NO_WINDOW


def missing_packages():
    return [distribution for module, distribution in PACKAGES if importlib.util.find_spec(module) is None]


def ask_launcher(binaries, host, port, window, run_dir):
    program = os.path.join(binaries, paths.program("launcher"))
    command = [program, "--dry-run", "--host", host, "--port", str(port), "--window", window, "--run-dir", run_dir]
    try:
        finished = subprocess.run(command, capture_output=True, text=True, timeout=180, creationflags=creation_flags())
    except (OSError, subprocess.SubprocessError) as error:
        return None, None, f"{program} could not be run: {error}"
    for line in (finished.stdout or "").splitlines():
        found = INSTALL_LINE.match(line.strip())
        if found:
            return found.group(1), found.group(2), ""
    reason = " ".join(line.strip() for line in (finished.stderr or "").splitlines() if line.strip()) or "it said nothing"
    return None, None, f"the launcher found no Wizard101 install: {reason}"


def database_answers(host, port, timeout=2.0):
    try:
        with socket.create_connection((host, port), timeout):
            return True
    except OSError:
        return False


def capture_ready(tshark):
    if not tshark:
        return False, "tshark is not installed, so the run cannot capture its loopback traffic"
    try:
        finished = subprocess.run([tshark, "-D"], capture_output=True, text=True, timeout=60, creationflags=creation_flags())
    except (OSError, subprocess.SubprocessError) as error:
        return False, f"{tshark} could not list its interfaces: {error}"
    if LOOPBACK_DEVICE not in (finished.stdout or ""):
        return False, "Npcap has no loopback adapter, so the run cannot capture its loopback traffic"
    return True, ""


def reference_problems(scenario, references, folder, revision, need_crops=True):
    problems = []
    if references is None:
        return ["the reference file could not be read"]
    problems.extend(scenario.names_against(references))
    matches, reason = references.matches(revision)
    if not matches:
        problems.append(reason + f"; rebuild them with 'drive.py capture-refs' or point --references at a file for {revision}")
    wanted = scenario.screens_used()
    if wanted and need_crops:
        absent = references.missing_crops(folder, wanted)
        if absent:
            problems.append(f"the reference crops {', '.join(absent)} are not in {os.path.join(folder, references.folder_name)}; "
                            "they are client imagery, so they are never committed: rebuild them with 'drive.py capture-refs'")
    return problems


def probe(scenario, references, options):
    environment = {
        "platform": sys.platform,
        "windows": sys.platform == "win32",
        "packages_missing": missing_packages(),
        "database": f"{options['db_host']}:{options['db_port']}",
    }
    binaries, reason = paths.find_binaries(options.get("binaries"))
    environment["binaries"] = binaries
    environment["binaries_reason"] = reason
    environment["server_defaults"] = paths.server_defaults(binaries)
    if binaries:
        with tempfile.TemporaryDirectory(prefix="ambrose-clientdriver-") as folder:
            install, revision, install_reason = ask_launcher(binaries, options["host"], options["port"], options["window"], folder)
        environment["install"] = install
        environment["revision"] = revision
        environment["install_reason"] = install_reason
    else:
        environment["install"] = None
        environment["revision"] = None
        environment["install_reason"] = "the launcher is not built, so no install was looked for"
    environment["database_answers"] = database_answers(options["db_host"], options["db_port"])
    environment["tshark"] = paths.tshark()
    if options.get("capture", True) and scenario.needs_capture:
        environment["capture"], environment["capture_reason"] = capture_ready(environment["tshark"])
    else:
        environment["capture"] = False
        environment["capture_reason"] = ""
    environment["reference_problems"] = reference_problems(scenario, references, options["refs"], environment["revision"],
                                                           need_crops=options.get("need_crops", True))
    return environment


def missing(scenario, environment, options):
    gaps = []
    if not environment.get("windows"):
        gaps.append(f"the retail client runs only on Windows, and this machine is {environment.get('platform')}")
    absent = environment.get("packages_missing") or []
    if absent:
        gaps.append(f"the Python packages {', '.join(absent)} are not installed (pip install -r apps/clientdriver/requirements.txt)")
    if not environment.get("binaries"):
        gaps.append(environment.get("binaries_reason") or "the server and launcher are not built")
    if not environment.get("server_defaults"):
        gaps.append("loginserver.conf.dist was found neither beside the programs nor in the repository")
    elif not environment.get("install"):
        gaps.append(environment.get("install_reason") or "no Wizard101 install was found")
    if not environment.get("database_answers"):
        gaps.append(f"no database answers on {environment.get('database')}, and the driver uses only that one")
    if options.get("capture", True) and scenario.needs_capture and not environment.get("capture"):
        gaps.append(environment.get("capture_reason") or "the loopback capture is not available")
    if scenario.needs_client:
        gaps.extend(environment.get("reference_problems") or [])
    return gaps
