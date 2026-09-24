<!-- Project Ambrose by Imjustchico: How a reverse-engineering finding is submitted, what evidence it carries, and how Ambrose proves it before anything is built on it. -->

# contrib/findings

This is the most valuable thing an outside contributor can add: a claim about how Wizard101 actually behaves, written so that Ambrose can prove it later without taking anyone's word for it.

Ambrose is built on proven data. Nothing here is believed because it sounds right; every finding is re-derived by the project against its own tools and a real client before a milestone is built on it. A finding that cannot be checked is not useful, however plausible, because a wrong fact discovered later costs more than the finding saved.

## What a finding is

One file per claim, `contrib/findings/<area>/<subject>.json`, where `<area>` is one of `protocol`, `objects`, `world`, `quests`, `combat`, `client`, `data` or `patching`. A claim is a single statement that could be shown false: what a message field means, what order a duel resolves in, which file a zone's objects come from, what a flag does.

The file carries the claim, how it was found, exactly how someone else repeats that, and what would disprove it:

```json
{
  "subject": "MSG_CHARACTERINFO.CharacterInfo",
  "area": "protocol",
  "claim": "The blob is a WizardCharacterCreationInfo whose m_nHairColor field is 6 bits, so a value above 63 is truncated rather than refused.",
  "revision": "r806919.Wizard_1_610",
  "method": "capture",
  "how_to_repeat": [
    "Log a wizard into your own server and reach character select.",
    "Capture the loopback traffic on the login port with tshark.",
    "Build the type dump of your own install with `typeextract --out dump.json`, and read WizardCharacterCreationInfo's properties from it.",
    "Find the MSG_CHARACTERINFO frame and read its payload against that property list, counting the bits every property before m_nHairColor takes.",
    "Set m_nHairColor to 64 in your own server's reply and watch the client render colour 0."
  ],
  "evidence": [
    "Frame 41 of the capture carries 157 bytes whose first four are the class hash 292458316.",
    "Bit 41 of the decoded blob begins the field; the next six bits hold the value."
  ],
  "disproof": "A client that renders colour 64 correctly, or a capture where the field spans more than six bits.",
  "confidence": "high",
  "submitted_by": "your name or handle",
  "submitted_on": "2026-09-18",
  "status": "claimed"
}
```

The tools decode objects, not captures. `bindecode` reads one KIWAD archive of your own install: `bindecode --client <your install> --wad Root.wad --list <pattern>` prints the entry names that contain a pattern, and `bindecode --client <your install> --wad Root.wad <entry>` prints that entry as JSON. It takes no bytes from a capture and reads nothing from standard input, so a captured payload is read against the type dump `typeextract` builds, as the steps above do.

`method` is one of `capture` (your own session against your own server), `observation` (what your own client does on screen), `static` (reading your own copy of a file or program with your own tools), `experiment` (you changed something and watched the result) or `reasoning` (derived from other findings, which you name). Anything from a public source names the source and its licence, and waits for the maintainer to accept it.

`python apps/ci/ci_findings.py --paths contrib/findings/<area>/<file>.json` checks the shape of your file before you open a pull request, and CI runs it for you.

## What a finding never carries

- No bytes from the game client: no archive, asset, text, image or extracted table, and no blob pasted into the file. Offsets, field names, sizes, counts and hashes are facts about the data and are welcome; the data itself is not.
- No code, table or text copied from another server project, wiki or site. Say what you observed, not what someone else wrote.
- Nothing that requires running KingsIsle's launcher or patcher against the pinned development install. Your own copy is your own choice.

## How Ambrose proves it

A merged finding is `claimed`, and nothing is built on it yet. When a milestone needs it, the project re-derives it: a capture of its own, a run of the client driver, a decode with its own tools, or a test written to fail if the claim is wrong. The same file then records the outcome, and the milestone cites the finding:

```json
  "status": "verified",
  "verified_by": "3.15 CreationInfo decode and validation",
  "verified_on": "2026-10-02",
  "verified_how": "CreationInfoTest asserts the six-bit field against a capture of the maintainer's own client; a seventh bit fails the test."
```

A finding that does not survive becomes `refuted`, with `refuted_how` saying what actually happens. That is a good outcome and the file stays: knowing a plausible thing is false is worth as much as knowing a true one, and it stops the next person chasing it.

Findings still `claimed` are never quoted as fact in the repository's documents. A document may cite one as a lead, saying it is unverified.

## Optional machine check

A finding may carry the optional `machine_check` object proposed in
`contrib/proposals/machine-checkable-findings.md`. Before a runner is trusted,
validate the block without executing it:

```powershell
.\contrib\tools\ambrose-finding-check\build\Debug\ambrose-finding-check.exe `
  contrib\findings\protocol\example.json
```

The dependency-free validator accepts schema `1` and kind `command`. It
requires a regular repository-relative `entrypoint` below the finding's own
folder, an argv `arguments` array, unique declared `inputs`, and `pass`,
`fail`, and `unable` result contracts with exit codes `0`, `1`, and `77`.
`${name}` placeholders must refer to exactly one declared input. The
validator is deliberately dry: it never opens an input, starts an entrypoint,
runs a shell, contacts a network, or changes a finding's `claimed` status.
Missing `machine_check` blocks remain valid.

## Which findings are wanted most

The list in doc/CONTRIBUTOR-TRACK.md names them with their ids. The short version: anything phase 5 and later needs and nobody has written down, above all combat, quests, pets, housing and the zone data, and anything that tells Ambrose what a newer client revision changed.
