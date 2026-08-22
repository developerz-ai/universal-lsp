import { describe, expect, it } from 'bun:test';

import { fixture } from '../lib/fixture.ts';
import { run } from './doc-links.ts';

describe('doc-links', () => {
  it('flags a link to a file that does not exist', async () => {
    const fx = await fixture({ 'docs/README.md': 'See [spec](spec.md).\n' });
    const findings = await run(fx.root);
    expect(findings).toHaveLength(1);
    expect(findings[0]?.message).toContain('dead link');
    fx.dispose();
  });

  it('resolves a relative link that climbs out of its directory', async () => {
    const fx = await fixture({
      'docs/engine/README.md': 'See [spec](../spec.md).\n',
      'docs/spec.md': '# Spec\n',
    });
    expect(await run(fx.root)).toEqual([]);
    fx.dispose();
  });

  it('ignores external links and bare anchors', async () => {
    const fx = await fixture({
      'README.md': '[site](https://example.com) [top](#heading) [mail](mailto:a@b.c)\n',
    });
    expect(await run(fx.root)).toEqual([]);
    fx.dispose();
  });

  it('strips an anchor before resolving', async () => {
    const fx = await fixture({
      'a.md': 'See [x](b.md#section).\n',
      'b.md': '# B\n',
    });
    expect(await run(fx.root)).toEqual([]);
    fx.dispose();
  });

  it('passes the real tree', async () => {
    expect(await run()).toEqual([]);
  });
});
