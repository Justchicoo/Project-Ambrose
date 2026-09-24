<!-- Project Ambrose by Imjustchico: C-41 dashboard definition for the metrics and operations overview. -->

# C-41: Ambrose dashboard definition

This folder contains a reviewable dashboard definition for the future panel. It names the figures each page assumes, their source stream, unit, permission, freshness budget, and supported history ranges. It is configuration for a future dashboard implementation, not a replacement for the panel or metrics endpoint.

The definition covers the figures promised by `doc/PANEL.md` and phase 17.19:

- host and app health: CPU, memory, threads, handles, network, disk, sessions, players, character-select sessions, and tick time;
- operations state: last reload, settings generation, pending SQL updates, client revision, and active downloads; and
- freshness and range behavior: one-second live samples, a five-minute to thirty-day history selector, and a stale state after the declared budget.

Every series names its permission and source. Figures owned by later milestones are marked `availability: milestone` with the owning milestone instead of being silently shown as zero. The validator rejects duplicate ids, unknown references, missing permissions, invalid ranges, and a stale budget shorter than the sample interval.

## Run the validator

```powershell
cd contrib\tools\ambrose-dashboard-definition
python validate.py
```

It reads only `dashboard.json` and prints the number of pages and series checked. The check is structural; it does not contact a server or claim that the future metrics endpoint already exists.

## Evidence and limitations

The figures, permissions, one-second subscription cadence, five-minute to thirty-day ranges, and stale-sample requirement come from `doc/PANEL.md` and the phase-17.06 and 17.19 acceptance text. The grouping and labels are a proposed reviewable configuration for C-41. A maintainer may rename a figure or move it between pages, but the endpoint contract and dashboard must then be updated together. This file does not define alert rules, player-identifying data, or client-derived content.
