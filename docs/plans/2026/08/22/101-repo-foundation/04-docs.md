# 04 — Documentation

> Part of [overview.md](overview.md). Depends on: none

## Files

`README.md` · `docs/README.md` · `docs/spec.md` ·
`docs/research/things-to-know.md` · `docs/{graph-lite,engine,analysis,plugins}/**`
· `docs/architecture/**` (incl. 3 ADRs) · `docs/standards/**` · `docs/gotchas.md`
· `docs/plans/README.md` · `CONTRIBUTING.md` · `SECURITY.md` ·
`CODE_OF_CONDUCT.md` · `CHANGELOG.md`

## Steps

1. `docs/graph-lite/**` is written as though the library already stands alone —
   and is scanned by the vocabulary guard, so the forcing function is real rather
   than aspirational.
2. Anything that must discuss both vocabularies at once (the boundary rule itself)
   lives in `docs/architecture/`, since it cannot be written in the words the
   guard permits.
3. Every doc referenced from an index exists — `scripts/lint/doc-links.ts` fails
   the gate on a dead relative link, so the index cannot rot silently.
4. Preventive rules in `CLAUDE.md` (read every session); symptom lookups in
   `docs/gotchas.md` (read when something breaks). Keeping the split is what keeps
   `CLAUDE.md` from bloating into a runbook.

## Done when

- No dead relative links, asserted by a guard.
- `CLAUDE.md` under 600 lines.
