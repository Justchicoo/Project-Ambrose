<!-- Project Ambrose by Imjustchico: Where a contributor's authored world rows wait while their pull request is open, how the file is named, and the shape the door destination table starts from. -->

# data/sql/updates/pending_db_world

World rows you authored yourself land here. doc/ARCHITECTURE.md's SQL convention sends open pull requests to `pending_db_<name>/`, and the maintainer moves the file into `data/sql/updates/db_world/` when it is merged, where the updater applies it in order and records it. A file here is not parked outside the server: `data/sql/updates/db_world/2026_01_01_00.sql` adds this folder to `updates_include` with the state `PENDING`, and the updater reads every folder that table lists. A `.sql` file sitting here is applied at the next start of a server pointed at this checkout, after the released updates and before the custom ones, and recorded as `PENDING`. That is what lets a reviewer run your rows; it also means the file must hold only what its pull request is about, because anyone who checks the branch out gets it.

It does not go in `data/sql/custom/db_world/`. That folder is a machine's own local SQL, which is never upstreamed.

## The file

One file per pull request, named `rev_<unix seconds>_<short-name>.sql`: the moment you wrote it, which `date +%s` prints, and a few words for what it holds, in letters, digits, hyphens and underscores, such as `rev_1790241513_zone-teleport.sql`. That is the only name the updater accepts for a pending file, and two open pull requests never choose the same one. When the pull request merges, the file is renamed to the next free `YYYY_MM_DD_NN.sql` in `data/sql/updates/db_world/`, as Content, SQL and releases in doc/ARCHITECTURE.md settles; that is the maintainer's work, not yours, and nothing about your rows changes. The file starts with the two-line SQL branding header and carries no other comment:

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
