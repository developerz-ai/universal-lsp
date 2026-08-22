# 02 — Gate and guards

> Part of [overview.md](overview.md). Depends on: 01

## Files

- `scripts/lib/gate.ts` — the gate, defined once, as data.
- `scripts/lib/affected.ts` — pure change detector, no git/fs/process.
- `scripts/check.ts`, `scripts/format.ts`, `scripts/setup.ts`, `scripts/doctor.ts`,
  `scripts/changed.ts`, `scripts/help.ts`.
- `scripts/lint/{graph-lite-vocabulary,graph-lite-boundary,max-file-loc}.ts` + tests.
- `scripts/lint/index.ts` — discovers guards from the directory; no registry.
- `bin/{setup,dev,check,fmt}` — thin shims.

## Steps

1. Gate steps carry a `scope`: `always` (whole-tree), `build`, `full` (sanitizers).
2. `computeAffected` fans out through the one-way dependency graph and widens to
   everything on a root/build-config change.
3. Vocabulary guard tokenises camelCase and snake_case — `\bsymbol\b` misses
   `SymbolNode`, which is the shape the leak actually takes.
4. Every guard takes an optional root so its test can point it at a fixture tree.
5. `index.ts` discovers guards rather than listing them: a registry is a file
   somebody has to remember.

## Tests

- `scripts/lib/affected.test.ts` — fan-out, no reverse fan-out, docs-only, filters.
- One `<rule>.test.ts` per guard: flags a bad sample, passes a good one, passes the
  real tree.
- Command: `bun test scripts/`

## Done when

- `bin/check` green; `--files` and `--changed` narrow the step list.
- A `SymbolNode` in `graph-lite/include/` fails the gate.
