import { describe, expect, it } from 'bun:test';

import { computeAffected, UNKNOWN_BASE } from './affected.ts';

describe('computeAffected', () => {
  it('fans out through the one-way dependency graph', () => {
    // graph-lite is the base of the stack: touching it re-verifies its consumers.
    const a = computeAffected(['graph-lite/src/storage.cpp']);
    expect(a.subsystems).toEqual(['graph-lite', 'engine', 'analysis', 'daemon', 'cli']);
  });

  it('does not walk the dependency graph backwards', () => {
    // The whole point of the boundary: an engine change can never affect graph-lite.
    const a = computeAffected(['engine/src/transport.cpp']);
    expect(a.subsystems).not.toContain('graph-lite');
    expect(a.subsystems).toEqual(['engine', 'analysis', 'daemon', 'cli']);
  });

  it('treats a graph-lite test change as a graph-lite change', () => {
    expect(computeAffected(['tests/graph-lite/traversal_test.cpp']).subsystems).toContain(
      'graph-lite',
    );
  });

  it('widens to everything on a build-config change', () => {
    const a = computeAffected(['cmake/Sanitizers.cmake']);
    expect(a.fullGate).toBe(true);
    expect(a.subsystems).toHaveLength(5 + 1);
  });

  it('skips the build for a docs-only change but still reports it', () => {
    const a = computeAffected(['docs/spec.md', 'README.md']);
    expect(a.buildNeeded).toBe(false);
    expect(a.ctestFilter).toBe('');
  });

  it('widens to everything when the base ref could not be resolved', () => {
    // A first push sends an all-zero `before` ref. The detector must fail CLOSED:
    // an empty list reads as "nothing changed" downstream, the build is skipped,
    // and CI reports green over a build it never ran. Nothing errors — which is
    // why this is asserted rather than trusted. (Caught on the very first CI run.)
    const a = computeAffected([UNKNOWN_BASE]);
    expect(a.fullGate).toBe(true);
    expect(a.buildNeeded).toBe(true);
  });

  it('never reports an empty selection as "nothing to do" for an unknown diff', () => {
    expect(computeAffected([UNKNOWN_BASE, 'docs/spec.md']).buildNeeded).toBe(true);
  });

  it('emits a CTest filter that selects only the affected suites', () => {
    expect(computeAffected(['plugins/lua/scopes.lua']).ctestFilter).toBe('^(plugins_)');
  });
});
