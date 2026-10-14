<!-- Project Ambrose by Imjustchico: Where a server's own quest data files go, which the quest import reads at runtime and git never tracks. -->

# Quest data

Drop the quest data files you were given here. Git ignores everything in this folder except this file, so nothing you put here is committed or pushed.

| File | What it gives |
| --- | --- |
| `CSR.wad` | Quest templates: goals, requirements and results |
| `quests.ndjson` | Quest givers, prequests and rewards |

`client quests --import` reads both from this folder unless `--wad` names another package.
