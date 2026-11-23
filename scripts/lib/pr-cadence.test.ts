import { describe, expect, it } from 'bun:test';

import { batchIntoPRs, TARGET_PRS_PER_DAY, withinBudget } from './pr-cadence.ts';

/** The measured fleet read: 95 work units opened as 95 PRs in one day. */
const units = Array.from({ length: 95 }, (_, i) => ({
  slice: `S${i + 1}`,
  area: i % 10 === 0 ? 'engine' : i % 2 === 0 ? 'graph-lite' : 'docs',
}));

describe('PR cadence policy', () => {
  it('fails a day of one PR per work unit', () => {
    const onePerUnit = units.map((u) => [u]);
    expect(onePerUnit).toHaveLength(95);
    expect(withinBudget(onePerUnit)).toBe(false);
  });

  it('groups the same 95 units by area into at most five PRs', () => {
    const batched = batchIntoPRs(units);
    expect(batched.length).toBeLessThanOrEqual(TARGET_PRS_PER_DAY);
    expect(withinBudget(batched)).toBe(true);
    expect(batched.flat()).toHaveLength(units.length);
  });

  it('keeps a slice in exactly one area group, in input order within the group', () => {
    const batched = batchIntoPRs(units);
    expect(new Set(batched.flat().map((u) => u.slice)).size).toBe(units.length);
    for (const group of batched) {
      expect(group).toEqual(units.filter((u) => group.includes(u)));
    }
  });

  it('preserves input order when interleaved areas are folded into the last group', () => {
    const interleaved = ['a', 'b', 'c', 'd', 'e', 'f', 'e', 'f'].map((area, i) => ({
      slice: `S${i + 1}`,
      area,
    }));
    const batched = batchIntoPRs(interleaved);
    expect(batched).toHaveLength(TARGET_PRS_PER_DAY);
    expect(batched.slice(0, 4)).toEqual(interleaved.slice(0, 4).map((unit) => [unit]));
    expect(batched[4]).toEqual(interleaved.slice(4));
  });
});
