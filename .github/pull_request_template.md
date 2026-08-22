## What

<!-- One sentence: the change. -->

## Why

<!-- The problem. Link the issue: Closes #N -->

## How

<!-- Only if non-obvious. Reference files, don't paste code. -->

## Checks

- [ ] `bin/check` is green
- [ ] Tests written with the code (bug → reproducing test first)
- [ ] No warning suppression, disabled guard, or skipped test added
- [ ] No new dependency on an external process, binary, or network service
- [ ] `graph-lite/` still depends on nothing in this repo, and names no consumer concept
- [ ] A defect that could recur ships its guard in this PR
- [ ] Public contract changed (LSP surface, MCP tool names, `graph-lite.h`, persistence format)? → an ADR is in this PR
