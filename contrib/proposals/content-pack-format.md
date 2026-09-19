<!-- Project Ambrose by Imjustchico: Proposal for a signed, reversible content-pack format for authored server data. -->

# Proposal: content-pack format

## Item

C-49: define the manifest, versioning, installation, update, and uninstall rules for authored content packs.

## Scope

This proposal turns authored data such as world SQL updates into an operator-installable unit. It does not change the roadmap, database updater, admin API, patch server, or any existing schema. Those implementation changes belong to the roadmap milestone that accepts this shape.

The pack format must remain separate from the game client. A pack contains Ambrose-authored data only: dated database updates, server data files, documentation, and metadata. It never contains a client archive, asset, dump, protocol file, capture, encoded client bytes, or a file copied from an installation.

## Manifest

A pack has one manifest at its root. The manifest should contain at least:

```json
{
  "format": 1,
  "id": "example.world.doors",
  "name": "Example world doors",
  "version": "1.0.0",
  "ambrose": {
    "minimum_version": "0.0.0",
    "maximum_version": null
  },
  "license": {
    "spdx": "MIT",
    "notice": "NOTICE.txt"
  },
  "contents": [
    {
      "path": "sql/world/2026_09_19_00.sql",
      "kind": "world_update",
      "sha256": "..."
    }
  ],
  "requires": [],
  "conflicts": []
}
```

The final schema should settle:

- a stable reverse-domain or maintainer-scoped pack id;
- a version format with ordered versions and no silent downgrade;
- the supported Ambrose version range;
- the licence identifier and any required notice files;
- every path and its kind;
- a SHA-256 for every payload file;
- required packs and minimum versions;
- conflicting packs or mutually exclusive data sets; and
- an optional signature referring to the exact manifest and payload hashes.

Paths must be relative, normalized, unique, and confined to the pack root. The installer must reject absolute paths, parent traversal, duplicate entries, unknown content kinds, missing payloads, hash mismatches, and files present in the archive but absent from the manifest.

## Versioning and installation journal

Installing a pack should validate the complete archive before changing a database or the server's data directory. After validation, one transaction should:

1. record the pack id, version, manifest hash, signer, install time, and source;
2. record every content file and its hash;
3. record dependency versions and the Ambrose version used;
4. apply the pack's dated database updates through the existing updater; and
5. record the resulting update names and ownership.

The journal must be the source of truth for what an installation owns. A pack update is a new version transition in the same journal, not an overwrite that loses the previous manifest. A newer version must declare whether it adds, replaces, or removes each owned file and update. Downgrades require an explicit supported reverse transition; otherwise they are refused.

The operation must be atomic from the operator's perspective. If validation, dependency checks, a database update, or file placement fails, the journal and installed data must remain at the prior version. Error output should identify the first failing path or update and redact credentials.

## Uninstall and dependency rules

Uninstall should reverse only data that the journal proves the pack owns. It must refuse to remove:

- a pack required by another installed pack;
- a row or file changed by an operator after installation;
- an update that a later pack depends on; or
- a shared resource whose ownership is ambiguous.

The refusal must name the dependent pack, owned path, or update rather than silently leaving a partial uninstall. When uninstall is safe, it should reverse the pack's journal entries in reverse dependency order, remove exactly its files and rows, and record the uninstall result. A failed reversal must leave enough journal information for a retry and must not claim success.

## Signing and clean-room checks

Signatures should cover the canonical manifest bytes and every listed SHA-256. Verification uses keys explicitly trusted by the operator. Unsigned packs are refused by default; an operator may opt in to an unsigned local pack with an explicit acknowledgement recorded in the journal.

The installer must reject forbidden client-derived files by extension and content checks, including archives, assets, captures, protocol XML, type dumps, and encoded client bytes. The format and its command output should state that a runtime reader may inspect a user's own installation, but a content pack may not carry that data into the server.

## Cheapest disproof

Before implementing this format, construct disposable archives for these cases and confirm that the proposed validation boundary can distinguish them:

- a valid pack with one world update;
- a missing manifest entry;
- a payload whose SHA-256 does not match;
- a path containing `..`;
- a dependency on an absent pack;
- a second pack depending on the first;
- an operator edit after installation;
- an unsigned pack; and
- a client archive or type dump placed in the payload.

If the proposed journal cannot tell which pack owns a row or file, or cannot restore the prior state after a failed update, this format is not ready and the ownership model must be revised before implementation.

## Dependencies and cost

Implementation depends on the existing dated SQL update convention, the database updater and journal, the admin API or an equivalent local command, and the patch-server signing work cited by the roadmap. It also needs a canonical archive reader and a test fixture generator that creates synthetic data only.

No client installation, client capture, KingsIsle executable, external network service, or game-derived fixture is required. Tests should use temporary databases and temporary files, clean them up, and never commit generated packs or their contents.

## Acceptance

The proposal is ready for implementation when a maintainer can point to one command or admin operation that:

- validates a manifest and every listed hash before installation;
- rejects traversal, duplicate, missing, unknown, and client-derived payloads;
- installs a valid pack and records its id, version, contents, dependencies, licence, and signer;
- updates a pack through a journaled version transition;
- refuses unsafe uninstall with a useful dependency or ownership explanation;
- uninstalls a safe pack without removing later operator changes;
- rolls back or preserves the previous state after a failed operation; and
- leaves no credentials, client data, or temporary files behind.
