import { describe, expect, it } from 'bun:test';

import { GATE_STEPS, stepCommand } from './lib/gate.ts';
import { REPO_ROOT } from './lib/git.ts';

/**
 * Drift between the local gate and CI is invisible until it costs a day, so
 * assert it. A check that exists only in CI makes agents push red repeatedly and
 * blame the flake; a check that exists only locally means CI is not a gate.
 */
const ci = await Bun.file(`${REPO_ROOT}/.github/workflows/ci.yml`).text();

describe('CI parity', () => {
  it('CI runs every command bin/check runs', () => {
    const missing = GATE_STEPS.filter((s) => !ci.includes(stepCommand(s))).map(stepCommand);
    expect(missing).toEqual([]);
  });

  it('CI pins runners deliberately', () => {
    // Changing where CI runs means changing this assertion in the same commit,
    // with a human deciding. A "temporary" runner switch nobody reverses is how
    // a repo's required check goes absent rather than red.
    const runners = [...ci.matchAll(/runs-on:\s*(\S+)/g)].map((m) => m[1]);
    expect(runners.length).toBeGreaterThan(0);
    expect([...new Set(runners)]).toEqual(['ubuntu-latest']);
  });

  it('every CI job carries a timeout — a hung job holds the merge and teaches nothing', () => {
    // Scan the jobs block only: `on:` also has two-space keys, and counting
    // those made this assertion pass or fail for the wrong reason.
    const jobsBlock = ci.slice(ci.indexOf('\njobs:\n'));
    const jobs = [...jobsBlock.matchAll(/^ {2}([a-z][a-z0-9-]*):$/gm)].map((m) => m[1]);
    const timeouts = [...jobsBlock.matchAll(/^ {4}timeout-minutes:/gm)];

    expect(jobs.length).toBeGreaterThan(0);
    expect(timeouts.length).toBe(jobs.length);
  });

  it('the aggregate job needs every other job, so a new job cannot be forgotten', () => {
    const jobsBlock = ci.slice(ci.indexOf('\njobs:\n'));
    const jobs = [...jobsBlock.matchAll(/^ {2}([a-z][a-z0-9-]*):$/gm)].map((m) => m[1]);
    const aggregate = jobsBlock.match(/^ {4}needs: \[([^\]]+)\]/m)?.[1] ?? '';
    const needed = aggregate.split(',').map((s) => s.trim());

    // Every job except the aggregate itself must be in its needs list.
    expect(jobs.filter((j) => j !== 'ci' && !needed.includes(j))).toEqual([]);
  });
});
