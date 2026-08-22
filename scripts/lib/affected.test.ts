import { describe, expect, it } from 'bun:test';

import { computeAffected } from './affected.ts';

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

  it('emits a CTest filter that selects only the affected suites', () => {
    expect(computeAffected(['plugins/lua/scopes.lua']).ctestFilter).toBe('^(plugins_)');
  });
});
