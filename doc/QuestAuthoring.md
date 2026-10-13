<!-- Project Ambrose by Imjustchico: How to scaffold quest SQL and follow the key-only authoring rules. -->
# Quest authoring

Quest content belongs in the world database. Use client identifiers in authored SQL, never display text: locale keys, template ids, persona names, quest names, goal names and zone names.

## Scaffold support

Use `questtool scaffold --title-key <key> --giver <template-id> --lang-stem <stem>` to write a skeleton update under `data/sql/updates/pending_db_world/`. Keep the filename in the pending-update naming format; the merge process promotes it to the next dated update.

The scaffold creates a quest, its initial persona goal and start-goal relation, a `Prep` dialog, and a first dialog entry bound to the giver's template id. The persona name and generated locale keys are placeholders to edit before applying the update. A persona goal's `persona_name` must exactly match an `object_name`.

## Table reference

- `quest_template` holds the quest name, locale keys, progression flags and optional requirement-list id.
- `quest_start_goal` names the goals active when the quest starts.
- `quest_goal` defines each goal's type, locale keys, destination, persona, tally and zone fields. Supported types are `persona`, `waypoint`, `bounty`, `bounty_collect`, `scavenge`, `usage` and `achieve_rank`.
- `quest_goal_adjective` and `quest_goal_client_tag` hold ordered adjective and client-tag identifiers for a goal. Bounty matching adjectives belong in `quest_goal_adjective`.
- `quest_goal_logic` defines each completion outcome for a goal; `quest_goal_logic_member` supplies its `AND`, `OR` and `ADD` goal references.
- `quest_dialog` declares a dialog owned by a quest or goal and its `Start`, `Prep`, `Underway`, `Completion` or `Complete` tag. Quest-owned rows use an empty `goal_name`; goal-owned rows name a goal.
- `quest_dialog_entry` stores each ordered dialog line, including its actor template id and `dialog_key`. `quest_dialog_madlib` attaches substitution values to a specific line and identifier.

## Reference index

Build a local server-class supplement from the user's install with `schemaprobe --client <install> --type-dump <dump> --all-wads --server-classes <classes-file>`. Keep that client-derived file outside the repository. Then run `questtool refs --client <install> --type-dump <dump> --classes <classes-file> --zone <zone-path-fragment>` to list each decoded `ReqHasQuest` and `ReqHasGoal` with its zone, source and goal. The command uses the shared zone extractor and exits nonzero if a relevant trigger, volume, spawner or requirement part cannot be decoded.

## Authoring rules

Use locale keys for quest and goal text. A `*_key` SQL column must never contain display text. The automated SQL-key codestyle check is not included yet because this milestone has no grant to change the codestyle checker; this branch does not enforce the rule.

Build goals from the supported types and keep each goal name unique within its quest. A bounty or bounty-collect goal needs at least one adjective and a nonzero tally count. Logic entries should refer only to goals in the same quest; a logic entry cannot both add goals and complete the quest.

Use the first ordered line of the quest-owned `Prep` dialog for the giver binding; its `actor_template_id` is the giver template id. `AND` members require all named goals, `OR` members accept any named goal, and `ADD` members schedule named goals. Keep `complete_quest` separate from `ADD` members.

The quest validator and CSR importer are not implemented yet. Do not treat a scaffold as validated or imported content as exportable until those commands are available.

## Reload and test

After the update is reviewed and applied to a local world database, run `.reload quest_template`. The reload validates the complete quest data before replacing the serving generation; a rejected reload leaves the prior generation active. Test the giver and persona in the client when the quest offer and completion path are available in the running server.

Content imported from a CSR package is local-only and retains the package name as its source. Do not scaffold or export imported rows into `data/sql`.
