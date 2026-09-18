<!-- Project Ambrose by Imjustchico: Where a contributor's authored world rows wait while their pull request is open, how the file is named, and the shape the door destination table starts from. -->

# data/sql/updates/pending_db_world

World rows you authored yourself land here. doc/ARCHITECTURE.md's SQL convention sends open pull requests to `pending_db_<name>/`, and the maintainer moves the file into `data/sql/updates/db_world/` when it is merged, where the updater applies it in order and records it. Nothing waiting here touches anybody's database: `data/sql/base/db_world/updates_include.sql` lists the released and custom folders and not this one, so a file sitting here is reviewed, not run.

It does not go in `data/sql/custom/db_world/`. That folder is a machine's own local SQL, which is never upstreamed.

## The file

One file per pull request, named `YYYY_MM_DD_NN.sql` for the day you wrote it with `NN` counting from `00` within that day, which is what doc/ARCHITECTURE.md asks of every update and what the updater demands of this one once it is moved. The name must be unique across every update folder of this database, so read `data/sql/updates/db_world/` before choosing one. How pending files are finally named is still an open decision in doc/ROADMAP.md, so the maintainer may rename yours when it is promoted; that is their work, not yours, and nothing about your rows changes. The file starts with the two-line SQL branding header and carries no other comment:

```sql
-- Project Ambrose by Imjustchico
-- Door destinations for the Wizard City starting area, authored by hand from client identifiers.
```

## The door destination table

Items C-01 and C-48 author door destinations. Milestone 6.14 is what creates `zone_teleport` and reads it, and phase 10 is the content that walks a player through it, so rows written today use that shape and keep the create above them, so the file stands on its own whichever lands first:

```sql
CREATE TABLE IF NOT EXISTS `zone_teleport` (
    `zone` VARCHAR(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    `trigger_name` VARCHAR(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    `dest_zone` VARCHAR(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    `dest_location` VARCHAR(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT '',
    `transition_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `same_zone` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`zone`, `trigger_name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
```

A row says that walking into `trigger_name` in `zone` arrives at `dest_location` in `dest_zone`, with `same_zone` set when the destination is the zone the player is already standing in. Leave `transition_id` at `0` unless you can say what the value you chose means.

Every column holds a client identifier: a zone name, a trigger name, a location name. doc/ARCHITECTURE.md settled on 2026-09-16 that a destination table naming zones, locations and triggers that way may be committed. Display text may not, and no file, asset or run of bytes from a client install ever may.

## Why this is authored and not extracted

These rows are authored data, not derived from the client. `ResTeleport` carries no properties at all in the r806919 type dump, so nothing found so far in the client's own data says where a door leads. Finding F-07 is the open question of what a teleporter really carries. Until it is answered, this table is written and reviewed by hand: say in your pull request how you checked each row, and expect the shape to be revisited if F-07 shows the destination is in the client's data after all.
