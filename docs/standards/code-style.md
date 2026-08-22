# C++ conventions

What the linters do not encode. Formatting is `.clang-format`'s job and never a
decision you make; this is the rest.

## Structure

- **Files ≤ 500 lines.** One file, one job. Guard: `scripts/lint/max-file-loc.ts`.
- **One class or one cohesive free-function group per header.** If a header needs
  a table of contents, it is two headers.
- Headers under `<subsystem>/include/<subsystem>/`; private implementation under
  `<subsystem>/src/`. graph-lite is the exception by design: one public header at
  `graph-lite/include/graph-lite.h`.

## Types and errors

- **Custom error types, never a bare `throw std::runtime_error("...")`.** A caught
  type is self-documenting; a string is a grep.
- An error message names **the value, the seam, and the fix**. `"invalid offset"`
  costs the next debugger an hour; `"utf16 offset 14 lands inside a surrogate pair
  at line 3 (convert at the transport boundary, engine/text/offsets.h)"` does not.
- Make illegal states unrepresentable before validating them. A strong typedef for
  a byte offset versus a code-unit offset prevents the whole position-encoding bug
  class at compile time — which is cheaper than every test that would catch it.
- Exhaustive `switch` over enums, no `default:` that silently swallows a new case.

## Resources

- **RAII for everything with a lifecycle** — sessions, workspace state, worker
  pools, Lua interpreter states, graph handles. Construction acquires, destruction
  releases. This is also what makes test fixtures cheap.
- No naked `new`/`delete`. No owning raw pointers. `std::unique_ptr` by default,
  `shared_ptr` only when ownership is genuinely shared.
- Arena / `std::pmr` allocation is for measured hot paths, with a comment saying
  what was measured. Not a default, and not a vibe.

## Warnings

`-Werror` is on, with `-Wconversion` and `-Wsign-conversion` among others — those
two exist because every index in this project is integer maths on untrusted input.
**Never add a suppression to get green.** Fix the code, or the guard was right.

## Comments

- Comment the **why**, never the what. The code says what.
- A comment asserting a guarantee the code no longer provides is worse than no
  comment — when you change the mechanism, change the sentence.
- A comment saying "be careful here" is a bug report against the tooling. Make the
  machine careful instead: a type, an assertion at the seam, a test, a guard.
