# Project Ambrose by Imjustchico
# Walks the user's install before and after a run and names every file added, removed or changed, so a run proves it only read the install.
import os


def snapshot(root):
    found = {}
    if not root or not os.path.isdir(root):
        return found
    for folder, _folders, files in os.walk(root):
        for name in files:
            path = os.path.join(folder, name)
            relative = os.path.relpath(path, root).replace(os.sep, "/")
            try:
                status = os.stat(path)
            except OSError:
                found[relative] = None
                continue
            found[relative] = [status.st_size, status.st_mtime_ns]
    return found


def diff(before, after):
    return {
        "added": sorted(name for name in after if name not in before),
        "removed": sorted(name for name in before if name not in after),
        "changed": sorted(name for name in after if name in before and after[name] != before[name]),
    }


def count(difference):
    return sum(len(names) for names in difference.values())
