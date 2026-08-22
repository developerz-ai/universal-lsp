#!/usr/bin/env bun
/**
 * The gate. Three tiers — run the tightest one that answers your question:
 *
 *   bin/check --files <paths>   after each edit      format + guards on those files
 *   bin/check --changed         after a slice        the affected subsystems only
 *   bin/check                   once, before push    everything, sanitizers included
 *
 * Do NOT reflexively run the full gate: it configures three build trees and runs
 * the suite twice, once under ASan + UBSan. Run it once, before pushing.
 *
 * Fail one, fix one: the run stops at the first failing step. Re-run only that
 * command after the fix — re-running the chain wastes the fast tiers on a
 * known-failing state.
 */
import { computeAffected } from './lib/affected.ts';
import { run } from './lib/cmd.ts';
import { GATE_STEPS, type GateStep, stepCommand } from './lib/gate.ts';
import { changedFiles, REPO_ROOT } from './lib/git.ts';
import { dim, fail, ok, step } from './lib/log.ts';

const argv = process.argv.slice(2);
const fileIndex = argv.indexOf('--files');
const files = fileIndex === -1 ? [] : argv.slice(fileIndex + 1).filter((a) => !a.startsWith('--'));
const scoped = argv.includes('--changed');
const fast = argv.includes('--fast') || scoped || files.length > 0;

let steps: GateStep[] = GATE_STEPS.filter((s) => (fast ? s.scope !== 'full' : true));
let selection = 'full gate';

if (files.length > 0) {
  const affected = computeAffected(files);
  selection = `files: ${files.join(' ')} → ${affected.subsystems.join(', ') || 'no build'}`;
  if (!affected.buildNeeded) steps = steps.filter((s) => s.scope === 'always');
} else if (scoped) {
  const changed = await changedFiles();
  const affected = computeAffected(changed);
  selection = `changed: ${changed.length} file(s) → ${affected.subsystems.join(', ') || 'no build'}`;
  if (!affected.buildNeeded) steps = steps.filter((s) => s.scope === 'always');
}

step(`${selection} ${dim(`${steps.length} steps`)}`);

const started = Date.now();
for (const s of steps) {
  const at = Date.now();
  const result = await run(s.argv, REPO_ROOT);
  const secs = ((Date.now() - at) / 1000).toFixed(1);

  if (result.code !== 0) {
    fail(`${s.label} ${dim(`${secs}s`)}`);
    console.error(result.stdout.trimEnd());
    console.error(result.stderr.trimEnd());
    console.error(`\nRe-run just this step: ${stepCommand(s)}`);
    process.exit(result.code);
  }
  ok(`${s.label} ${dim(`${secs}s`)}`);
}

ok(`gate green ${dim(`${((Date.now() - started) / 1000).toFixed(1)}s`)}`);
