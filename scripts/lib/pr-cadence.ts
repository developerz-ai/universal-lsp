/**
 * PR cadence policy. Pure data in, groups out: no git, no fs, no process.
 * The single source for the daily budget cited by docs/standards/pr-cadence.md.
 */

/** Owner-attributed PRs per day (median over 30 days), lower is better. */
export const TARGET_PRS_PER_DAY = 5;

/** One separable piece of work, as produced by a plan slice or a gate failure. */
export interface WorkUnit {
  slice: string;
  area: string;
}

/**
 * Group work by area, first-seen order, into at most TARGET_PRS_PER_DAY PRs.
 * Extra areas are folded into the last group: the daily budget outranks a
 * perfect one-area-per-PR mapping. Every input unit appears in exactly one
 * group, in input order within its group.
 */
export function batchIntoPRs(units: WorkUnit[]): WorkUnit[][] {
  const groups = new Map<string, WorkUnit[]>();
  for (const unit of units) {
    const group = groups.get(unit.area);
    if (group) group.push(unit);
    else groups.set(unit.area, [unit]);
  }

  if (groups.size <= TARGET_PRS_PER_DAY) return [...groups.values()];

  const all = [...groups.values()];
  const batched = all.slice(0, TARGET_PRS_PER_DAY - 1);
  batched.push(all.slice(TARGET_PRS_PER_DAY - 1).flat());
  return batched;
}

/** True when a day's PR list respects the budget. */
export function withinBudget(prs: unknown[][]): boolean {
  return prs.length <= TARGET_PRS_PER_DAY;
}
