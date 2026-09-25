# Project Ambrose by Imjustchico
# The zone rows a run's world database needs before its game server starts: extracted once per client revision by the zone extractor from the user's own install into the Ambrose data folder, never into the repository, and read from there on every later run, because extracting every zone costs about a minute and the rows only change when the install does.
import os
import subprocess

from . import paths
from .errors import StepFailed

NO_WINDOW = 0x08000000


def cache_path(revision):
    return os.path.join(paths.driver_folder(), "zones", f"{revision}.sql")


def ensure(binaries, install, revision, timeout=1800):
    if not revision:
        raise StepFailed("the install's revision is not known, so its zone rows cannot be cached under it")
    target = cache_path(revision)
    if os.path.isfile(target) and os.path.getsize(target) > 0:
        return target, f"read the zone rows of {revision} from {target}"
    extractor = os.path.join(binaries, paths.program("zone_extractor"))
    if not os.path.isfile(extractor):
        raise StepFailed(f"{extractor} is missing; build the zone extractor first")
    dump = os.path.join(paths.data_folder(), "types", f"{revision}.json")
    if not os.path.isfile(dump):
        raise StepFailed(f"the type dump {dump} is missing; start a server once so it is built")
    os.makedirs(os.path.dirname(target), exist_ok=True)
    partial = target + ".part"
    command = [extractor, "--client", install, "--type-dump", dump, "--sql", partial]
    try:
        finished = subprocess.run(command, capture_output=True, timeout=timeout, creationflags=NO_WINDOW)
    except (OSError, subprocess.SubprocessError) as error:
        raise StepFailed(f"the zone extractor could not run: {error}")
    if finished.returncode != 0 or not os.path.isfile(partial):
        said = (finished.stderr or finished.stdout or b"").decode("utf-8", "replace").strip().splitlines()
        raise StepFailed(f"the zone extractor exited with {finished.returncode}: {said[-1] if said else 'no output'}")
    os.replace(partial, target)
    return target, f"extracted the zone rows of {revision} from {install} into {target}"
