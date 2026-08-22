# Gotchas — symptom → cause → fix

A **diagnostic runbook**, read when something is already wrong. Preventive rules —
the ones that bite before any symptom — live in [`../CLAUDE.md`](../CLAUDE.md);
the split matters, or `CLAUDE.md` bloats into this file.

Add an entry the first time a symptom costs someone real time. Date it. Delete it
when it stops being true.

---

### `cmake --preset dev` fails with "Invalid extra field ... in Preset"

**Cause:** a field CMake's preset schema does not accept for that version —
`sourceDir` on a configure preset, for example, is not valid.
**Fix:** a separate source root needs its own `CMakePresets.json` (as
`graph-lite/` has), or an explicit `cmake -S <dir> -B <dir>` invocation. Check
your CMake version against `"version"` at the top of the presets file.
*(2026-08)*

---

### `ctest` reports "No tests were found"

**Cause:** the binary was never built, or a suite was registered with a bare
`add_test` instead of `ulsp_add_test`, so `gtest_discover_tests` never ran.
**Fix:** `bin/dev test` builds first, then runs. `bin/dev doctor` reports the
discovered test count as a row. Register suites with the helper.
*(2026-08)*

---

### clangd shows "file not found" for a header that plainly exists

**Cause:** no `compile_commands.json` at the repo root, or it points at a stale
build tree.
**Fix:** `bin/setup` symlinks it to `build/dev/`. `bin/dev doctor` has a row for
it.
*(2026-08)*

---

### A guard fails on a word inside a longer identifier — or fails to

**Cause:** `scripts/lint/graph-lite-vocabulary.ts` tokenises camelCase and
snake_case before matching, because `\bsymbol\b` does not match `SymbolNode` and
that is exactly the shape a leak takes. Consequence: `SymbolNode` is flagged,
`symbolic` is not.
**Fix:** rename to the generic equivalent. Do not add an allowlist entry — the
translation belongs in `analysis/`.
*(2026-08)*

---

### CI is green locally but red on a workflow change (or vice versa)

**Cause:** the gate is defined once in `scripts/lib/gate.ts`; CI runs the same
command strings. `scripts/ci-parity.test.ts` fails when they diverge.
**Fix:** change `gate.ts` and `ci.yml` in the same commit, then
`bun test scripts/`.
*(2026-08)*

---

### Every CI job sits queued forever with no logs and no runner

**Cause:** a runner label the repo is not enrolled for. An un-enrolled label does
not fail — no runner is ever assigned, so the required check is **absent** rather
than red, and nothing can merge on any branch.
**Fix:** `runs-on: ubuntu-latest`, which is what `ci-parity.test.ts` pins. Moving
off it means enrolling the repo and updating that assertion in the same commit.
*(2026-08)*
