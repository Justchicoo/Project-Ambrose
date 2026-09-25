# Project Ambrose by Imjustchico
# Builds a run's report from the two logs and what the run measured: every message the server did not handle, every warning on either side, and the checks that decide whether the run is clean, each of which fails when what it judges was never measured, then renders it as Markdown beside its JSON.
import json
import os
import re

SERVER_NOT_HANDLED = re.compile(r"does not handle yet|never accepts|only the server sends|has no handler")
SERVER_DROPPED = re.compile(r"which needs |truncated body|Dropped ")
SERVER_LEVEL = re.compile(r"^\S+\s+(WARN|ERROR|FATAL)\b|^(WARN|ERROR|FATAL)\s*:")
MESSAGE_NAME = re.compile(r"\b([A-Z][A-Z0-9]*) (MSG_[A-Za-z0-9_]+) \((\d+):(\d+)\)")
MESSAGE_ONLY = re.compile(r"\b(MSG_[A-Za-z0-9_]+)\b")
CLIENT_LEVEL = re.compile(r"\[(ERRO|WARN)\]")
CLIENT_MESSAGE_BOX = re.compile(r"DisplayMessageBox\(")
CLIENT_UNKNOWN = re.compile(r"unknown message|invalid message service|Unknown message type", re.IGNORECASE)
CLIENT_LEFT_THE_MACHINE = re.compile(r"external browser|steam page|steam overlay")
CLIENT_MILESTONE = re.compile(
    r"Connect to login server|Send MSG_|LOGIN RESPONSE|admitted the user|CHARACTER LIST|scene has been loaded|"
    r"CREATE CHARACTER|DELETE CHARACTER|MSG_CharacterSelected|Mainloop exited|Metric Url|AppCloseConnection|"
    r"Failed to connect|Connection Timer ended")
SERVER_SESSION = re.compile(r"Session \d+ (offered|accepted|closed)|authenticated as|failed to authenticate")


def message_of(line):
    named = MESSAGE_NAME.search(line)
    if named:
        return named.group(2), f"{named.group(1)} {named.group(2)} ({named.group(3)}:{named.group(4)})"
    only = MESSAGE_ONLY.search(line)
    return (only.group(1), only.group(1)) if only else (None, line.strip())


def gather(server_lines, client_lines):
    not_handled = [line for line in server_lines if SERVER_NOT_HANDLED.search(line)]
    dropped = [line for line in server_lines if SERVER_DROPPED.search(line)]
    counted = {}
    for line in not_handled + dropped:
        _name, key = message_of(line)
        counted[key] = counted.get(key, 0) + 1
    return {
        "unhandled_messages": counted,
        "server_not_handled_lines": not_handled,
        "server_dropped_lines": dropped,
        "server_warn_error": [line for line in server_lines if SERVER_LEVEL.search(line)],
        "server_sessions": [line for line in server_lines if SERVER_SESSION.search(line)],
        "client_errors_warnings": [line for line in client_lines if CLIENT_LEVEL.search(line)],
        "client_message_boxes": [line for line in client_lines if CLIENT_MESSAGE_BOX.search(line)],
        "client_unknown_messages": [line for line in client_lines if CLIENT_UNKNOWN.search(line)],
        "client_left_the_machine": [line for line in client_lines if CLIENT_LEFT_THE_MACHINE.search(line)],
        "client_milestones": [line for line in client_lines if CLIENT_MILESTONE.search(line)],
    }


def allowed_by(patterns, line):
    return any(re.search(pattern, line) for pattern in patterns)


def named_by(patterns, line):
    return any(re.search(rf"\b{re.escape(pattern)}\b", line) for pattern in patterns)


def unexpected_messages(lines, allowed):
    unexpected = []
    seen = set()
    for line in lines:
        name, _key = message_of(line)
        if name:
            seen.add(name)
        excused = name in allowed if name else named_by(allowed, line)
        if not excused:
            unexpected.append(line.strip())
    return unexpected, sorted(name for name in allowed if name not in seen)


def screen_checks(steps):
    missing = []
    for step in steps:
        screen = step.get("screen") or {}
        if screen.get("changed") and not screen.get("shot"):
            missing.append(step.get("step"))
    return missing


def checks(facts, gathered):
    found = []

    def add(what, ok, detail):
        found.append({"check": what, "ok": bool(ok), "detail": detail})

    with_client = bool(facts.get("needs_client", True))
    changes = facts.get("install_changes") or {}
    changed = sum(len(names) for names in changes.values())
    read = facts.get("install_files")
    if changed:
        add("the install was only read", False, json.dumps(changes))
    elif with_client and not read:
        add("the install was only read", False,
            f"no file under {facts.get('install') or 'the install'} was read, so nothing was compared")
    else:
        own = facts.get("client_writes") or {}
        written = sorted(name for names in own.values() for name in names)
        add("the install was only read", True, f"no file of the {read} read under the install was added, removed or changed"
            + (f", apart from the client's own {', '.join(written)}" if written else ""))
    if with_client:
        guard = facts.get("netguard")
        violations = (guard or {}).get("violations") or []
        remotes = (guard or {}).get("remotes") or []
        blind = (guard or {}).get("failed") if guard else "nothing watched the client's connections during this run"
        add("the client contacted only this machine", bool(guard) and not violations and not blind,
            f"{len(remotes)} address(es) contacted, all of them local" if guard and not violations and not blind
            else json.dumps(violations) if violations else str(blind))
    pending = facts.get("pending_allowed") or []
    unexpected, unused = unexpected_messages(gathered["server_not_handled_lines"], pending)
    add("every message the server did not handle is one the scenario expects", not unexpected,
        ("the scenario allows " + ", ".join(pending) if pending else "the server handled every message the client sent")
        + (f"; it never saw {', '.join(unused)}" if unused else "")
        if not unexpected else "; ".join(unexpected))
    dropped, _unused = unexpected_messages(gathered["server_dropped_lines"], facts.get("dropped_allowed") or [])
    add("the server read every message the client sent", not dropped,
        "no message was dropped or refused" if not dropped else "; ".join(dropped))
    allowed_warnings = facts.get("server_log_allowed") or []
    loud = [line.strip() for line in gathered["server_warn_error"] if not allowed_by(allowed_warnings, line)]
    add("no server WARN, ERROR or FATAL outside the allow-list", not loud,
        f"{len(gathered['server_warn_error'])} allowed line(s)" if not loud else "; ".join(loud))
    confused = [line.strip() for line in gathered["client_unknown_messages"]
                if not allowed_by(facts.get("client_log_allowed") or [], line)]
    add("the client understood every message the server sent", not confused,
        "no line says the client read a message it does not know" if not confused else "; ".join(confused))
    add("the client opened nothing outside itself", not gathered["client_left_the_machine"],
        "no line opens an external browser" if not gathered["client_left_the_machine"] else "; ".join(line.strip() for line in gathered["client_left_the_machine"]))
    leftover = facts.get("leftover_processes") or []
    add("no process was left running", not leftover,
        "the client, its helpers, the server and the capture all stopped" if not leftover else ", ".join(leftover))
    after = facts.get("databases_after")
    add("no database was left behind", not after,
        "the driver's own databases are gone" if not after else str(after))
    steps = facts.get("steps") or []
    without = screen_checks(steps)
    add("every step that changed the screen has a screenshot", not without,
        f"{len(facts.get('screenshots') or [])} screenshot(s)" if not without else ", ".join(str(name) for name in without))
    around = [step.get("step") for step in steps if step.get("stage") == "driver" and not step.get("ok")]
    add("every step the driver took around the scenario passed", not around,
        "everything the run started was started and stopped as it should be" if not around
        else ", ".join(str(name) for name in around))
    failed = [step.get("step") for step in steps if step.get("stage") != "driver" and not step.get("ok")]
    if facts.get("expect_failure"):
        add("the step the scenario expects to fail did fail", bool(failed),
            ", ".join(str(name) for name in failed) if failed else "every step passed, which this scenario does not expect")
    else:
        stopped = facts.get("failed")
        add("every step passed", not failed and not stopped,
            "every step was satisfied within its timeout" if not failed and not stopped
            else ", ".join(str(name) for name in failed) or str(stopped))
    return found


def build(facts, server_lines, client_lines):
    gathered = gather(server_lines, client_lines)
    report = dict(facts)
    report.update(gathered)
    report["checks"] = checks(facts, gathered)
    report["clean"] = all(check["ok"] for check in report["checks"])
    return report


ORDER = ("run_id", "scenario", "title", "result", "clean", "failed", "seconds", "checks", "launcher", "references",
         "steps", "screenshots", "unhandled_messages")


def render(report):
    lines = [f"# Client drive report {report.get('run_id', '')}", ""]
    for key in list(ORDER) + [key for key in report if key not in ORDER]:
        if key not in report:
            continue
        value = report[key]
        lines.append(f"## {key}")
        if key == "checks":
            lines.append("| check | result | detail |")
            lines.append("|---|---|---|")
            for check in value:
                detail = str(check["detail"]).replace("|", "\\|")
                lines.append(f"| {check['check']} | {'pass' if check['ok'] else 'FAIL'} | {detail} |")
        elif isinstance(value, list):
            lines.extend([f"- `{json.dumps(item)}`" if isinstance(item, (dict, list)) else f"- `{item}`" for item in value] or ["- none"])
        elif isinstance(value, dict):
            lines.extend([f"- {name}: {json.dumps(item)}" for name, item in value.items()] or ["- none"])
        else:
            lines.append(str(value))
        lines.append("")
    return "\n".join(lines)


def write(folder, report):
    os.makedirs(folder, exist_ok=True)
    json_path = os.path.join(folder, "report.json")
    with open(json_path, "w", encoding="utf-8") as handle:
        json.dump(report, handle, indent=1)
        handle.write("\n")
    markdown_path = os.path.join(folder, "report.md")
    with open(markdown_path, "w", encoding="utf-8") as handle:
        handle.write(render(report))
    return markdown_path
