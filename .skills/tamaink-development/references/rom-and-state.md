# ROM and State Contract

Read this reference for ROM parsing, TamaLib integration, persistence, testing,
or handling user-provided binary data.

## ROM boundary

- Accept a user-provided `/rom.bin` from microSD.
- Open it read-only and never modify it in place.
- Never bundle, download, redistribute, print, or commit ROM contents.
- Keep ROM paths, hashes, and test metadata free of unnecessary personal data.
- Use synthetic fixtures for committed tests whenever possible.

The initial known-compatible P1 ROM has:

- file size: 12,288 bytes;
- 6,144 packed 12-bit instructions stored as two bytes each;
- zero non-zero upper nibbles in each instruction's first byte;
- TamaTool detection CRC32 `C7875F27`; and
- SHA-256 `67B6388F26E2E3F15674932BAF2FC2FB1C6F388CC0F16EA1AA0F441DB1A4F43C`.

The hash identifies the development sample; do not make it the only accepted P1
ROM. Validate structure and supported P1 identity separately.

## Packed decoding

For each two-byte pair `b0, b1`, require `(b0 & 0xF0) == 0`, then decode:

```text
instruction = b1 | ((b0 & 0x0F) << 8)
```

Reject odd sizes, unexpected instruction counts, invalid high nibbles,
unsupported types, truncated reads, and allocation failures with a clear error.

Match TamaTool identification by converting decoded instructions into the
configured `u12_t` representation and calculating CRC32 over byte offset
`0x2F0`, length `0x110`. Do not silently rely on TamaTool's fallback-to-P1
behavior for unknown CRCs.

## Host validation

Test at minimum:

- the known P1 sample;
- missing file;
- empty and odd-sized files;
- truncated input;
- invalid high nibble;
- structurally valid but unsupported identity;
- read failure and allocation failure; and
- deterministic decode results across supported hosts.

Do not copy the real ROM into the repository or CI. Allow an optional local path
for non-committed integration tests.

## Save-state durability

Design a versioned, endian-defined format rather than dumping C structs.
Include:

- magic and format version;
- emulator/core compatibility identifier;
- ROM identity;
- monotonic generation or sequence;
- complete required emulator state;
- timekeeping metadata; and
- checksum or CRC covering the serialized record.

Maintain at least two generations. Write a new temporary record, flush and
verify it, then promote it without destroying the previous valid record. On
startup, validate all candidates and load the newest compatible valid state.

Never claim atomicity based only on a filesystem rename; test actual SdFat and
microSD behavior under forced reset at every write phase.

## Time semantics

Treat save/resume and elapsed-time catch-up as emulator behavior, not merely RTC
bookkeeping. Do not replace skipped CPU execution with wall-clock adjustment
until deterministic host tests prove equivalent P1 outcomes for the relevant
intervals and events.

