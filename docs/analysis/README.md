# analysis/

The translation layer. Turns tree-sitter output and a plugin's Tier 2/3 rules into
calls against graph-lite's generic API: a "symbol" becomes a node with a caller-
defined type tag, "calls" becomes a typed directed edge.

**This is the only place in the repository where both vocabularies may meet.**
Everything LSP-shaped stops here on the way down; everything graph-shaped stops
here on the way up. See
[`../architecture/graph-lite-boundary.md`](../architecture/graph-lite-boundary.md).

> Pre-implementation. First pass is build phase 5.

## What it owns

| Concern | Shape |
|---|---|
| Node type tags | The vocabulary — `function`, `class`, `module`, `file` — defined here, opaque to the library |
| Edge type tags | `calls`, `imports`, `defines`, `references` |
| Id mapping | LSP identity (URI + range, or a stable symbol key) ↔ graph node id |
| Incremental update | A file changed → which nodes and edges are stale, and removing them without orphaning |
| Query shaping | Impact radius, call path, architecture overview — composed from traversals, exposed to MCP |

## Rules

- **No LSP type crosses into a graph-lite call.** Translate at the seam; do not
  pass a struct through and let the library "just use the id field".
- **Id mapping is a one-way door.** Graph node ids persist; the scheme that
  produces them from source locations has to survive a file being renamed and a
  function moving down forty lines. Decide it deliberately, with an ADR.
- **Deletion must not orphan.** An edge pointing at a removed node is a corrupt
  answer, not a missing one.
