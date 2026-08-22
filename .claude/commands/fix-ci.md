---
description: Pull the failing CI job, reproduce it locally with the scoped command, fix, push
allowed-tools: Bash, Read, Edit
---

# /fix-ci

1. `gh run list --branch $(git branch --show-current) --limit 3`
2. `gh run view <id> --log-failed` — read the actual error, not the summary.
3. Map the job to its local command (they are the same strings; the mapping lives
   in `scripts/lib/gate.ts`):

   | Job | Local |
   |---|---|
   | Format + guards | `bin/check --files <paths>` |
   | Build + test | `bin/dev test` |
   | graph-lite standalone | `cmake -S graph-lite -B build/graph-lite-standalone -G Ninja && cmake --build build/graph-lite-standalone` |
   | ASan + UBSan | `bin/dev asan` |
   | clang-tidy | `clang-tidy -p build/dev <file>` |

4. Reproduce **locally first**. A fix pushed without reproducing is a guess with a
   five-minute feedback loop.
5. Fix, `bin/check --changed`, push.

If the job failed because CI and the local gate disagree, that is the parity bug —
fix `scripts/lib/gate.ts` and `.github/workflows/ci.yml` together, and
`bun test scripts/` proves it.

**Flaky is not acceptable.** A flaky job trains everyone to re-run instead of
read. Open an issue and quarantine it; never `continue-on-error`.
