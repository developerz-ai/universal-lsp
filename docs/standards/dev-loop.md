# The dev loop — which check to run when

Two different questions wear the word "check", and answering the inner-loop one
with the outer-loop command is the largest avoidable waste of wall clock here —
an agent pays it on **every edit**.

| Question | Command | Cost |
|---|---|---|
| did my last edit format + pass the guards? | `bin/check --files <paths>` | < 3s |
| do the subsystems I touched still build and pass? | `bin/check --changed` | < 30s |
| is the repo green? | `bin/check` | ~1 min |

`bin/check` configures three build trees and runs the suite twice, once under ASan
+ UBSan. Run it **once**, before pushing — in the background if that is slow.

## How "changed" is computed

`scripts/lib/affected.ts` is a pure function of the changed-file list: no git, no
fs, no process. One implementation, three consumers — the local `--changed` tier,
`bin/dev build`, and CI's `detect` job. If local scoping and CI scoping drift,
"green locally" stops meaning anything, so they read the same function rather than
two descriptions of the same rule.

It maps files to owning subsystems, then fans out through the **one-way**
dependency graph: a `graph-lite/` change re-verifies engine, analysis, daemon and
cli; an `engine/` change never re-verifies graph-lite. A root or build-config
change (`CMakeLists.txt`, `cmake/`, `scripts/`, `bin/`, workflows) widens to
everything.

Locally it counts uncommitted **and untracked** files — an agent works uncommitted
by definition, and a detector that only sees commits reports "nothing changed" for
exactly the state being asked about.

## Blind spots, stated on purpose

`--changed` is scoped. It will not show:

- a break in a subsystem the diff did not touch or fan out to;
- anything only the sanitizer build catches;
- anything only an integration test covers.

That is an acceptable trade **because** the full gate runs before push. Undocumented
it would be a trap.

## Fail one, fix one

Run the tiers in order, stop at the first failure, fix, re-run **only that
command**. Chaining with `&&` and re-running the chain wastes the fast tiers on a
known-failing state. `bin/check` prints the exact re-run command for the step that
failed.

Never trust a piped exit code — `cmd | tail; echo $?` reports `tail`'s status. Run
the tool directly, or check `${PIPESTATUS[0]}`.
