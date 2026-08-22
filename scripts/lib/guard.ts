/**
 * Shared shape for repo guards. One rule per file under scripts/lint/, each with
 * its own test — a guard that silently matches nothing is worse than no guard.
 *
 * Every finding names the value, the seam, and the fix. CI output is a prompt:
 * "bad vocabulary" costs the next agent a turn; the sentence below does not.
 */
export interface Finding {
  file: string;
  line: number;
  message: string;
}

export interface Guard {
  name: string;
  /** One line, shown in the gate output. */
  describe: string;
  run(): Promise<Finding[]>;
}

export function formatFinding(f: Finding): string {
  return `${f.file}:${f.line} ${f.message}`;
}

/** Read a file as lines, 1-indexed by position in the returned array + 1. */
export async function lines(path: string): Promise<string[]> {
  return (await Bun.file(path).text()).split('\n');
}
