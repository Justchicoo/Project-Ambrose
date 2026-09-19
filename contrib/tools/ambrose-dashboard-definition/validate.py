# Project Ambrose by Imjustchico
# Validates the C-41 dashboard definition without contacting a server.

import json
from pathlib import Path


def main() -> None:
    definition = json.loads(Path(__file__).with_name("dashboard.json").read_text(encoding="utf-8"))
    if definition["item"] != "C-41":
        raise ValueError("unexpected item number")
    if definition["stale_after_seconds"] <= definition["sample_interval_seconds"]:
        raise ValueError("stale budget must exceed the sample interval")
    ranges = definition["history_ranges"]
    if [entry["seconds"] for entry in ranges] != sorted(entry["seconds"] for entry in ranges):
        raise ValueError("history ranges are not ordered")
    series = definition["series"]
    series_ids = {entry["id"] for entry in series}
    if len(series_ids) != len(series):
        raise ValueError("duplicate series id")
    for entry in series:
        if not entry["permission"] or not entry["source"] or not entry["scope"]:
            raise ValueError(f"incomplete series: {entry['id']}")
    page_ids = {page["id"] for page in definition["pages"]}
    if len(page_ids) != len(definition["pages"]):
        raise ValueError("duplicate page id")
    referenced = set()
    for page in definition["pages"]:
        if not page["permission"]:
            raise ValueError(f"missing page permission: {page['id']}")
        for panel in page["panels"]:
            for series_id in panel["series"]:
                if series_id not in series_ids:
                    raise ValueError(f"unknown series: {series_id}")
                referenced.add(series_id)
    if referenced != series_ids:
        raise ValueError("series is not placed on a page")
    print(f"validated {len(definition['pages'])} pages and {len(series)} series")


if __name__ == "__main__":
    main()
