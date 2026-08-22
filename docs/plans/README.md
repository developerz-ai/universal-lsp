# Plans

Dated, self-contained execution plans — the output of `/planx`. A plan is written
by one agent and executed by another with no shared context, so everything the
executor needs is on disk.

```
docs/plans/<YYYY>/<MM>/<DD>/<1NN>-<slug>/
├── overview.md          the map: goal, context, slices in order, done-when, risks
├── 01-<aspect>.md       one separable area of work, independently executable
├── 02-<aspect>.md
└── status.yml           the ONLY tracker — machine-readable, enums fixed
```

## Rules

- `1NN` auto-increments per day from `101`. Slug is kebab-case, ≤ 5 words.
- **Never one `plan.md`.** Multiple files, split by *area of work* — because the
  slice boundary is the file-set boundary an executor locks on, and the file set
  is the PR. Split by chapter instead and you invent boundaries the code does not
  have.
- **Reference, never paste.** `path:line` and `Class::method`. Pasted code is
  stale on arrival.
- **No checkboxes in the `.md` slices.** They are reference maps. `status.yml` is
  the one tracker.
- `created_by` ≠ `worked_by` — that is what lets one agent plan and another
  execute. `worked_by: ""` means unclaimed.
- **Distrust the paperwork.** Before working off a plan, check it against the code
  and `git log`. Trackers rot in both directions: a plan that says `not_started`
  over eight merged PRs costs you a re-implementation; one that records a decision
  already superseded costs you a "fix" to working code. State plainly which claims
  you falsified, and update the plan in the same change.

The plan maps 1:1 onto GitHub: the plan dir is a milestone, each slice is an
issue, `status.yml` `evidence` holds the PRs that closed them.

| Plan | What |
|---|---|
| [2026/08/22/101-repo-foundation](2026/08/22/101-repo-foundation/overview.md) | Repository, gates, CI and docs scaffolding (this one is complete) |
