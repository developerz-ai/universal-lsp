#!/usr/bin/env bun
/** Prove graph-lite builds and tests after copying only its distributable tree. */
import { cpSync, mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

import { runOrThrow } from './lib/cmd.ts';
import { REPO_ROOT } from './lib/git.ts';

const source = join(REPO_ROOT, 'graph-lite');
const root = mkdtempSync(join(tmpdir(), 'graph-lite-standalone-'));

try {
  rmSync(root, { recursive: true });
  cpSync(source, root, { recursive: true });
  await runOrThrow(
    ['cmake', '-S', '.', '-B', 'build', '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Debug'],
    root,
  );
  await runOrThrow(['cmake', '--build', 'build'], root);
  await runOrThrow(['ctest', '--test-dir', 'build', '--output-on-failure'], root);
} finally {
  rmSync(root, { recursive: true, force: true });
}
