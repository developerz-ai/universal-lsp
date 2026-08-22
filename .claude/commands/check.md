---
description: Run the right gate tier, report concisely, fix what fails
allowed-tools: Bash, Read, Edit
---

# /check

1. `bin/check --changed`
2. If it passes and you are about to push: `bin/check` (background if slow).

Stop at the first failure, fix it, re-run **only** that command.

Report one line per step: `✓ <label>` or `✗ <label> — <error summary>`.

Fix rules:
- Compiler warning → fix the code. **Never** add a suppression; `-Werror` is on
  deliberately.
- Guard violation → the message names the fix. Never an allowlist entry.
- Format → `bin/fmt`.
- Test failure → read the assertion before touching anything. Do not "fix" a test
  to match new behaviour unless the new behaviour is what was asked for.
