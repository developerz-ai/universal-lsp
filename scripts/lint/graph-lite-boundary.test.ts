import { describe, expect, it } from 'bun:test';

import { fixture } from '../lib/fixture.ts';
import { run } from './graph-lite-boundary.ts';

const CLEAN_CMAKE = 'add_library(graph-lite src/version.cpp)\n';

describe('graph-lite-boundary', () => {
  it('flags an include that reverses the dependency direction', async () => {
    const fx = await fixture({
      'graph-lite/src/store.cpp': '#include "engine/session.h"\n',
      'graph-lite/CMakeLists.txt': CLEAN_CMAKE,
    });
    const findings = await run(fx.root);
    expect(findings).toHaveLength(1);
    expect(findings[0]?.message).toContain('one');
    fx.dispose();
  });

  it('flags a link against a repo target, where it would otherwise compile fine', async () => {
    const fx = await fixture({
      'graph-lite/CMakeLists.txt': 'target_link_libraries(graph-lite PUBLIC ulsp::engine)\n',
    });
    expect(await run(fx.root)).toHaveLength(1);
    fx.dispose();
  });

  it('does not flag the rule stated in a comment', async () => {
    const fx = await fixture({
      'graph-lite/CMakeLists.txt': `# Never link ulsp::engine here.\n${CLEAN_CMAKE}`,
    });
    expect(await run(fx.root)).toEqual([]);
    fx.dispose();
  });

  it('passes the real tree', async () => {
    expect(await run()).toEqual([]);
  });
});
