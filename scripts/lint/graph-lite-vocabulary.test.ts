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

  it('skips a fenced code block in markdown — a third-party API name is not a leak', async () => {
    // Bun's FFI genuinely returns a `symbols` object. A guard that fires on
    // someone else's API name is a guard that gets disabled.
    const fx = await fixture({
      'docs/graph-lite/README.md':
        '# graph-lite\n\n```js\nconst { symbols } = dlopen(lib, {});\n```\n',
    });
    expect(await run(fx.root)).toEqual([]);
    fx.dispose();
  });

  it('still flags prose around a fenced block', async () => {
    const fx = await fixture({
      'docs/graph-lite/README.md': '```js\nok();\n```\n\nStores a symbol per node.\n',
    });
    expect(await run(fx.root)).toHaveLength(1);
    fx.dispose();
  });

  it('does NOT skip code in a source file — SymbolNode is the leak this exists to catch', async () => {
    const fx = await fixture({
      'graph-lite/include/graph-lite.h': 'struct SymbolNode { int id; };\n',
    });
    expect(await run(fx.root)).toHaveLength(1);
    fx.dispose();
  });

  it('flags a banned word in the C ABI test — the scan covers .c, not only .cpp/.h', async () => {
    // c_abi_test.c ships with the library tests and SCANNED promises
    // tests/graph-lite/** — a glob without the .c extension made the file
    // invisible to the guard while the docstring still claimed coverage.
    const fx = await fixture({
      'tests/graph-lite/c_abi_test.c':
        '#include "graph-lite.h"\n\nstatic graph_lite_store* workspace_store = 0;\n',
    });
    const findings = await run(fx.root);
    expect(findings).toHaveLength(1);
    expect(findings[0]?.file).toBe('tests/graph-lite/c_abi_test.c');
    expect(findings[0]?.message).toContain('consumer concept');
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

  it('does not fire on names inherited from Object.prototype', async () => {
    // `constructor`, `toString`, … are inherited members of the plain lookup
    // object: the unguarded lookup returned Object.prototype.constructor —
    // truthy for every token — so generic C++ words fired with a mangled
    // message (`"constructor" is function Object() { [native code] }`).
    // Line 2 is the positive control: the file must still be scanned, and the
    // single remaining finding must be the real violation.
    const fx = await fixture({
      'graph-lite/include/graph-lite.h':
        '// Built in the constructor; toString, valueOf and hasOwnProperty are generic.\n' +
        '// workspace is a consumer concept.\n',
    });
    const findings = await run(fx.root);
    expect(findings).toHaveLength(1);
    expect(findings[0]?.line).toBe(2);
    expect(findings[0]?.message).toContain('consumer concept');
    fx.dispose();
  });

  it('passes the real tree', async () => {
    expect(await run()).toEqual([]);
  });
});
