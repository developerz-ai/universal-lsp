# .claude/

Agent configuration for this repo. Everything here is read by an agent, so every
file pays its token cost on activation — keep them short.

| Path | Is | Triggered by |
|---|---|---|
| `settings.json` | permissions + a post-edit clang-format hook | the harness |
| `commands/*.md` | canned workflows | `/name` typed by the user |
| `agents/*.md` | subsystem specialists | delegation from the main agent |
| `skills/*/SKILL.md` | domain expertise packages | the agent matching the description |

## Budgets

`SKILL.md` < 80 lines · agent < 50 · command < 30 (`/planx` and `/feature` are the
justified exceptions). Compress prose; never compress a command, path, or version.

## The roster

One subagent per subsystem, named after the directory it owns. Delegation is then
unambiguous — a path maps to exactly one owner — and the file sets are disjoint by
construction, which is what makes parallel work safe in one checkout.

| Agent | Owns |
|---|---|
| `architect` | should this be built, and in what shape |
| `graph-lite-dev` | `graph-lite/`, `tests/graph-lite/` |
| `engine-dev` | `engine/`, `daemon/`, `cli/`, `tests/engine/` |
| `analysis-dev` | `analysis/` |
| `plugins-dev` | `plugins/`, `tests/plugins/` |
| `docs-dev` | `docs/`, `README.md` |

## Rules every agent here inherits

- **One checkout, no worktrees.** Work lands where everyone is looking.
- **Scope every command to the files you edited.** Never a repo-wide gate, never
  a working-tree-diffing `--changed` when others are working — in a shared tree
  that expands to nearly the whole repo, N times over.
- **Concurrency 1.** Saturating the box is the coordinator's job, once, at the end.
- **No git operations.** Leave work uncommitted; the coordinator owns git. Never
  `git stash` — one global stack, shared.
