# Project Ambrose by Imjustchico
# C-27 standalone TCP session quality prober with explicit timeout reporting.
import argparse
import ipaddress
import json
import socket
import statistics
import sys
import threading
import time
from dataclasses import asdict, dataclass


DEFAULT_PAYLOAD = b"AMBROSE_PROBE_C27\n"


@dataclass(frozen=True)
class Sample:
    sequence: int
    rtt_ms: float


@dataclass(frozen=True)
class Report:
    host: str
    port: int
    requested: int
    completed: int
    timeouts: int
    failures: int
    samples: list[Sample]
    mean_rtt_ms: float | None
    min_rtt_ms: float | None
    max_rtt_ms: float | None
    jitter_ms: float | None
    status: str
    reason: str | None


class ProbeError(ValueError):
    pass


def parse_payload(value: str) -> bytes:
    try:
        payload = value.encode("ascii")
    except UnicodeEncodeError as exc:
        raise ProbeError("--payload must contain ASCII only") from exc
    if not payload:
        raise ProbeError("--payload must not be empty")
    if len(payload) > 4096:
        raise ProbeError("--payload must be at most 4096 bytes")
    return payload


def stats(samples: list[Sample]) -> tuple[float | None, float | None, float | None, float | None]:
    values = [sample.rtt_ms for sample in samples]
    if not values:
        return None, None, None, None
    differences = [abs(right - left) for left, right in zip(values, values[1:])]
    return (
        round(statistics.mean(values), 3),
        round(min(values), 3),
        round(max(values), 3),
        round(statistics.mean(differences), 3) if differences else 0.0,
    )


def validate_target(host: str, allow_private_network: bool) -> None:
    if host.lower() == "localhost":
        return
    try:
        address = ipaddress.ip_address(host)
    except ValueError as exc:
        raise ProbeError("host must be localhost or a literal IP address") from exc
    if address.is_loopback:
        return
    if not allow_private_network or not address.is_private:
        raise ProbeError("non-loopback targets require --allow-private-network and a private IP")


def probe(args: argparse.Namespace, payload: bytes) -> Report:
    samples: list[Sample] = []
    timeouts = 0
    failures = 0
    reason: str | None = None
    for sequence in range(1, args.count + 1):
        try:
            started = time.perf_counter()
            with socket.create_connection((args.host, args.port), timeout=args.connect_timeout) as connection:
                connection.settimeout(args.response_timeout)
                connection.sendall(payload)
                received = bytearray()
                while len(received) < len(payload):
                    chunk = connection.recv(len(payload) - len(received))
                    if not chunk:
                        raise ProbeError("endpoint closed before echo completed")
                    received.extend(chunk)
                if bytes(received) != payload:
                    raise ProbeError("endpoint returned a response different from the probe payload")
            samples.append(Sample(sequence, round((time.perf_counter() - started) * 1000, 3)))
        except socket.timeout:
            timeouts += 1
            reason = "connect or response timeout"
        except (ConnectionError, OSError, ProbeError) as exc:
            failures += 1
            reason = str(exc)
        if sequence != args.count and args.interval > 0:
            time.sleep(args.interval)

    mean_rtt, min_rtt, max_rtt, jitter = stats(samples)
    if not samples:
        status = "unable"
    elif failures or timeouts:
        status = "fail"
    elif args.max_rtt_ms is not None and (mean_rtt is None or mean_rtt > args.max_rtt_ms):
        status = "fail"
        reason = f"mean RTT exceeds {args.max_rtt_ms:g} ms"
    elif args.max_jitter_ms is not None and (jitter is None or jitter > args.max_jitter_ms):
        status = "fail"
        reason = f"jitter exceeds {args.max_jitter_ms:g} ms"
    else:
        status = "pass"
    return Report(
        args.host,
        args.port,
        args.count,
        len(samples),
        timeouts,
        failures,
        samples,
        mean_rtt,
        min_rtt,
        max_rtt,
        jitter,
        status,
        reason,
    )


def echo_server(ready: threading.Event, port: list[int], payload: bytes) -> None:
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        listener.listen()
        port.append(listener.getsockname()[1])
        ready.set()
        connection, _ = listener.accept()
        with connection:
            received = connection.recv(len(payload))
            if received == payload:
                connection.sendall(received)


def run_self_test() -> int:
    ready = threading.Event()
    port: list[int] = []
    thread = threading.Thread(target=echo_server, args=(ready, port, DEFAULT_PAYLOAD), daemon=True)
    thread.start()
    if not ready.wait(2):
        print("C-27 self-test: echo fixture did not start", file=sys.stderr)
        return 2
    args = argparse.Namespace(
        host="127.0.0.1",
        port=port[0],
        count=1,
        connect_timeout=1.0,
        response_timeout=1.0,
        interval=0.0,
        max_rtt_ms=None,
        max_jitter_ms=None,
    )
    report = probe(args, DEFAULT_PAYLOAD)
    thread.join(2)
    print(json.dumps(asdict(report), indent=2))
    return 0 if report.status == "pass" else 2


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Measure TCP session quality against an owned endpoint.")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int)
    parser.add_argument("--allow-private-network", action="store_true")
    parser.add_argument("--count", type=int, default=5)
    parser.add_argument("--connect-timeout", type=float, default=1.0)
    parser.add_argument("--response-timeout", type=float, default=1.0)
    parser.add_argument("--interval", type=float, default=0.1)
    parser.add_argument("--payload", default=DEFAULT_PAYLOAD.decode("ascii"))
    parser.add_argument("--max-rtt-ms", type=float)
    parser.add_argument("--max-jitter-ms", type=float)
    parser.add_argument("--pretty", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)
    try:
        if args.self_test:
            return run_self_test()
        validate_target(args.host, args.allow_private_network)
        if args.port is None or not 1 <= args.port <= 65535:
            raise ProbeError("--port must be between 1 and 65535")
        if args.count < 1 or args.count > 1000:
            raise ProbeError("--count must be between 1 and 1000")
        if args.connect_timeout <= 0 or args.response_timeout <= 0 or args.interval < 0:
            raise ProbeError("timeouts must be positive and interval must not be negative")
        if args.max_rtt_ms is not None and args.max_rtt_ms < 0:
            raise ProbeError("--max-rtt-ms must not be negative")
        if args.max_jitter_ms is not None and args.max_jitter_ms < 0:
            raise ProbeError("--max-jitter-ms must not be negative")
        report = probe(args, parse_payload(args.payload))
    except ProbeError as exc:
        print(f"C-27 prober: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(asdict(report), indent=2 if args.pretty else None))
    return {"pass": 0, "fail": 1, "unable": 77}[report.status]


if __name__ == "__main__":
    raise SystemExit(main())
