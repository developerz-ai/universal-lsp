# Testing

Tests are how an agent verifies its own work. This project has no UI to eyeball
and no staging to poke — a test is the only thing that says "done".

**TDD is the working method**, not an aspiration: failing test, then code.

## The pyramid

| Kind | Hits | Speed target | Where |
|---|---|---|---|
| Unit | pure logic, no I/O | < 3s per file, < 10s suite | `tests/<subsystem>/` |
| Property | generated inputs, asserted invariants | as needed | same |
| Fuzz | malformed/adversarial input | runs long, separately | persistence, protocol decode |
| Golden-file | a plugin's output vs a checked-in expectation | fast | `tests/plugins/<language>/` |
| Integration | the daemon end to end over a real socket | minutes OK | `tests/engine/`, CTest label `integration` |

## Registering a suite

Use the `ulsp_add_test` helper in `tests/CMakeLists.txt` — never `add_executable`
plus a hand-written `add_test`. A suite registered by hand is a suite that
eventually stops being run and nobody notices, because green is green.

```cmake
ulsp_add_test(graph_lite_traversal_test
    SOURCES traversal_test.cpp
    LINK    graph-lite::graph-lite)
```

The name prefix matters: `scripts/lib/affected.ts` maps a subsystem to a CTest
regex by prefix, so `graph_lite_*` and `engine_*` are how the scoped tier finds
your test.

## Sanitizers are part of the suite, not a special occasion

`bin/dev asan` (and the `asan` CI job) run everything under ASan + UBSan. This is
wired in from the first commit because the hot paths are arena/`std::pmr`
allocation and CSR indices, where a use-after-free reads as a *wrong answer*
rather than a crash — the exact bug class that survives a green test run.

## Rules

- **Never `sleep()` in a test.** Inject the clock and drive it. A real wait tests
  the host's scheduler; under load it coalesces and the suite goes red for
  nothing, and then someone "fixes" it with a looser assertion that no longer
  means anything. If a test needs `sleep`, the code is missing a seam.
- **Fake the outside, run the inside for real.** graph-lite's own storage in a
  graph-lite test: real. A filesystem watcher's kernel events: injected. Fakes,
  not mocks — a small real implementation beats a pile of call-shape assertions.
- **Embarrassingly parallel.** No shared files, no fixed ports, no global
  singletons, no order dependencies. A test that only passes when it runs first is
  a bug in the test.
- **Assert observable state, never "it eventually settles".** A component
  configured to retry forever never fails; the test just hangs until the runner's
  timeout.
- **The guards have their own tests.** Every `scripts/lint/<rule>.ts` ships a
  `<rule>.test.ts` asserting it flags a bad sample *and* passes a good one. A
  guard that silently matches nothing is worse than none — it looks green.

## Commands

```bash
bin/dev test                    # build + whole suite
bin/dev test Position           # only tests matching a regex
bin/dev asan                    # the same suite under ASan + UBSan
bun test scripts/               # the repo tooling's own tests
```
