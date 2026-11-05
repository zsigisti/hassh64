# hassh64 (Rust)

A fast, seeded, non-cryptographic 64-bit hash. Pure Rust, `#![no_std]`, no dependencies.
Output is bit-identical to the C reference (`hassh.h`) and to [`SPEC.md`] on every platform,
and frozen for all 1.x releases.

> **AI-generated.** The hash design, this crate, its tests and its benchmarks were written by
> an AI (Anthropic's Claude, via Claude Code) under human direction, and reviewed only by AI
> agents. Nothing here has been checked by an independent human expert.

**Not** a cryptographic hash, **not** a MAC, and **no** claim of HashDoS resistance. Use it
for hash tables, checksums and deduplication, not for passwords, signatures or anything an
attacker must not be able to forge.

```rust
let h = hassh64::hassh64(b"hello", 0);
assert_eq!(h, 0xedfe40b18d046dba); // same as the C header and the CLI

// As a HashMap hasher, with a fixed seed (deterministic):
use std::collections::HashMap;
let mut m: HashMap<u64, &str, hassh64::Hassh64BuildHasher> = HashMap::default();
m.insert(1, "one");

// Or with a random per-map seed (feature `std`, on by default):
let mut m: hassh64::HashMap<&str, u32> = hassh64::HashMap::default();
m.insert("two", 2);
```

## API

| item | what it is |
|---|---|
| `hassh64(data: &[u8], seed: u64) -> u64` | the hash; equal to C `hassh64(data, len, seed)` |
| `Hassh64Hasher` | `core::hash::Hasher` (see below) |
| `Hassh64BuildHasher` | `BuildHasher` with a fixed seed (`with_seed`, `Default` = seed 0) |
| `RandomState`, `HashMap`, `HashSet` | random-seeded builder and std aliases (feature `std`) |

### The `Hasher` adapter

hassh64 is one-shot, so `Hassh64Hasher` buffers writes in a 64-byte inline buffer and hashes
them in `finish`. If a write does not fit, the buffer is topped up and compressed into the
state (`state = hassh64(buffer, state)`), and a remaining part over 64 bytes is compressed in
one call.

- Up to 64 bytes in total, the result is exactly `hassh64(bytes, seed)`, however the bytes
  were split across `write` calls.
- Beyond that, the result depends on the write boundaries and differs from `hassh64` of the
  concatenation. That is allowed by the `Hasher` contract, but do not compare `Hasher`
  output with `hassh64` output.
- Integers are written as little-endian bytes (`usize`/`isize` as 64-bit), so `Hasher` output
  is the same on every platform. A `u64` key costs one 8-byte store and one 8-byte hash.

## Implementation paths

| input | path |
|---|---|
| 0..64 bytes | scalar 64x64->128 multiplies; identical code everywhere |
| 65+ bytes, x86_64 | AES-NI + SSE4.1: compiled in with `-C target-cpu=native` (or `+aes,+sse4.1`), otherwise detected at run time with `std` |
| 65+ bytes, aarch64 | ARMv8 AES: compiled in with the `aes` target feature (default on Apple), otherwise detected at run time with `std` |
| otherwise | portable software AES round (same output, slower) |

The C header's 256-bit VAES path is not ported; it changes speed, not output.

## Benchmarks

Intel Core Ultra 7 265KF, one core, `-C target-cpu=native`, criterion medians. Latency is one
hash whose result feeds the next input, in ns (lower is better):

| bytes | hassh64 | rapidhash v3 | foldhash fast | ahash |
|---:|---:|---:|---:|---:|
| 8 | 4.3 | 3.8 | 4.1 | 6.2 |
| 16 | 4.5 | 2.9 | 3.2 | 5.0 |
| 32 | 6.1 | 5.3 | 4.9 | 8.2 |
| 64 | 3.5 | 8.0 | 6.2 | 9.3 |
| 128 | 11.5 | 4.9 | 7.6 | 11.2 |
| 256 | 12.9 | 7.9 | 11.6 | 12.5 |
| 1024 | 30.4 | 21.6 | 23.8 | 20.3 |

`HashMap<u64, u64>`, 4096 inserts then 4096 lookups, in µs:

| hassh64 | rapidhash fast | foldhash fast | ahash | std (SipHash) |
|---:|---:|---:|---:|---:|
| 31.2 | 20.4 | 19.0 | 20.7 | 150.3 |

**hassh64 is not the fastest Rust hasher.** It is about 1.5x slower than rapidhash and
foldhash for `u64` map keys, because its 8-byte path is two dependent multiplies (part of the
frozen algorithm, and what keeps SMHasher3's Bitflip test clean) where they do one. It is the
fastest of the four at 64 bytes and close at 8, 32 and 256 bytes, but clearly slower at 16
and 128 bytes and at 1 KiB (the C header's VAES path is not ported yet). Choose it for its
SMHasher3 result and its cross-language, cross-platform output, not for raw speed in Rust.

## Features

- `std` (default): run-time CPU detection, `RandomState`, `HashMap`, `HashSet`. Without it
  the crate is `#![no_std]` and needs no allocator.

MSRV: Rust 1.72 (aarch64 AES intrinsics).

## Testing

`cargo test` checks all 532 frozen vectors from the C repository's `test/vectors.h` (copied
into `tests/`) on the default path, the forced portable path and the hardware path, at
aligned and unaligned addresses. `cargo bench` compares against rapidhash, foldhash and
ahash.

## License

MIT.

[`SPEC.md`]: https://github.com/zsigisti/hassh64/blob/main/SPEC.md
