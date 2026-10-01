# Project Ambrose by Imjustchico
# Watches the client tree the driver started, and nothing else on the machine, and kills that tree the moment one of its processes connects to an address that is not on this machine, recording every address any of them reached and every helper that appeared after the client died. The only exceptions are the ones netguard-allow.json declares, each with its evidence and the day it was settled: a connection passes only when its process name, its port and the processes above it all match and its address lies in the same /48, or /24 for IPv4, as an address the declared host resolves to now, and when the host cannot be resolved nothing passes. Each connection let through is still recorded, with the allowance that let it.
import ipaddress
import json
import os
import socket
import threading
import time

WATCHED = ("wizardgraphicalclient.exe", "kiwebhelper.exe", "bugreporter.exe", "wizard101.exe", "wizardlauncher.exe",
           "kingsisle patcher.exe")
STARTED_HERE = ("loginserver.exe", "loginserver", "launcher.exe", "launcher", "msedgewebview2.exe", "tshark.exe", "tshark")
INTERVAL = 0.1
ALLOW_FILE = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "netguard-allow.json")
ALLOW_KEYS = ("host", "port", "process", "browser_process_only", "under", "since", "reason", "evidence")
RESOLVE_EVERY = 60


def load_allowances(path=ALLOW_FILE):
    if not os.path.exists(path):
        return []
    with open(path, "r", encoding="utf-8") as handle:
        entries = json.load(handle)
    if not isinstance(entries, list):
        raise ValueError(f"{path} must hold a list of allowances")
    for entry in entries:
        if not isinstance(entry, dict) or set(entry) != set(ALLOW_KEYS):
            raise ValueError(f"{path}: an allowance must have exactly {', '.join(ALLOW_KEYS)}")
        if not isinstance(entry["port"], int) or not all(isinstance(entry[key], str) and entry[key] for key in ALLOW_KEYS if key not in ("port", "browser_process_only")):
            raise ValueError(f"{path}: an allowance for {entry.get('host')!r} has an empty or mistyped field")
    return entries


def networks_of(host, resolve=socket.getaddrinfo):
    try:
        found = resolve(host, 443, proto=socket.IPPROTO_TCP)
    except OSError:
        return set()
    networks = set()
    for row in found:
        address = ipaddress.ip_address(row[4][0].split("%")[0])
        networks.add(ipaddress.ip_network(f"{address}/{48 if address.version == 6 else 24}", strict=False))
    return networks


def allowance_for(allowances, name, cmdline, ancestors, address, port, networks):
    try:
        plain = ipaddress.ip_address(str(address).split("%")[0])
    except ValueError:
        return None
    for entry in allowances:
        if name.lower() != entry["process"].lower() or port != entry["port"]:
            continue
        if entry["browser_process_only"] and any(part.startswith("--type=") for part in cmdline):
            continue
        if entry["under"].lower() not in (ancestor.lower() for ancestor in ancestors):
            continue
        if any(plain in network for network in networks.get(entry["host"], ())):
            return entry
    return None


def local_addresses():
    import psutil

    found = {"127.0.0.1", "::1", "0.0.0.0", "::"}
    for addresses in psutil.net_if_addrs().values():
        for address in addresses:
            found.add(str(address.address).split("%")[0])
    return found


def is_local(address, known):
    plain = str(address).split("%")[0]
    if plain.startswith("::ffff:"):
        plain = plain[7:]
    return plain.startswith("127.") or plain == "::1" or plain in known


def kill_tree(pid):
    import psutil

    try:
        root = psutil.Process(pid)
    except psutil.Error:
        return
    for process in root.children(recursive=True) + [root]:
        try:
            process.kill()
        except psutil.Error:
            pass


def adopted(rows, started, known):
    found = []
    for row in rows:
        if (row.get("name") or "").lower() not in WATCHED:
            continue
        if (row.get("create_time") or 0) < started:
            continue
        if row.get("ppid") in known:
            found.append(row)
    return found


def leftovers_among(rows, started, known):
    found = []
    for row in rows:
        name = (row.get("name") or "").lower()
        if name not in WATCHED and name not in STARTED_HERE:
            continue
        if (row.get("create_time") or 0) < started:
            continue
        if row.get("pid") not in known and row.get("ppid") not in known:
            continue
        found.append(f"{row.get('name')} pid {row.get('pid')}")
    return sorted(found)


def rows_of(processes):
    return [dict(process.info, pid=process.pid, process=process) for process in processes]


def listed():
    import psutil

    return rows_of(psutil.process_iter(["name", "create_time", "ppid"]))


def own_pids(known=()):
    import psutil

    found = set(known)
    try:
        for child in psutil.Process().children(recursive=True):
            found.add(child.pid)
    except psutil.Error:
        pass
    return found


def leftover_processes(started, known=()):
    return leftovers_among(listed(), started, own_pids(known))


def kill_leftovers(started, known=()):
    import psutil

    killed = leftover_processes(started, known)
    for description in killed:
        try:
            kill_tree(int(description.rsplit(" ", 1)[1]))
        except (ValueError, psutil.Error):
            pass
    return killed


class NetGuard(threading.Thread):
    def __init__(self, pids, record_path, started=None, interval=INTERVAL, allowances=(), resolve=socket.getaddrinfo):
        super().__init__(daemon=True)
        self.allowances = list(allowances)
        self.resolve = resolve
        self.networks = {}
        self.resolved_at = None
        self.allowed = {}
        self.record_path = record_path
        self.started = started if started is not None else time.time() - 5
        self.interval = interval
        self.local = local_addresses()
        self.known = {}
        self.seen = {}
        self.violations = []
        self.failed = None
        self._halt = threading.Event()
        self._written = None
        self.remember(pids)

    def remember(self, pids):
        import psutil

        for pid in pids:
            try:
                self.known[pid] = psutil.Process(pid).create_time()
            except psutil.Error:
                continue

    def processes(self):
        import psutil

        found = {}
        for pid, when in list(self.known.items()):
            try:
                root = psutil.Process(pid)
                if root.create_time() != when:
                    del self.known[pid]
                    continue
                for process in [root] + root.children(recursive=True):
                    found[process.pid] = process
            except psutil.Error:
                continue
        rows = [row for row in listed() if row["pid"] not in found]
        for row in adopted(rows, self.started, self.known):
            found[row["pid"]] = row["process"]
        for pid, process in found.items():
            if pid not in self.known:
                try:
                    self.known[pid] = process.create_time()
                except psutil.Error:
                    continue
        return found

    def run(self):
        try:
            self.watch()
        except Exception as error:
            self.failed = f"the guard stopped early: {error}"
        self.flush()

    def watch(self):
        import psutil

        while not self._halt.is_set():
            processes = self.processes()
            for process in list(processes.values()):
                try:
                    connections = process.net_connections(kind="inet")
                    name = process.name()
                except psutil.Error:
                    continue
                for connection in connections:
                    if not connection.raddr:
                        continue
                    key = f"{name} {connection.raddr.ip}:{connection.raddr.port}"
                    self.seen.setdefault(key, time.strftime("%H:%M:%S"))
                    if not is_local(connection.raddr.ip, self.local):
                        entry = self.allowance(process, name, connection.raddr.ip, connection.raddr.port)
                        if entry is not None:
                            self.allowed.setdefault(key, {"host": entry["host"], "reason": entry["reason"], "since": entry["since"],
                                                          "first_seen": time.strftime("%H:%M:%S")})
                            continue
                        self.violations.append({"time": time.strftime("%H:%M:%S"), "process": name, "pid": process.pid,
                                                "remote": f"{connection.raddr.ip}:{connection.raddr.port}",
                                                "status": connection.status})
                        for other in processes.values():
                            try:
                                other.kill()
                            except psutil.Error:
                                pass
            self.flush()
            self._halt.wait(self.interval)

    def allowance(self, process, name, address, port):
        import psutil

        if not self.allowances:
            return None
        if self.resolved_at is None or time.monotonic() - self.resolved_at > RESOLVE_EVERY:
            self.networks = {entry["host"]: networks_of(entry["host"], self.resolve) for entry in self.allowances}
            self.resolved_at = time.monotonic()
        try:
            cmdline = process.cmdline()
            ancestors = [parent.name() for parent in process.parents()]
        except psutil.Error:
            return None
        return allowance_for(self.allowances, name, cmdline, ancestors, address, port, self.networks)

    def record(self):
        return {"remotes": [{"remote": key, "first_seen": when} for key, when in sorted(self.seen.items())],
                "allowed": [dict(remote=key, **detail) for key, detail in sorted(self.allowed.items())],
                "violations": self.violations, "failed": self.failed,
                "watched": sorted(self.known)}

    def flush(self):
        record = self.record()
        if record == self._written:
            return
        with open(self.record_path, "w", encoding="utf-8") as handle:
            json.dump(record, handle, indent=1)
            handle.write("\n")
        self._written = record

    def stop(self):
        self._halt.set()
        self.join(5)
        if self.is_alive():
            self.failed = self.failed or "the guard was still watching after it was asked to stop"
            self.flush()
        said = f"{len(self.seen)} address(es) reached, {len(self.violations)} of them off this machine"
        return said if not self.failed else f"{said}; {self.failed}"
