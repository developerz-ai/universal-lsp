---
description: Take a feature from idea to merged and verified — explore, build, test, PR
argument-hint: [what to build, or an issue number]
allowed-tools: Read, Write, Edit, Glob, Grep, Bash, Task, Skill
---

# /feature

**Done means merged and verified.** A green local gate is not done; an open PR is
not done.

## Goal
$ARGUMENTS

## Steps

1. **Restate the goal in one line.** If two readings exist, say so before building.
2. **Distrust the paperwork.** If working from a plan or a doc, check it against
   the code and `git log` first — trackers rot in both directions. State plainly
   which claims you falsified and fix the doc in the same change.
3. **Explore in parallel**, read-only, into a ranked worklist of PR-sized batches.
4. **Test first.** Bug → reproducing test. Feature → the assertion that defines
   done. Register suites with `ulsp_add_test`, named `<subsystem>_*`.
5. **Build the smallest correct thing.** One primitive with its first real caller;
   no abstraction before a second consumer exists.
6. **Verify by tier**: `bin/check --files <paths>` after each edit,
   `bin/check --changed` when the slice compiles, `bin/check` once before pushing.
   Fail one, fix one — re-run only the failing command.
7. **PR**: `<type>/<slug>` branch, conventional commit, `Closes #N`, one concern,
   ~110 files max. Never push red; wait for checks to register.
8. **Merge queue depth 1.** Never two PRs in flight — each merge rebases `main`
   under the other and CI results go stale silently.

## Fan-out

One checkout. **No git worktrees** — work lands in a directory nobody is looking
at, and each one needs its own configure and build tree. Brief every agent with
its exclusive paths, concurrency 1, no git operations, never `git stash`.

## Stop for

An irreversible action, a change to a public contract (the LSP surface, MCP tool
names, `graph-lite.h`, the persistence format), or anything that would need a
guard disabled. Those are decisions, not steps.
