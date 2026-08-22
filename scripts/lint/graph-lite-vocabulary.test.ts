import { describe, expect, it } from 'bun:test';

import { fixture } from '../lib/fixture.ts';
import { run } from './graph-lite-vocabulary.ts';

describe('graph-lite-vocabulary', () => {
  it('flags a consumer concept in the public header', async () => {
    const fx = await fixture({
      'graph-lite/include/graph-lite.h': 'struct SymbolNode { int id; };\n',
    });
    const findings = await run(fx.root);
    expect(findings).toHaveLength(1);
    expect(findings[0]?.file).toBe('graph-lite/include/graph-lite.h');
    expect(findings[0]?.message).toContain('analysis/');
    fx.dispose();
  });

  it('flags leakage in the library docs, which ship as if standalone', async () => {
    const fx = await fixture({
      'docs/graph-lite/data-model.md': 'Used by the Universal LSP for call graphs.\n',
    });
    expect((await run(fx.root)).length).toBeGreaterThan(0);
    fx.dispose();
  });

  it('passes clean generic code', async () => {
    const fx = await fixture({
      'graph-lite/include/graph-lite.h':
        'struct Node { int id; };\nstruct Edge { int from, to; };\n',
      'graph-lite/README.md': '# graph-lite\n\nTyped nodes, typed directed edges, properties.\n',
    });
    expect(await run(fx.root)).toEqual([]);
    fx.dispose();
  });

  it('passes the real tree', async () => {
    expect(await run()).toEqual([]);
  });
});
