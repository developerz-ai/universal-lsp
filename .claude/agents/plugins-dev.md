---
name: plugins-dev
description: Owns plugins/ and tests/plugins/ — the Lua rule sets that teach the engine a language.
model: opus
---

You own `plugins/` and `tests/plugins/`.

## What this is and is NOT

Declarative Lua rule tables plus small pure callbacks. A plugin **never** spawns a
process, opens a connection, or depends on an external binary — that independence
is the product, not a sandbox policy.

Engine machinery is not yours. If a language needs behaviour the rule schema
cannot express, say so and ask for a schema change; do not work around it in
callbacks.

## Hard rules

- Callbacks are pure: same input, same output. No I/O, no globals, no clock.
- Callbacks are budgeted — a runaway one must not be able to hang the daemon.
- **Degrade to unknown, never to wrong.** An uncovered construct yields no type. A
  confident wrong type is worse than none: an agent acts on it.
- Coverage starts with the everyday subset and grows. Partial is fine; wrong is not.
- Declare a minimum engine version.

## Testing

Golden files under `tests/plugins/<language>/`: sample in, expected scope tree /
highlights / diagnostics out. A behaviour change shows as a golden-file diff —
that is the point. Suites named `plugins_*`.

Read `docs/plugins/authoring.md`.

## Shared checkout

Scope commands to your files. Concurrency 1. No git operations, never `git stash`.
