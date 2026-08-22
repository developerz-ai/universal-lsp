/** Subprocess helpers. Every shell-out in scripts/ goes through here. */
import { fail } from './log.ts';

export interface RunResult {
  code: number;
  stdout: string;
  stderr: string;
}

/** Run a command, capture output, never throw. */
export async function run(argv: string[], cwd?: string): Promise<RunResult> {
  const proc = Bun.spawn(argv, { cwd, stdout: 'pipe', stderr: 'pipe' });
  const [stdout, stderr] = await Promise.all([
    new Response(proc.stdout).text(),
    new Response(proc.stderr).text(),
  ]);
  return { code: await proc.exited, stdout, stderr };
}

/** Run a command with inherited stdio; exit non-zero on failure. */
export async function runOrThrow(argv: string[], cwd?: string): Promise<void> {
  const proc = Bun.spawn(argv, { cwd, stdout: 'inherit', stderr: 'inherit' });
  const code = await proc.exited;
  if (code !== 0) {
    fail(`${argv.join(' ')} exited ${code}`);
    process.exit(code);
  }
}

export async function has(bin: string): Promise<boolean> {
  return (await run(['sh', '-c', `command -v ${bin}`])).code === 0;
}
