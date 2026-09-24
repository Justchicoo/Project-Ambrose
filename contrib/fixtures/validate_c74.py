# Project Ambrose by Imjustchico
# Validates capture-free C-74 replay manifests against the C-23 replayer shape.

import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent
REQUIRED = {
    "c74-login-replay.json": ("login-success", "MSG_USER_AUTHEN_V3", "MSG_USER_AUTHEN_RSP"),
    "c74-wrong-password-replay.json": ("login-wrong-password", "MSG_USER_AUTHEN_V3", "MSG_USER_AUTHEN_RSP"),
    "c74-keepalive-replay.json": ("keepalive-exchange", "CLIENT_KEEPALIVE", "KEEPALIVE_RESPONSE"),
}

AUTH_SUCCESS_BODY = bytes(30)
AUTH_FAILURE_BODY = bytes.fromhex("8091683b" + "00" * 26)
ADMIT_BODY = bytes.fromhex("01000000" + "00000000")
CHARACTER_LIST_BODY = bytes.fromhex("00000000")


def frame_header(vector, label):
    data = bytes.fromhex(vector)
    if len(data) < 9 or data[:2] != b"\x0d\xf0":
        raise ValueError(f"{label}: not an Ambrose frame")
    declared = int.from_bytes(data[2:4], "little")
    if declared != len(data) - 4:
        raise ValueError(f"{label}: frame length is {declared}, actual {len(data) - 4}")
    return data


def dml_body(frame, label):
    data = frame
    if data[4] != 0 or data[5] != 0:
        raise ValueError(f"{label}: expected a DML frame")
    dml_length = int.from_bytes(data[10:12], "little")
    if dml_length != len(data) - 9:
        raise ValueError(f"{label}: DML length does not match frame")
    return data[12:-1]


def check(path):
    document = json.loads(path.read_text(encoding="utf-8"))
    expected_name, first_message, second_message = REQUIRED[path.name]
    if document.get("version") != 1 or document.get("name") != expected_name:
        raise ValueError(f"{path.name}: invalid version or corpus name")
    if not document.get("source", "").startswith("Ambrose-authored"):
        raise ValueError(f"{path.name}: source claims a client capture")
    sequence = document.get("sequence")
    exchanges = document.get("exchanges")
    if not isinstance(sequence, list) or not isinstance(exchanges, list):
        raise ValueError(f"{path.name}: expected replay exchanges and a sequence")
    if sequence[0].get("message") != first_message or sequence[1].get("message") != second_message:
        raise ValueError(f"{path.name}: message-definition sequence does not match C-74")
    if path.name == "c74-keepalive-replay.json":
        exchange = exchanges[0]
        request = frame_header(exchange["request_hex"], f"{path.name} request")
        response = frame_header(exchange["expected_response_hex"], f"{path.name} response")
        if exchange["response_bytes"] != len(response):
            raise ValueError(f"{path.name}: response_bytes does not match response frame")
        if request[4:6] != bytes((1, 3)) or response[4:6] != bytes((1, 4)):
            raise ValueError(f"{path.name}: control opcode does not match keepalive definitions")
        return
    if len(exchanges) < 1:
        raise ValueError(f"{path.name}: expected at least one login exchange")
    for index, exchange in enumerate(exchanges, 1):
        request = frame_header(exchange["request_hex"], f"{path.name} request {index}")
        response = frame_header(exchange["expected_response_hex"], f"{path.name} response {index}")
        if exchange["response_bytes"] != len(response):
            raise ValueError(f"{path.name}: response_bytes does not match response frame")
        if request[4] != 0 or response[4] != 0:
            raise ValueError(f"{path.name}: login vectors must be DML frames")
    auth_response = frame_header(exchanges[0]["expected_response_hex"], f"{path.name} authentication response")
    auth_request = frame_header(exchanges[0]["request_hex"], f"{path.name} authentication request")
    if auth_request[8:10] != bytes((7, 27)):
        raise ValueError(f"{path.name}: authentication request service/order does not match definitions")
    if auth_response[8:10] != bytes((7, 14)):
        raise ValueError(f"{path.name}: authentication service/order does not match definitions")
    expected_auth = AUTH_SUCCESS_BODY if path.name == "c74-login-replay.json" else AUTH_FAILURE_BODY
    if dml_body(auth_response, f"{path.name} authentication response") != expected_auth:
        raise ValueError(f"{path.name}: authentication body does not match its message definition fields")
    if path.name == "c74-login-replay.json":
        if len(exchanges) != 3 or len(sequence) != 4:
            raise ValueError(f"{path.name}: expected authentication, admit, and character-list exchanges")
        admit = frame_header(exchanges[1]["expected_response_hex"], f"{path.name} admit response")
        character_request = frame_header(exchanges[2]["request_hex"], f"{path.name} character-list request")
        character_list = frame_header(exchanges[2]["expected_response_hex"], f"{path.name} character-list response")
        if character_request[8:10] != bytes((7, 8)):
            raise ValueError(f"{path.name}: character-list request service/order does not match definitions")
        if admit[8:10] != bytes((7, 20)) or dml_body(admit, f"{path.name} admit response") != ADMIT_BODY:
            raise ValueError(f"{path.name}: admit body does not encode Status=1 and PositionInQueue=0")
        if character_list[8:10] != bytes((7, 4)) or dml_body(character_list, f"{path.name} character-list response") != CHARACTER_LIST_BODY:
            raise ValueError(f"{path.name}: character-list body does not encode Error=0")


def main():
    try:
        for name in REQUIRED:
            check(ROOT / name)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"C-74 validation failed: {error}", file=sys.stderr)
        return 1
    print(f"C-74 validation passed: {len(REQUIRED)} capture-free replay manifests")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
