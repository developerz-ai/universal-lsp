/** Build a throwaway file tree for a guard's own test. Guards read the repo, so
 *  testing one means giving it a repo to read. */
import { mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

export interface Fixture {
  root: string;
  dispose(): void;
}

export async function fixture(files: Record<string, string>): Promise<Fixture> {
  const root = mkdtempSync(join(tmpdir(), 'ulsp-guard-'));
  for (const [rel, content] of Object.entries(files)) {
    await Bun.write(join(root, rel), content);
  }
  return { root, dispose: () => rmSync(root, { recursive: true, force: true }) };
}
