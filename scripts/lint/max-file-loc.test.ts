import { describe, expect, it } from 'bun:test';

import { fixture } from '../lib/fixture.ts';
import { run } from './max-file-loc.ts';

describe('max-file-loc', () => {
  it('flags a file over the limit and names the count', async () => {
    const fx = await fixture({ 'engine/src/huge.cpp': 'int x;\n'.repeat(600) });
    const findings = await run(fx.root);
    expect(findings).toHaveLength(1);
    expect(findings[0]?.message).toContain('601 lines');
    fx.dispose();
  });

  it('passes a file at the limit', async () => {
    const fx = await fixture({ 'engine/src/ok.cpp': 'int x;\n'.repeat(400) });
    expect(await run(fx.root)).toEqual([]);
    fx.dispose();
  });

  it('passes the real tree', async () => {
    expect(await run()).toEqual([]);
  });
});
