<!-- Project Ambrose by Imjustchico: The record of every client revision typeextract has built a dump from, with what it derived, what it assumed and how the result was checked. -->

# Clients extracted

typeextract works out where the client keeps its type data rather than reading it from a table, and this is the record of the clients it has been run against. Add a row whenever a revision is extracted for the first time, or when a revision's result changes. A row holds counts and how they were checked, never anything read out of the client.

A field is **derived** when the running client placed it, and **assumed** when the tool fell back to a written offset. `--require-derived-layout` refuses an extraction with any assumed field, naming the first, and writes no dump.

| Revision | Extracted | Taken as | Derived | Assumed | Classes | Properties | Checked against |
|---|---|---|---|---|---|---|---|
| r806919 | 2026-10-07 | the pinned build | 33 of 33 | none | 6,986 | 49,465 | The saved r806919 dump with `--require-derived-layout --compare`: 0 differences. Every derived field equals the written offset. |
| r801440 (`Wizard_1_610`) | 2026-10-07 | an unknown build, no written offset used | 33 of 33 | none | 6,987 | 49,456 | The same 33 offsets as r806919, field by field, which are the offsets written down for r801440. |

## How each field is placed

- **std::map node:** the tree's shape, its red-black colors, keys that rise in order, and values that hold and name their keys.
- **Type.pointer, Type.property_list, PropertyList.name:** the types the client registers.
- **PropertyList base and singleton:** a probe of the client's PropertyList constructor.
- **Property vector, name, type and hash:** the property hashes.
- **Property id and container:** list positions, and the container vtable entries called on copies.
- **Property offset, flags and options:** a probe of a property adder.
- **Enum options:** their vectors.

Every probe that runs before the walk is undone from a saved snapshot of guest memory and heap, so the walk reads the registry as the client's own initializers left it.
