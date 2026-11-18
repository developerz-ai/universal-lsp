import { describe, expect, it } from 'bun:test';

import { fixture } from '../lib/fixture.ts';
import { run } from './preset-schema.ts';

function preset(version: number, minor: number): string {
  return presetWithFloor(version, `${3}.${minor}`);
}

/** Same file, but the floor is written as \"<major>.<minor>\" so CMake 4.x is expressible. */
function presetWithFloor(version: number, floor: string): string {
  const [major, minor] = floor.split('.').map(Number);
  return `${JSON.stringify(
    {
      version,
      cmakeMinimumRequired: { major, minor, patch: 0 },
      configurePresets: [{ name: 'dev', generator: 'Ninja' }],
    },
    null,
    2,
  )}\n`;
}

/** A valid preset file with no cmakeMinimumRequired: the field is optional. */
function presetWithoutFloor(version: number): string {
  return `${JSON.stringify(
    { version, configurePresets: [{ name: 'dev', generator: 'Ninja' }] },
    null,
    2,
  )}\n`;
}

describe('preset-schema', () => {
  it('flags schema 6 declaring a CMake 3.24 floor, at the version line', async () => {
    const fx = await fixture({ 'CMakePresets.json': preset(6, 24) });
    const findings = await run(fx.root);
    expect(findings).toHaveLength(1);
    expect(findings[0]?.file).toBe('CMakePresets.json');
    expect(findings[0]?.line).toBe(2);
    expect(findings[0]?.message).toContain('needs CMake 3.25');
    expect(findings[0]?.message).toContain('cmakeMinimumRequired 3.24');
    expect(findings[0]?.message).toContain('Lower "version" to 5');
    fx.dispose();
  });

  it('flags a nested preset file too', async () => {
    const fx = await fixture({ 'graph-lite/CMakePresets.json': preset(6, 24) });
    const findings = await run(fx.root);
    expect(findings.map((f) => f.file)).toEqual(['graph-lite/CMakePresets.json']);
    fx.dispose();
  });

  it('passes schema 5 with a CMake 3.24 floor, and a higher floor', async () => {
    const fx = await fixture({
      'CMakePresets.json': preset(5, 24),
      'graph-lite/CMakePresets.json': preset(4, 28),
    });
    expect(await run(fx.root)).toEqual([]);
    fx.dispose();
  });

  it('passes a file with no cmakeMinimumRequired, which cmake-presets(7) makes optional', async () => {
    const fx = await fixture({
      'CMakePresets.json': presetWithoutFloor(6),
      'graph-lite/CMakePresets.json': presetWithoutFloor(5),
    });
    expect(await run(fx.root)).toEqual([]);
    fx.dispose();
  });

  it('flags a non-numeric cmakeMinimumRequired rather than the absent field', async () => {
    const fx = await fixture({
      'CMakePresets.json': `${JSON.stringify(
        {
          version: 5,
          cmakeMinimumRequired: { major: '3', minor: '24' },
          configurePresets: [{ name: 'dev', generator: 'Ninja' }],
        },
        null,
        2,
      )}\n`,
    });
    const findings = await run(fx.root);
    expect(findings).toHaveLength(1);
    expect(findings[0]?.message).toContain('not numbers');
    fx.dispose();
  });

  it('passes schema 7 and 12 with a floor that supports them', async () => {
    const fx = await fixture({
      'CMakePresets.json': presetWithFloor(7, '3.27'),
      'graph-lite/CMakePresets.json': presetWithFloor(12, '4.4'),
    });
    expect(await run(fx.root)).toEqual([]);
    fx.dispose();
  });

  it('skips preset files inside a generated build tree', async () => {
    const fx = await fixture({ 'build/dev/_deps/vendor/CMakePresets.json': preset(6, 24) });
    expect(await run(fx.root)).toEqual([]);
    fx.dispose();
  });

  it('flags an unknown schema version instead of guessing its CMake', async () => {
    const fx = await fixture({ 'CMakePresets.json': preset(99, 24) });
    const findings = await run(fx.root);
    expect(findings).toHaveLength(1);
    expect(findings[0]?.message).toContain('Extend SCHEMA_SUPPORT');
    fx.dispose();
  });

  it('passes the real tree', async () => {
    expect(await run()).toEqual([]);
  });
});
