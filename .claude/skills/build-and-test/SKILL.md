---
name: build-and-test
description: Build, run and debug this C++ project — CMake presets, CTest, sanitizers, clang-tidy, and which gate tier to run. Triggers: build, compile, test, ctest, cmake, sanitizer, asan, segfault, linker error.
allowed-tools: [Bash, Read, Edit]
---

# Build and test

## Which command

| Question | Command |
|---|---|
| did my edit format + pass the guards? | `bin/check --files <paths>` |
| do the subsystems I touched still pass? | `bin/check --changed` |
| is the repo green? (once, before push) | `bin/check` |
| build only | `bin/dev build [dev\|asan\|release]` |
| one test | `bin/dev test <regex>` |
| memory bugs | `bin/dev asan` |
| environment wrong? | `bin/dev doctor` |

Never hand-roll a `cmake` invocation a preset covers. Never trust a piped exit
code — `cmd \| tail; echo $?` reports `tail`'s status.

## Failure → fix

| Symptom | Do |
|---|---|
| `No tests were found` | build first (`bin/dev test` does), and register via `ulsp_add_test`, not bare `add_test` |
| clangd cannot find a header | `bin/setup` symlinks `compile_commands.json` |
| a warning fails the build | fix the code. `-Werror` is deliberate; **never** add a suppression |
| ASan reports a use-after-free | read the allocation trace, not just the free — arena/pmr lifetimes are the usual cause |
| a guard fires | the message names the fix. Never an allowlist entry |
| green locally, red in CI | the parity bug: fix `scripts/lib/gate.ts` and `.github/workflows/ci.yml` together, then `bun test scripts/` |

## Adding a suite

```cmake
ulsp_add_test(engine_offsets_test
    SOURCES offsets_test.cpp
    LINK    ulsp::engine)
```

Name it `<subsystem>_*` — `scripts/lib/affected.ts` maps subsystem to CTest regex
by prefix, so a differently-named suite silently stops being selected by the
scoped tier.

Details: `docs/standards/testing.md`, `docs/standards/dev-loop.md`.
