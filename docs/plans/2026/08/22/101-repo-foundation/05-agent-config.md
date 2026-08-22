# 05 — Agent configuration

> Part of [overview.md](overview.md). Depends on: 02, 04

## Files

- `CLAUDE.md` + `AGENTS.md` (symlink — one source of truth, copies drift)
- `.claude/settings.json` — permissions + a post-edit format hook
- `.claude/commands/{planx,feature,check,fix-ci}.md`
- `.claude/agents/{architect,graph-lite-dev,engine-dev,analysis-dev,plugins-dev,docs-dev}.md`
- `.claude/skills/build-and-test/SKILL.md`
- `.mcp.json`, `.coderabbit.yaml`

## Steps

1. `CLAUDE.md` follows the house section order and stays a router: always-needed
   rules and exact commands inline, everything deep behind a link.
2. One subagent per subsystem, named after the directory it owns — delegation is
   then unambiguous and the file sets are disjoint by construction.
3. Every agent file carries the shared-checkout rules: scope commands to the files
   you edited, concurrency 1, no git operations, never `git stash`.
4. `/feature` and `/planx` carry this repo's real commands. A command copied
   verbatim from another project hands the agent confidently wrong build steps.
5. `.mcp.json` names servers for what they reach and never inlines a secret.

## Done when

- A fresh agent can read `CLAUDE.md`, run `bin/setup`, run `bin/check`, and know
  which tier to run after an edit.
