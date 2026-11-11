import { describe, expect, it } from 'bun:test';

import { fixture } from '../lib/fixture.ts';
import { run } from './graph-lite-exports.ts';

const CLEAN_HEADER = `
#define GRAPH_LITE_API
GRAPH_LITE_API void graph_lite_version(int* major, int* minor, int* patch);
GRAPH_LITE_API graph_lite_status graph_lite_graph_create(graph_lite_graph** out);
`;

describe('graph-lite-exports', () => {
  it('passes intentional C ABI declarations', async () => {
    const fx = await fixture({ 'graph-lite/include/graph-lite.h': CLEAN_HEADER });
    expect(await run(fx.root)).toEqual([]);
    fx.dispose();
  });

  it('flags an exported declaration outside the audited ABI allowlist', async () => {
    const fx = await fixture({
      'graph-lite/include/graph-lite.h':
        '#define GRAPH_LITE_API\nGRAPH_LITE_API void graph_lite_internal_reset(void);\n',
    });
    const findings = await run(fx.root);
    expect(findings).toHaveLength(1);
    expect(findings[0]?.file).toBe('graph-lite/include/graph-lite.h');
    expect(findings[0]?.message).toContain('graph_lite_internal_reset');
    expect(findings[0]?.message).toContain('allowlist');
    fx.dispose();
  });

  it('passes the audited real header', async () => {
    expect(await run()).toEqual([]);
  });
});
