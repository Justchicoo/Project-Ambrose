# Project Ambrose by Imjustchico
# Watches the client tree the driver started, and nothing else on the machine, and kills that tree the moment one of its processes connects to an address that is not on this machine, recording every address any of them reached and every helper that appeared after the client died.
import json
import threading
import time

WATCHED = ("wizardgraphicalclient.exe", "kiwebhelper.exe", "bugreporter.exe", "wizard101.exe", "wizardlauncher.exe",
           "kingsisle patcher.exe")
STARTED_HERE = ("loginserver.exe", "loginserver", "launcher.exe", "launcher", "tshark.exe", "tshark")
INTERVAL = 0.1


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
    def __init__(self, pids, record_path, started=None, interval=INTERVAL):
        super().__init__(daemon=True)
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

    def record(self):
        return {"remotes": [{"remote": key, "first_seen": when} for key, when in sorted(self.seen.items())],
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
