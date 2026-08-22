---
description: Write a multi-file execution plan to docs/plans/<YYYY>/<MM>/<DD>/<1NN>-<slug>/ for another agent to execute
argument-hint: [what you want done]
allowed-tools: Write, Read, Glob, Grep, Task, Bash
---

# /planx

Plan only. **No implementation, no builds, no edits outside the plan dir.**

## Goal
$ARGUMENTS

## Steps

1. **Resolve the path.** `date +%Y %m %d`. Dir = `docs/plans/<YYYY>/<MM>/<DD>/`.
   `Glob docs/plans/<YYYY>/<MM>/<DD>/1*` → next = highest `1NN-*` + 1, else `101`.
   Slug kebab-case, ≤ 5 words.

2. **Explore read-only** (`Task`, subagent_type=Explore, "very thorough"): the
   files to touch with `file:line`, which subsystem owns them
   (`graph-lite` / `engine` / `analysis` / `plugins`), the existing tests and how
   they are registered (`ulsp_add_test`), and anything in
   [`docs/research/things-to-know.md`](../../docs/research/things-to-know.md) or
   [`docs/gotchas.md`](../../docs/gotchas.md) that already covers this ground.
   Skip only for trivial asks.

3. **Write `overview.md` plus one `<NN>-<aspect>.md` per separable area of work.**
   Never a single `plan.md`. Split by area, not by chapter: the slice boundary is
   the file-set boundary an executor locks on, and the file set is the PR.
   Shapes and rules: [`docs/plans/README.md`](../../docs/plans/README.md).

4. **Write `status.yml`** — the one tracker. `created_by`/`owner` from
   `git config user.name`; leave `worked_by: ""` so another agent can claim it.

## Rules

- Compact English. Fragments. Tables. `file:line` and `Class::method` over prose.
- **Reference, never paste.** Pasted code is stale on arrival.
- No checkboxes in the `.md` slices — `status.yml` is the only tracker.
- Assume `CLAUDE.md`; do not restate it.
- If a slice touches `graph-lite/`, say explicitly which vocabulary the executor
  may **not** use — that guard fires late otherwise.
