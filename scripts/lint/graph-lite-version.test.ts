import { describe, expect, it } from 'bun:test';

import { fixture } from '../lib/fixture.ts';
import { run } from './graph-lite-version.ts';

const headerFor = (version: string, str = version) =>
  [
    `#define GRAPH_LITE_VERSION_MAJOR ${version.split('.')[0]}`,
    `#define GRAPH_LITE_VERSION_MINOR ${version.split('.')[1]}`,
    `#define GRAPH_LITE_VERSION_PATCH ${version.split('.')[2]}`,
    `#define GRAPH_LITE_VERSION_STRING "${str}"`,
    '',
  ].join('\n');

const cmakeFor = (project: string, set: string) =>
  [
    `project(graph-lite VERSION ${project} LANGUAGES CXX)`,
    `set(GRAPH_LITE_VERSION ${set})`,
    '',
  ].join('\n');

describe('graph-lite-version', () => {
  it('passes statements that agree', async () => {
    const fx = await fixture({
      'graph-lite/include/graph-lite.h': headerFor('0.1.0'),
      'graph-lite/CMakeLists.txt': cmakeFor('0.1.0', '0.1.0'),
    });
    expect(await run(fx.root)).toEqual([]);
    fx.dispose();
  });

  it('flags the CMake statements that lag the header, by file and line', async () => {
    const fx = await fixture({
      'graph-lite/include/graph-lite.h': headerFor('0.2.0'),
      'graph-lite/CMakeLists.txt': cmakeFor('0.1.0', '0.1.0'),
    });
    const findings = await run(fx.root);
    expect(findings).toHaveLength(2);
    expect(findings[0]?.file).toBe('graph-lite/CMakeLists.txt');
    expect(findings[0]?.line).toBe(1);
    expect(findings[0]?.message).toContain('project(... VERSION ...)');
    expect(findings[1]?.line).toBe(2);
    expect(findings[1]?.message).toContain('set(GRAPH_LITE_VERSION)');
    for (const f of findings) expect(f.message).toContain('0.2.0');
    fx.dispose();
  });

  it('flags a header STRING that disagrees with the macros above it', async () => {
    const fx = await fixture({
      'graph-lite/include/graph-lite.h': headerFor('0.1.0', '0.2.0'),
      'graph-lite/CMakeLists.txt': cmakeFor('0.1.0', '0.1.0'),
    });
    const findings = await run(fx.root);
    expect(findings).toHaveLength(1);
    expect(findings[0]?.file).toBe('graph-lite/include/graph-lite.h');
    expect(findings[0]?.line).toBe(4);
    expect(findings[0]?.message).toContain('GRAPH_LITE_VERSION_STRING');
    fx.dispose();
  });

  it('flags every line that lags a header-macro bump, each with the fix', async () => {
    const fx = await fixture({
      'graph-lite/include/graph-lite.h': headerFor('0.2.0', '0.1.0'),
      'graph-lite/CMakeLists.txt': cmakeFor('0.1.0', '0.1.0'),
    });
    const findings = await run(fx.root);
    expect(findings).toHaveLength(3);
    expect(findings[0]?.file).toBe('graph-lite/include/graph-lite.h');
    expect(findings[0]?.line).toBe(4);
    expect(findings[1]?.file).toBe('graph-lite/CMakeLists.txt');
    expect(findings[1]?.line).toBe(1);
    expect(findings[2]?.line).toBe(2);
    for (const f of findings) expect(f.message).toContain('0.2.0');
    fx.dispose();
  });

  it('flags a version statement that was deleted', async () => {
    const fx = await fixture({
      'graph-lite/include/graph-lite.h': headerFor('0.1.0'),
      'graph-lite/CMakeLists.txt': 'project(graph-lite VERSION 0.1.0 LANGUAGES CXX)\n',
    });
    const findings = await run(fx.root);
    expect(findings).toHaveLength(1);
    expect(findings[0]?.message).toContain('set(GRAPH_LITE_VERSION');
    fx.dispose();
  });

  it('flags a version stated through something the guard cannot read', async () => {
    const fx = await fixture({
      'graph-lite/include/graph-lite.h': headerFor('0.1.0'),
      'graph-lite/CMakeLists.txt': `project(graph-lite VERSION 0.1.0 LANGUAGES CXX)\nset(GRAPH_LITE_VERSION \${PROJECT_VERSION})\n`,
    });
    const findings = await run(fx.root);
    expect(findings).toHaveLength(1);
    expect(findings[0]?.line).toBe(2);
    expect(findings[0]?.message).toContain('literal');
    fx.dispose();
  });

  it('flags exactly the statements that depart from the macros', async () => {
    const fx = await fixture({
      'graph-lite/include/graph-lite.h': headerFor('0.1.0', '0.2.0'),
      'graph-lite/CMakeLists.txt': cmakeFor('0.2.0', '0.1.0'),
    });
    const findings = await run(fx.root);
    expect(findings).toHaveLength(2);
    expect(findings[0]?.file).toBe('graph-lite/include/graph-lite.h');
    expect(findings[0]?.line).toBe(4);
    expect(findings[1]?.file).toBe('graph-lite/CMakeLists.txt');
    expect(findings[1]?.line).toBe(1);
    for (const f of findings) expect(f.message).toContain('0.1.0');
    fx.dispose();
  });

  it('flags a version macro that was deleted', async () => {
    const header = headerFor('0.1.0')
      .split('\n')
      .filter((l) => !l.includes('GRAPH_LITE_VERSION_PATCH'))
      .join('\n');
    const fx = await fixture({
      'graph-lite/include/graph-lite.h': header,
      'graph-lite/CMakeLists.txt': cmakeFor('0.1.0', '0.1.0'),
    });
    const findings = await run(fx.root);
    expect(findings).toHaveLength(1);
    expect(findings[0]?.message).toContain('GRAPH_LITE_VERSION_PATCH');
    fx.dispose();
  });

  it('passes the real tree', async () => {
    expect(await run()).toEqual([]);
  });
});
