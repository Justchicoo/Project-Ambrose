# Project Ambrose by Imjustchico
# C-23 safely replays private request/response vectors without exposing bytes.
import argparse
import hashlib
import ipaddress
import json
import socket
import sys
import tempfile
import threading
import time
from pathlib import Path
from typing import Any


MAX_EXCHANGES = 100
MAX_VECTOR_BYTES = 1_048_576
DEFAULT_TIMEOUT = 5.0


def digest(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def validate_target(host: str, allow_private_network: bool) -> None:
    if host.lower() == "localhost":
        return
    try:
        address = ipaddress.ip_address(host)
    except ValueError as exc:
        raise ValueError("host must be localhost or a literal IP address") from exc
    if address.is_loopback:
        return
    if not allow_private_network or not address.is_private:
        raise ValueError("non-loopback targets require --allow-private-network and a private IP")


def decode_hex(value: Any, field: str) -> bytes:
    if not isinstance(value, str) or len(value) % 2:
        raise ValueError(f"{field} must be an even-length hexadecimal string")
    try:
        result = bytes.fromhex(value)
    except ValueError as exc:
        raise ValueError(f"{field} is not valid hexadecimal") from exc
    if len(result) > MAX_VECTOR_BYTES:
        raise ValueError(f"{field} exceeds {MAX_VECTOR_BYTES} bytes")
    return result


def load_manifest(path: Path) -> list[dict[str, Any]]:
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ValueError(f"cannot read manifest: {exc}") from exc
    if not isinstance(document, dict) or document.get("version") != 1:
        raise ValueError("manifest version must be 1")
    exchanges = document.get("exchanges")
    if not isinstance(exchanges, list) or not exchanges or len(exchanges) > MAX_EXCHANGES:
        raise ValueError(f"exchanges must contain 1 to {MAX_EXCHANGES} entries")
    parsed = []
    for index, exchange in enumerate(exchanges, 1):
        if not isinstance(exchange, dict):
            raise ValueError(f"exchange {index} must be an object")
        request = decode_hex(exchange.get("request_hex"), f"exchange {index} request_hex")
        expected = decode_hex(exchange.get("expected_response_hex"), f"exchange {index} expected_response_hex")
        response_bytes = exchange.get("response_bytes")
        if not isinstance(response_bytes, int) or response_bytes < 0 or response_bytes > MAX_VECTOR_BYTES:
            raise ValueError(f"exchange {index} response_bytes is invalid")
        if response_bytes != len(expected):
            raise ValueError(f"exchange {index} response_bytes does not match expected_response_hex")
        delay_ms = exchange.get("delay_ms", 0)
        if not isinstance(delay_ms, int) or delay_ms < 0 or delay_ms > 60_000:
            raise ValueError(f"exchange {index} delay_ms is invalid")
        parsed.append({"name": str(exchange.get("name", f"exchange-{index}")), "request": request, "expected": expected, "delay_ms": delay_ms})
    return parsed


def receive_exact(connection: socket.socket, count: int) -> bytes:
    received = bytearray()
    while len(received) < count:
        chunk = connection.recv(count - len(received))
        if not chunk:
            raise ConnectionError("endpoint closed before the expected response length")
        received.extend(chunk)
    return bytes(received)


def replay(host: str, port: int, exchanges: list[dict[str, Any]], timeout: float) -> dict[str, Any]:
    reports = []
    try:
        with socket.create_connection((host, port), timeout=timeout) as connection:
            connection.settimeout(timeout)
            for exchange in exchanges:
                if exchange["delay_ms"]:
                    time.sleep(exchange["delay_ms"] / 1000)
                connection.sendall(exchange["request"])
                actual = receive_exact(connection, len(exchange["expected"]))
                reports.append({
                    "name": exchange["name"],
                    "status": "pass" if actual == exchange["expected"] else "fail",
                    "expected_bytes": len(exchange["expected"]),
                    "actual_bytes": len(actual),
                    "expected_sha256": digest(exchange["expected"]),
                    "actual_sha256": digest(actual),
                })
    except (ConnectionError, OSError, socket.timeout) as exc:
        return {"version": 1, "status": "unable", "completed": len(reports), "total": len(exchanges), "reason": type(exc).__name__, "exchanges": reports}
    status = "fail" if any(item["status"] == "fail" for item in reports) else "pass"
    return {"version": 1, "status": status, "completed": len(reports), "total": len(exchanges), "exchanges": reports}


def echo_server(ready: threading.Event, port: list[int]) -> None:
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        listener.listen()
        port.append(listener.getsockname()[1])
        ready.set()
        connection, _ = listener.accept()
        with connection:
            while True:
                data = connection.recv(4096)
                if not data:
                    return
                connection.sendall(data)


def self_test() -> int:
    ready = threading.Event()
    port: list[int] = []
    thread = threading.Thread(target=echo_server, args=(ready, port), daemon=True)
    thread.start()
    if not ready.wait(2):
        print("C-23 self-test: fixture did not start", file=sys.stderr)
        return 2
    with tempfile.TemporaryDirectory(prefix="ambrose-c23-") as directory:
        manifest = Path(directory) / "manifest.json"
        manifest.write_text(json.dumps({"version": 1, "exchanges": [
            {"name": "synthetic", "request_hex": "010203", "expected_response_hex": "010203", "response_bytes": 3}
        ]}), encoding="utf-8")
        report = replay("127.0.0.1", port[0], load_manifest(manifest), DEFAULT_TIMEOUT)
    thread.join(2)
    print(json.dumps({"status": report["status"], "completed": report["completed"], "total": report["total"]}))
    return 0 if report["status"] == "pass" else 2


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Replay private TCP request/response vectors safely.")
    parser.add_argument("--manifest")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int)
    parser.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT)
    parser.add_argument("--allow-private-network", action="store_true")
    parser.add_argument("--pretty", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)
    if args.self_test:
        return self_test()
    try:
        validate_target(args.host, args.allow_private_network)
        if not args.manifest or args.port is None or not 1 <= args.port <= 65535:
            raise ValueError("--manifest and a port from 1 to 65535 are required")
        if args.timeout <= 0 or args.timeout > 300:
            raise ValueError("--timeout must be greater than 0 and at most 300 seconds")
        exchanges = load_manifest(Path(args.manifest))
        report = replay(args.host, args.port, exchanges, args.timeout)
    except ValueError as exc:
        print(f"C-23 replayer: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(report, indent=2 if args.pretty else None))
    return {"pass": 0, "fail": 1, "unable": 77}[report["status"]]


if __name__ == "__main__":
    raise SystemExit(main())
