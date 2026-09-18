# Project Ambrose by Imjustchico
# Watches the client and every helper it starts and kills the whole tree the moment one of them connects to an address that is not on this machine, recording every address any of them reached.
import json
import threading
import time

WATCHED = ("wizardgraphicalclient.exe", "kiwebhelper.exe", "bugreporter.exe", "wizard101.exe", "wizardlauncher.exe",
           "kingsisle patcher.exe")
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


def leftover_processes(started):
    import psutil

    found = []
    for process in psutil.process_iter(["name", "create_time", "pid"]):
        name = (process.info["name"] or "").lower()
        if name in WATCHED and (process.info["create_time"] or 0) >= started - 5:
            found.append(f"{process.info['name']} pid {process.info['pid']}")
    return found


def kill_leftovers(started):
    import psutil

    killed = leftover_processes(started)
    for description in killed:
        try:
            kill_tree(int(description.rsplit(" ", 1)[1]))
        except (ValueError, psutil.Error):
            pass
    return killed


class NetGuard(threading.Thread):
    def __init__(self, pid, record_path, started=None, interval=INTERVAL):
        super().__init__(daemon=True)
        self.pid = pid
        self.record_path = record_path
        self.started = started if started is not None else time.time() - 5
        self.interval = interval
        self.local = local_addresses()
        self.seen = {}
        self.violations = []
        self._halt = threading.Event()

    def processes(self):
        import psutil

        found = {}
        try:
            root = psutil.Process(self.pid)
            for process in [root] + root.children(recursive=True):
                found[process.pid] = process
        except psutil.Error:
            pass
        for process in psutil.process_iter(["name", "create_time"]):
            if (process.info["name"] or "").lower() in WATCHED and (process.info["create_time"] or 0) >= self.started:
                found[process.pid] = process
        return found

    def run(self):
        import psutil

        while not self._halt.is_set():
            processes = self.processes()
            if not processes:
                break
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
        self.flush()

    def record(self):
        return {"remotes": [{"remote": key, "first_seen": when} for key, when in sorted(self.seen.items())],
                "violations": self.violations}

    def flush(self):
        with open(self.record_path, "w", encoding="utf-8") as handle:
            json.dump(self.record(), handle, indent=1)
            handle.write("\n")

    def stop(self):
        self._halt.set()
        self.join(5)
        return f"{len(self.seen)} address(es) reached, {len(self.violations)} of them off this machine"
