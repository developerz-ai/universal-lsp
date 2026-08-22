#!/usr/bin/env bun
/** Print the affected-subsystem selection as JSON. CI's `detect` job and the
 *  local `--changed` tier call the SAME computeAffected(), so they cannot drift. */
import { appendFileSync } from 'node:fs';

import { computeAffected } from './lib/affected.ts';
import { changedFiles } from './lib/git.ts';

const baseRef = process.env.BASE_REF || 'origin/main';
const files = await changedFiles(baseRef);
const affected = computeAffected(files);

console.log(JSON.stringify({ ...affected, changedFiles: files.length }, null, 2));

// GitHub Actions step outputs, when running in CI. APPEND — $GITHUB_OUTPUT
// accumulates across every step in the job; writing truncates the ones before.
if (process.env.GITHUB_OUTPUT) {
  appendFileSync(
    process.env.GITHUB_OUTPUT,
    `build_needed=${affected.buildNeeded}\nctest_filter=${affected.ctestFilter}\nsubsystems=${affected.subsystems.join(',')}\n`,
  );
}
