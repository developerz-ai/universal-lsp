import { describe, expect, it } from 'bun:test';

import type { RunResult } from './cmd.ts';
import {
  CLANG_FORMAT_MAJOR,
  clangFormatMismatch,
  installMissing,
  missingRequired,
  prereqs,
  type Shell,
} from './prereqs.ts';

/**
 * A box with exactly `bins` on PATH, where `sudo -n true` exits `sudoCode` and
 * `apt-get update` exits `updateCode`.
 */
function box(bins: string[], sudoCode = 0, updateCode = 0): Shell & { ran: string[][] } {
  const ran: string[][] = [];
  const ok: RunResult = { code: 0, stdout: '', stderr: '' };
  return {
    ran,
    has: async (bin) => bins.includes(bin),
    run: async (argv) => {
      ran.push(argv);
      const cmd = argv.join(' ');
      if (cmd === 'sudo -n true') return { ...ok, code: sudoCode };
      if (cmd.includes('apt-get update'))
        return { ...ok, code: updateCode, stderr: 'E: Failed to fetch' };
      return ok;
    },
    runOrThrow: async (argv) => {
      ran.push(argv);
      // The real runOrThrow exits setup on a non-zero code.
      if (argv.includes('update') && updateCode !== 0) throw new Error(`exited ${updateCode}`);
    },
  };
}

const installs = (sh: { ran: string[][] }) => sh.ran.filter((a) => a.includes('apt-get'));

describe('installMissing', () => {
  const missing = prereqs().filter((p) => p.bin === 'cmake' || p.bin === 'clang-format');

  it('installs the missing required packages on a passwordless-sudo apt box', async () => {
    const sh = box(['apt-get', 'sudo']);
    expect(await installMissing(missing, sh)).toEqual(['cmake', 'clang-format']);
    expect(installs(sh).at(-1)?.slice(-2)).toEqual(['cmake', 'clang-format']);
  });

  it('still installs when apt-get update could not refresh every index', async () => {
    // A mirror that drops one Packages.xz makes `apt-get update` exit 100 (measured on a
    // fleet box, 2026-11-10); the lists already on disk still resolve the packages.
    const sh = box(['apt-get', 'sudo'], 0, 100);
    expect(await installMissing(missing, sh)).toEqual(['cmake', 'clang-format']);
    expect(installs(sh).at(-1)?.slice(-2)).toEqual(['cmake', 'clang-format']);
  });

  it('installs nothing when sudo would prompt for a password', async () => {
    // A laptop: setup must not hang on a prompt or install behind the user's back.
    const sh = box(['apt-get', 'sudo'], 1);
    expect(await installMissing(missing, sh)).toEqual([]);
    expect(installs(sh)).toEqual([]);
  });

  it('installs nothing without apt-get or sudo', async () => {
    for (const bins of [['sudo'], ['apt-get']]) {
      const sh = box(bins);
      expect(await installMissing(missing, sh)).toEqual([]);
      expect(sh.ran).toEqual([]);
    }
  });

  it('runs nothing when nothing is missing', async () => {
    const sh = box(['apt-get', 'sudo']);
    expect(await installMissing([], sh)).toEqual([]);
    expect(sh.ran).toEqual([]);
  });
});

describe('missingRequired', () => {
  it('names only the required tools that are absent', async () => {
    const all = prereqs();
    const present = all.filter((p) => p.bin !== 'git').map((p) => p.bin);
    const missing = await missingRequired(all, box(present));
    expect(missing.map((p) => p.bin)).toEqual(['git']);
  });

  it('never installs an optional tool', async () => {
    const missing = await missingRequired(prereqs(), box([]));
    expect(missing.every((p) => p.required)).toBe(true);
    expect(missing.map((p) => p.bin)).not.toContain('ccache');
  });
});

describe('the clang-format prerequisite', () => {
  it('checks the binary CLANG_FORMAT names, installed from the pinned package', () => {
    const p = prereqs(`clang-format-${CLANG_FORMAT_MAJOR}`).find(
      (x) => x.why === 'the format gate',
    );
    expect(p).toMatchObject({ bin: 'clang-format-18', apt: 'clang-format-18' });
  });

  it('accepts the pinned major', () => {
    expect(clangFormatMismatch('Ubuntu clang-format version 18.1.3 (1ubuntu1)')).toBeNull();
  });

  it('refuses another major with the fix, as the format gate would', () => {
    // Ubuntu 22.04's `clang-format` package is 14: present, but format.ts exits 1 on it.
    const why = clangFormatMismatch('Ubuntu clang-format version 14.0.0-1ubuntu1.1');
    expect(why).toContain('is version 14');
    expect(why).toContain('CLANG_FORMAT=clang-format-18');
  });
});
