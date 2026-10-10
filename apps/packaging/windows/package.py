# Project Ambrose by Imjustchico
# Builds the Windows setup program from a release install: stages the servers and tools cmake --install laid out, the installer script, the licence and notices, the MariaDB server zip mariadb.json names after checking it against the SHA-256 pinned there and the one MariaDB publishes, and the Visual C++ runtime beside each program that loads it; refuses a stage holding anything outside its content list or anything from a client install; writes the manifest of every file with its size and SHA-256 and the uninstall list that removes exactly those files; and runs makensis on ambrose-setup.nsi, writing the setup and its SHA-256 beside it.
import argparse
import fnmatch
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
MANIFEST = "ambrose-manifest.json"
ROOT_FILES = {"ambrose.ps1", MANIFEST, "LICENSE.txt", "THIRD-PARTY-NOTICES.txt"}
CONTENT = ("bin/*", "etc/*.conf.dist", "share/ambrose/data/sql/*.sql", "mariadb/*")
SKIPPED = ("*.pdb", "*.ilk", "*.lib", "*.exp")
MARIADB_KEPT = ("bin/*.exe", "bin/*.dll", "share/*", "lib/plugin/*.dll")
MARIADB_SKIPPED = ("*.pdb", "*.lib", "bin/*test*", "bin/*embedded*", "bin/*.pl")
CLIENT_NAMES = ("*.wad", "*.nif", "*.kf", "*.kfm", "*.pcap", "*.pcapng", "wizardgraphicalclient*", "wizardlauncher*", "revision.dat",
                "patchconfig.xml", "*messages.xml", "latestfilelist*", "*.token", "*.log")


class PackageError(Exception):
    pass


def sha256_of(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def relative_files(folder):
    found = []
    for base, folders, files in os.walk(folder):
        folders.sort()
        for name in sorted(files):
            found.append(os.path.relpath(os.path.join(base, name), folder).replace(os.sep, "/"))
    return found


def matches(path, patterns):
    lowered = path.lower()
    return any(fnmatch.fnmatchcase(lowered, pattern) or fnmatch.fnmatchcase(lowered.rsplit("/", 1)[-1], pattern) for pattern in patterns)


def copy_tree(source, target, kept=("*",), skipped=()):
    for relative in relative_files(source):
        if not matches(relative, kept) or matches(relative, skipped):
            continue
        destination = os.path.join(target, *relative.split("/"))
        os.makedirs(os.path.dirname(destination), exist_ok=True)
        shutil.copy2(os.path.join(source, *relative.split("/")), destination)


def published_sum(text, name):
    for line in text.splitlines():
        parts = line.split()
        if len(parts) == 2 and parts[1].lstrip("*") == name:
            return parts[0].lower()
    raise PackageError(f"MariaDB's published checksums list no {name}")


def fetch(url, target):
    with urllib.request.urlopen(url, timeout=600) as response, open(target, "wb") as handle:
        shutil.copyfileobj(response, handle)


def mariadb_zip(spec, given, work, require_pin):
    name = spec["url"].rsplit("/", 1)[-1]
    path = given or os.path.join(work, name)
    if not given:
        print(f"download: MariaDB {spec['version']} from {spec['url']}")
        fetch(spec["url"], path)
    actual = sha256_of(path)
    pinned = spec.get("sha256", "").lower()
    if pinned and actual != pinned:
        raise PackageError(f"{name} has SHA-256 {actual}, not the {pinned} mariadb.json pins")
    if not given:
        sums = os.path.join(work, "sha256sums.txt")
        fetch(spec["sha256sums"], sums)
        with open(sums, encoding="utf-8") as handle:
            published = published_sum(handle.read(), name)
        if actual != published:
            raise PackageError(f"{name} has SHA-256 {actual}, not the {published} MariaDB publishes")
    if not pinned:
        if require_pin:
            raise PackageError(f"mariadb.json pins no SHA-256; pin {actual} before a release")
        print(f"note: mariadb.json pins no SHA-256; {name} has {actual}")
    return path


def stage_mariadb(archive, stage):
    unpacked = os.path.join(os.path.dirname(stage), "mariadb-unpacked")
    shutil.rmtree(unpacked, ignore_errors=True)
    with zipfile.ZipFile(archive) as bundle:
        for member in bundle.namelist():
            if member.startswith("/") or ".." in member.split("/"):
                raise PackageError(f"the MariaDB zip holds an unsafe path {member}")
        bundle.extractall(unpacked)
    tops = [entry for entry in os.listdir(unpacked) if os.path.isdir(os.path.join(unpacked, entry))]
    if len(tops) != 1 or not os.path.isfile(os.path.join(unpacked, tops[0], "bin", "mariadbd.exe")):
        raise PackageError("the MariaDB zip is not one folder holding bin/mariadbd.exe")
    source = os.path.join(unpacked, tops[0])
    copy_tree(source, os.path.join(stage, "mariadb"), MARIADB_KEPT, MARIADB_SKIPPED)
    for program in ("mariadbd.exe", "mariadb-install-db.exe", "mariadb-admin.exe", "mariadb.exe"):
        if not os.path.isfile(os.path.join(stage, "mariadb", "bin", program)):
            raise PackageError(f"the MariaDB zip has no bin/{program}")
    for name in ("COPYING", "README.md", "THIRDPARTY"):
        if os.path.isfile(os.path.join(source, name)):
            shutil.copy2(os.path.join(source, name), os.path.join(stage, "mariadb", name + (".txt" if "." not in name else "")))
    shutil.rmtree(unpacked, ignore_errors=True)


def stage_runtime(runtime, stage):
    dlls = sorted(name for name in os.listdir(runtime) if name.lower().endswith(".dll"))
    if not dlls:
        raise PackageError(f"{runtime} holds no Visual C++ runtime DLL")
    for folder in ("bin", os.path.join("mariadb", "bin")):
        for name in dlls:
            target = os.path.join(stage, folder, name)
            if not os.path.exists(target):
                shutil.copy2(os.path.join(runtime, name), target)
    return dlls


def text_copy(source, target):
    with open(source, encoding="utf-8") as handle:
        text = handle.read()
    with open(target, "w", encoding="utf-8", newline="\r\n") as handle:
        handle.write(text)


def stage_package(prefix, stage, archive, runtime):
    if os.path.exists(stage):
        shutil.rmtree(stage)
    os.makedirs(stage)
    for folder in ("bin", "etc", "share"):
        if not os.path.isdir(os.path.join(prefix, folder)):
            raise PackageError(f"{prefix} has no {folder} folder; run cmake --install into it first")
        copy_tree(os.path.join(prefix, folder), os.path.join(stage, folder), skipped=SKIPPED)
    for program in ("loginserver.exe", "gameserver.exe", "patchserver.exe", "supervisor.exe", "dbimport.exe", "client.exe"):
        if not os.path.isfile(os.path.join(stage, "bin", program)):
            raise PackageError(f"the install has no bin/{program}")
    shutil.copy2(os.path.join(ROOT, "apps", "installer", "ambrose.ps1"), os.path.join(stage, "ambrose.ps1"))
    text_copy(os.path.join(ROOT, "LICENSE"), os.path.join(stage, "LICENSE.txt"))
    text_copy(os.path.join(ROOT, "THIRD-PARTY-NOTICES.md"), os.path.join(stage, "THIRD-PARTY-NOTICES.txt"))
    stage_mariadb(archive, stage)
    if runtime:
        stage_runtime(runtime, stage)


def check_content(stage):
    problems = []
    for relative in relative_files(stage):
        if matches(relative, CLIENT_NAMES):
            problems.append(f"{relative} is a client file, capture, token or log, which no package carries")
        elif "/" not in relative:
            if relative not in ROOT_FILES:
                problems.append(f"{relative} is not on the package's content list")
        elif not any(fnmatch.fnmatchcase(relative, pattern) for pattern in CONTENT):
            problems.append(f"{relative} is not on the package's content list")
        elif relative.lower().endswith(".json") and not relative.startswith("mariadb/"):
            problems.append(f"{relative} may be a type dump or extracted data, which no package carries")
    if problems:
        raise PackageError("the stage holds what the package may not:\n  " + "\n  ".join(problems))


def write_manifest(stage, version):
    files = []
    for relative in relative_files(stage):
        if relative == MANIFEST:
            continue
        path = os.path.join(stage, *relative.split("/"))
        files.append({"path": relative, "size": os.path.getsize(path), "sha256": sha256_of(path)})
    manifest = {"name": "Project Ambrose", "version": version, "files": files}
    with open(os.path.join(stage, MANIFEST), "w", encoding="utf-8", newline="\n") as handle:
        json.dump(manifest, handle, indent=1)
        handle.write("\n")
    return manifest


def verify_manifest(stage):
    with open(os.path.join(stage, MANIFEST), encoding="utf-8") as handle:
        manifest = json.load(handle)
    listed = {entry["path"]: entry for entry in manifest["files"]}
    problems = []
    for relative in relative_files(stage):
        if relative == MANIFEST:
            continue
        entry = listed.pop(relative, None)
        path = os.path.join(stage, *relative.split("/"))
        if entry is None:
            problems.append(f"{relative} is not in the manifest")
        elif entry["size"] != os.path.getsize(path) or entry["sha256"] != sha256_of(path):
            problems.append(f"{relative} changed after the manifest was written")
    problems += [f"{missing} is in the manifest but not in the stage" for missing in listed]
    return problems


def write_uninstall_list(stage, target):
    files = relative_files(stage)
    folders = set()
    for relative in files:
        parts = relative.split("/")[:-1]
        for depth in range(1, len(parts) + 1):
            folders.add("/".join(parts[:depth]))
    lines = ["; Project Ambrose by Imjustchico", "; Removes exactly the files the setup installed and then each folder they were in, when nothing else is left in it."]
    lines += [f'Delete "$INSTDIR\\{relative.replace("/", chr(92))}"' for relative in files]
    lines += [f'RMDir "$INSTDIR\\{folder.replace("/", chr(92))}"' for folder in sorted(folders, key=lambda name: (-name.count("/"), name))]
    with open(target, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines) + "\n")


def numeric_version(version):
    numbers = [int(part) for part in re.findall(r"\d+", version)[:4]]
    return ".".join(str(number) for number in (numbers + [0, 0, 0, 0])[:4])


def build_setup(stage, version, output, makensis):
    work = os.path.dirname(stage)
    uninstall = os.path.join(work, "uninstall-files.nsh")
    write_uninstall_list(stage, uninstall)
    os.makedirs(os.path.dirname(os.path.abspath(output)), exist_ok=True)
    command = [makensis, "-V2", "-WX", "-INPUTCHARSET", "UTF8", f"-DVERSION={version}", f"-DVERSION_NUMERIC={numeric_version(version)}",
               f"-DSTAGE={os.path.abspath(stage)}", f"-DUNINSTALL_LIST={os.path.abspath(uninstall)}", f"-DOUTFILE={os.path.abspath(output)}",
               os.path.join(HERE, "ambrose-setup.nsi")]
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        raise PackageError(f"makensis failed:\n{result.stdout[-4000:]}{result.stderr[-4000:]}")
    digest = sha256_of(output)
    with open(output + ".sha256", "w", encoding="utf-8", newline="\n") as handle:
        handle.write(f"{digest}  {os.path.basename(output)}\n")
    return digest


def main(argv=None):
    parser = argparse.ArgumentParser(description="Build the Project Ambrose setup program for Windows from a release install.")
    parser.add_argument("--prefix", required=True, help="the folder cmake --install wrote, holding bin, etc and share")
    parser.add_argument("--version", required=True, help="the version the setup reports, such as 0.1.0 or a tag")
    parser.add_argument("--output", required=True, help="the setup program to write, such as dist/ProjectAmbrose-Setup-0.1.0.exe")
    parser.add_argument("--runtime", help="the folder holding the Visual C++ runtime DLLs to place beside each program")
    parser.add_argument("--mariadb-zip", help="a MariaDB winx64 zip already downloaded, checked against mariadb.json's pin instead of downloading it")
    parser.add_argument("--require-pin", action="store_true", help="refuse a MariaDB zip mariadb.json pins no SHA-256 for, as a release does")
    parser.add_argument("--stage-only", action="store_true", help="stage, check and write the manifest without running makensis")
    parser.add_argument("--makensis", default=shutil.which("makensis") or "makensis")
    args = parser.parse_args(argv)
    with open(os.path.join(HERE, "mariadb.json"), encoding="utf-8") as handle:
        spec = json.load(handle)
    work = tempfile.mkdtemp(prefix="ambrose-setup-")
    stage = os.path.join(work, "stage")
    try:
        archive = mariadb_zip(spec, args.mariadb_zip, work, args.require_pin)
        stage_package(args.prefix, stage, archive, args.runtime)
        check_content(stage)
        manifest = write_manifest(stage, args.version)
        print(f"staged: {len(manifest['files'])} files, {sum(entry['size'] for entry in manifest['files']) // (1024 * 1024)} MB")
        if args.stage_only:
            return 0
        digest = build_setup(stage, args.version, args.output, args.makensis)
        print(f"built: {args.output} {os.path.getsize(args.output) // (1024 * 1024)} MB, SHA-256 {digest}")
        return 0
    except PackageError as error:
        print(f"package: {error}", file=sys.stderr)
        return 1
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
