/** Leveled logging with a fixed shape, because CI output is a prompt. */
const c = (n: number, s: string) => (process.stdout.isTTY ? `\x1b[${n}m${s}\x1b[0m` : s);

export const ok = (msg: string) => console.log(`${c(32, '✔')}  ${msg}`);
export const fail = (msg: string) => console.error(`${c(31, '✖')}  ${msg}`);
export const warn = (msg: string) => console.error(`${c(33, '!')}  ${msg}`);
export const step = (msg: string) => console.log(`${c(36, '▸')} ${msg}`);
export const dim = (msg: string) => c(2, msg);
