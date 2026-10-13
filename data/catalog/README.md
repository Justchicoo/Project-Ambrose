<!-- Project Ambrose by Imjustchico: How the template id catalog is laid out, what each file holds and how to regenerate it from your own install. -->

# Template id catalog

Every template id the Wizard101 client lists, with every zone, sorted into categories so you can find an id by name without opening the game files. The servers never read this folder; they still load each template from the user's own install.

## Layout

One folder per client revision, such as `r806919.Wizard_1_610/`:

| File | Holds |
|---|---|
| `index.json` | The revision, the locale, the counts, and each category with its files and how many templates each group holds |
| `templates/<Category>.json` | One category's templates, one per line, sorted by group and then id |
| `templates/<Category>-01.json`, `-02.json`, ... | The same, split into parts when a category is too big for one file |
| `zones.json` (or `zones-01.json`, ...) | Every zone with its display name, named locations and the template ids it places |

## Categories

The category comes from a template's class, and for game objects and items from the behaviors that say what it is:

| Category | What is in it |
|---|---|
| Mobs | Game objects with a mob deck (MobDeckBehavior) |
| NPCs | Other game objects with NPCBehavior: quest givers, vendors, trainers, ambient characters |
| Objects | Other game objects: props, doors, signs, interactables |
| Equipment | Items with jewel sockets: hats, robes, boots, wands, athames, amulets, rings |
| Decks | Deck items and deck templates |
| Jewels | Jewels and pet jewels |
| Housing | Furniture, wallpaper, music, garden plants, house guests and other housing items |
| Pets | Pet items and pet actors |
| Mounts | Mount items |
| Elixirs, Reagents, PetSnacks | Items of those kinds |
| Items | Every other item |
| Spells | Spells, treasure cards, tiered, castle magic, cantrips, gardening, fishing and fusion spells |
| Recipes | Crafting recipes |
| PetTalents, Polymorphs, Bundles, Sounds, Triggers, Sigils, PvP, Schools, Currency, Cinematics, Tutorials, SetBonuses | Templates of those classes |
| Other | Anything no rule above covers |

## A template line

```json
{"id":1515210,"name":"Wand-KM-L130-Fire-01","display":"Fiery Liaison's Sword","class":"WizItemTemplate","group":"KM Equipment/Wands","behaviors":["RenderBehavior","JewelSocketBehavior"],"displayKey":"Items_00022015","archive":"Root.wad","path":"ObjectData/KM Equipment/Wands/Wand-KM-L130-Fire-01.xml"}
```

- `id` is the template id the client and the servers use.
- `name` is the object name, and `display` the English name players see, empty when the template has none.
- `group` is the folder below ObjectData that holds the template, which is where the client keeps it by world or set.
- `path` and `archive` say where the template lives in the install, for `client template <id>`, which prints the whole template.

## Finding things

- By name: `grep -i "fiery liaison" data/catalog/*/templates/Equipment*.json`
- By id: `grep '"id":1515210,' -r data/catalog`
- Everything in one world's folder: `grep '"group":"KM Equipment' data/catalog/*/templates/*.json`
- From code, read `index.json` and then the files each category lists.

## Regenerating

From a checkout, with the client tool built and your own install found as the other tools find it:

```
client catalog data/catalog
```

It rewrites the revision's folder whole. A new client revision writes a new folder next to the old one.
