# Project Ambrose by Imjustchico
# Checks that every reverse-engineering finding carries the fields Ambrose needs to prove or refute it later, that its claim is about the game rather than about this repository, and that none of them carries game data.
import argparse
import json
import os
import re
import sys

AREAS = ("protocol", "objects", "world", "quests", "combat", "client", "data", "patching")
METHODS = ("capture", "observation", "static", "experiment", "reasoning")
CONFIDENCE = ("low", "medium", "high")
STATUSES = ("claimed", "verified", "refuted")

REQUIRED = ("subject", "area", "claim", "revision", "method", "how_to_repeat", "evidence", "disproof",
            "confidence", "submitted_by", "submitted_on", "status")
VERIFIED_FIELDS = ("verified_by", "verified_on", "verified_how")
REFUTED_FIELDS = ("refuted_by", "refuted_on", "refuted_how")

DATE = re.compile(r"^\d{4}-\d{2}-\d{2}$")
REVISION = re.compile(r"^r\d+\.[A-Za-z0-9_]+$")
BASE64_RUN = re.compile(r"[A-Za-z0-9+/]{120,}={0,2}")
HEX_RUN = re.compile(r"(?:[0-9a-fA-F]{2}[ ,]?){48,}")
ABOUT_THE_REPOSITORY = re.compile(r"\b(?:the |this )?repository (?:does not|has not|holds no|contains no|lacks)\b|\bevidence gap\b|\bgap record\b|\bnot yet (?:contain|establish|verif|have)|\bdoes not yet establish\b", re.IGNORECASE)
GAP_NAME = re.compile(r"(?:^|[-_])gap(?:[-_]|\.json$)", re.IGNORECASE)


def problems_for(path, document):
    found = []

    def fail(message):
        found.append(f"{path}: {message}")

    if not isinstance(document, dict):
        fail("the file must hold one finding as a JSON object")
        return found

    for field in REQUIRED:
        if field not in document:
            fail(f"missing {field}")
    if found:
        return found

    if document["area"] not in AREAS:
        fail(f"area must be one of {', '.join(AREAS)}")
    if document["method"] not in METHODS:
        fail(f"method must be one of {', '.join(METHODS)}")
    if document["confidence"] not in CONFIDENCE:
        fail(f"confidence must be one of {', '.join(CONFIDENCE)}")
    if document["status"] not in STATUSES:
        fail(f"status must be one of {', '.join(STATUSES)}")
    if not DATE.match(str(document["submitted_on"])):
        fail("submitted_on must be a date such as 2026-09-18")
    if not REVISION.match(str(document["revision"])):
        fail("revision must name the client it was found on, such as r806919.Wizard_1_610")

    for field in ("how_to_repeat", "evidence"):
        value = document[field]
        if not isinstance(value, list) or not value or not all(isinstance(step, str) and step.strip() for step in value):
            fail(f"{field} must be a list of steps, each a sentence")

    for field in ("claim", "disproof"):
        if not isinstance(document[field], str) or len(document[field].strip()) < 20:
            fail(f"{field} must say enough to be checked")

    claim = str(document["claim"])
    if ABOUT_THE_REPOSITORY.search(claim):
        fail("the claim is about this repository, not about the game; a finding states what the game does, and a note that nothing is known yet is not one")
    if GAP_NAME.search(os.path.basename(path)):
        fail("the file is named as a gap record; a finding is named for the claim it makes about the game")

    expected_area = path.replace("\\", "/").split("/")
    if len(expected_area) >= 2 and expected_area[-2] != document["area"] and expected_area[-2] != "findings":
        fail(f"the file sits in {expected_area[-2]} but names area {document['area']}")

    if document["status"] == "verified":
        for field in VERIFIED_FIELDS:
            if not document.get(field):
                fail(f"a verified finding needs {field}")
    if document["status"] == "refuted":
        for field in REFUTED_FIELDS:
            if not document.get(field):
                fail(f"a refuted finding needs {field}")

    text = json.dumps(document)
    if BASE64_RUN.search(text):
        fail("a long run of base64 looks like game data; describe it instead")
    if HEX_RUN.search(text):
        fail("a long run of hex bytes looks like game data; give offsets and sizes instead")
    return found


def check_paths(paths, root):
    problems = []
    for path in paths:
        full = path if os.path.isabs(path) else os.path.join(root, path)
        try:
            with open(full, encoding="utf-8") as handle:
                document = json.load(handle)
        except FileNotFoundError:
            problems.append(f"{path}: no such file")
            continue
        except json.JSONDecodeError as error:
            problems.append(f"{path}: not valid JSON ({error})")
            continue
        problems.extend(problems_for(path, document))
    return problems


def findings_in(root):
    folder = os.path.join(root, "contrib", "findings")
    found = []
    for base, _, names in os.walk(folder):
        for name in names:
            if name.endswith(".json"):
                found.append(os.path.relpath(os.path.join(base, name), root).replace("\\", "/"))
    return sorted(found)


def main():
    parser = argparse.ArgumentParser(description="Project Ambrose findings check")
    parser.add_argument("--root", default=os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    parser.add_argument("--paths", nargs="*", help="findings to check; all of them when left out")
    arguments = parser.parse_args()

    paths = arguments.paths if arguments.paths else findings_in(arguments.root)
    if not paths:
        print("findings: none to check")
        return 0

    problems = check_paths(paths, arguments.root)
    for problem in problems:
        print(problem)
    print(f"findings: {len(paths)} checked, {len(problems)} problem(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
