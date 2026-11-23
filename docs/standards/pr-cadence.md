# PR cadence: batch work into at most five PRs per day

Fleet-read rule. Applies to every agent authoring PRs in this repo.

## The rule

| Constraint | Value |
| ---------- | ----- |
| Owner-attributed PRs per day, median over 30 days | at most 5 |
| Merge queue depth | 1 |
| Tracking issues | one per PR batch, not one per slice |

Group related work by area into one PR before opening it. A plan with many slices
produces at most five PRs per day, with several compatible slices per PR. Ship a
fix, its reproducing test, its guard and its doc in the same PR. Landing a batch
still means one merge at a time: queue depth 1 is unchanged.

Derive the budget from a pure policy, not from a per-agent judgement call.
`scripts/lib/pr-cadence.ts` exports `TARGET_PRS_PER_DAY = 5` and `batchIntoPRs`;
`scripts/lib/pr-cadence.test.ts` pins the fragmentation case and the batched case.
Run `bun test scripts/lib/pr-cadence.test.ts` to check the policy.

## Finding: why the median read 95

As of 2026-11-23, `owner_prs_per_day_p50` read 95 against a target of 5, lower is
better, noise floor 19, 30-day window. No PR-creation code exists in this checkout
(`gh pr create`, `pulls.create` and `createPull` return no hits), and the census is
platform-side (`census-corpus.ts` and `census-metrics.ts` are not in this tree).
The driver is the guidance agents read: it maps each unit of work to its own PR.

| Path and line | Excerpt | Effect |
| ------------- | ------- | ------ |
| `.claude/commands/feature.md:23` | `ranked worklist of PR-sized batches` | plans are cut to fit one PR each |
| `.claude/commands/feature.md:31` | `Closes #N`, one concern | one issue per tiny PR |
| `.claude/commands/planx.md:31-32` | `the file set is the PR` | each plan slice becomes a PR |
| `CONTRIBUTING.md:63-65` | `One concern per PR` and `Closes #N in the body` | fragmentation is the public rule too |
| `.dz/schedules/improve.md:16` | `One small PR per improvement` | monthly work is split per row |
| `CLAUDE.md:218` | `Plan slice → issue → PR is one continuous line` | the root contract repeats it |

Ninety-five work units opened one per PR give a median of 95 PRs per day. The same
units grouped by area give at most five PRs per day in the same window. That is the
cause this standard and the cadence policy fix.

## Applying the rule

| Situation | Do | Do not |
| --------- | -- | ------ |
| A plan has N slices | group slices with one owner into a PR batch | open one PR per slice |
| Several improvements pass the gate | batch them behind one tracking issue | open one PR per improvement |
| A bug needs a guard and a doc | land all three in one PR | split the guard into a follow-up |
| The batch exceeds ~110 files | split by area, still within the daily budget | keep splitting until files are tiny |

Protected inputs: root `CLAUDE.md` and `.dz/schedules/**` are the grader and out of
scope this run. Their one-PR-per-slice wording above still conflicts with this
rule. Reconcile them before treating the cadence as fully enforced. Meanwhile this
standard governs PR planning in the workflow docs and in agent-authored changes.

## Verification

- `bun test scripts/lib/pr-cadence.test.ts`: fails against one PR per work unit,
  passes with batching under the budget.
- `bun run scripts/lint/index.ts`: relative links in this doc must resolve.
- `docs/standards/testing.md`: TDD and guard-testing conventions.
- `docs/standards/dev-loop.md`: which check tier to run when.
