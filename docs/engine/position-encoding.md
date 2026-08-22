# Position encoding

**The single most reliable source of shipped bugs in the LSP ecosystem.** Get it
wrong here and the symptom is a crash in someone else's editor, on a file
containing an emoji, months after the mistake.

## The mismatch

LSP historically mandates **UTF-16 code units** for the `character` field of a
`Position` — a legacy of VS Code being JavaScript-based. The document content
itself travels as UTF-8. This engine will store text internally as UTF-8 or as
codepoints. So three encodings are in play and only one of them is on the wire.

| Encoding | Where |
|---|---|
| UTF-8 | the document content, always |
| UTF-16 code units | `Position.character`, unless negotiated otherwise |
| UTF-8 bytes or UTF-32 codepoints | internal storage and every internal index |

## Real cost of getting it wrong

rust-analyzer has panicked on certain multi-byte emoji sequences because a
computed offset landed **inside** a multi-code-unit character. That is the failure
shape: not a wrong hover, a process death, on input the user considers ordinary
text.

## Rules

1. **Negotiate.** LSP 3.17 added a `positionEncoding` client capability
   (`utf-8`, `utf-16`, `utf-32`). Read it at `initialize`, prefer UTF-8 or UTF-32
   when the client supports it, and **always** support UTF-16 as the fallback — it
   is still the default a client may assume.
2. **Convert at the boundary and nowhere else.** One function in, one function
   out. An offset that has crossed into the engine is in the internal encoding, by
   construction; if two encodings can coexist in the same struct, they eventually
   will.
3. **Never index into the middle of a character.** A conversion that lands
   mid-sequence is a bug in the conversion, not an input to handle downstream.
4. **A `Position` cannot legally point inside a line-ending sequence.** The spec
   recognises `\n`, `\r\n` and `\r`; line-splitting must treat all three
   consistently, and the cache must be unambiguous about which convention a given
   file uses.

## Test coverage this needs from the first commit

Not "later, as an edge case" — the conversion is a small pure function, which makes
it exactly the kind of thing that is cheap to test exhaustively and expensive to
debug in production.

| Case | Why it bites |
|---|---|
| ASCII | the trivial path must stay trivial |
| 2- and 3-byte UTF-8 (accents, CJK) | byte offset ≠ code-unit offset ≠ codepoint offset |
| Astral plane (emoji, U+1F600+) | one codepoint, **two** UTF-16 code units — the surrogate pair that produces the classic panic |
| Combining marks / ZWJ sequences | one user-perceived character, many codepoints |
| A position at end-of-line, and at end-of-file | off-by-one lives here |
| `\r\n` split across an incremental edit | a position must never land between the two |
| Round-trip property test | convert to the wire encoding and back; must be identity for every offset in a generated document |
