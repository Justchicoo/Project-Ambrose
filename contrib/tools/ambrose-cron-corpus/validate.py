# Project Ambrose by Imjustchico
# Validates C-42 corpus shape, ordering, and timestamp syntax without implementing cron semantics.

import json
from datetime import datetime
from pathlib import Path


def main() -> None:
    corpus_path = Path(__file__).with_name("corpus.json")
    corpus = json.loads(corpus_path.read_text(encoding="utf-8"))
    cases = corpus["cases"]
    if corpus["item"] != "C-42" or len(cases) != 14:
        raise ValueError("unexpected C-42 corpus metadata")

    for case in cases:
        expected_utc = [
            datetime.fromisoformat(value.replace("Z", "+00:00"))
            for value in case["expected_utc"]
        ]
        if expected_utc != sorted(expected_utc) or len(set(expected_utc)) != 5:
            raise ValueError(f"unsorted or duplicate UTC runs: {case['id']}")
        expected_local = [datetime.fromisoformat(value) for value in case["expected_local"]]
        if len(expected_local) != 5 or any(value.second != 0 for value in expected_local):
            raise ValueError(f"invalid local runs: {case['id']}")

    print(f"validated {len(cases)} C-42 cases and local/UTC projections")


if __name__ == "__main__":
    main()
