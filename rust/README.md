# hassh64 (Rust)

A fast, seeded, non-cryptographic 64-bit hash. Pure Rust, `#![no_std]`, no dependencies.
Output is bit-identical to the C reference (`hassh.h`) and to [`SPEC.md`] on every platform,
and frozen for all 2.x releases. The 2.x output is final: no 3.0 is planned (see the
[stability promise](https://github.com/zsigisti/hassh64#versioning)). 1.x outputs differ for inputs over 64 bytes (2.0 changed the
AES section: 3 rounds per block, a separate length vector, and LeMac's absorber from 4096
bytes); inputs of 0..64 bytes hash the same in 1.x and 2.x.

```
cargo add hassh64
```

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
| `Hassh64Stream` | streaming: `update` in pieces, `digest` == `hassh64` of the whole input; `std::io::Write` with `std` |
| `Hassh64Hasher` | `core::hash::Hasher` (see below) |
| `Hassh64BuildHasher` | `BuildHasher` with a fixed seed (`with_seed`, `Default` = seed 0) |
| `RandomState`, `HashMap`, `HashSet` | random-seeded builder and std aliases (feature `std`) |

### Streaming

```rust
use std::io;
let mut st = hassh64::Hassh64Stream::new(seed);
io::copy(&mut std::fs::File::open(path)?, &mut st)?;   // or st.update(chunk) by hand
let h = st.digest();                                    // == hassh64(&whole_file, seed)
```

The result equals the one-shot hash for every way of splitting the input. Under 4 KiB the
input is buffered; past that the LeMac absorber runs as data arrives, at one-shot speed for
large chunks. `digest` does not consume the stream. Use this, not `Hassh64Hasher`, when the
value must match `hassh64` (stored fingerprints, the C or SQL versions).

### The `Hasher` adapter

hassh64 is one-shot, so `Hassh64Hasher` buffers writes in a 64-byte inline buffer and hashes
them in `finish`. If a write does not fit, the buffered bytes are compressed into the seed
(`seed = hassh64(buffer, seed)`), and a write over 64 bytes is then hashed in one call with
that seed. The seed keys are derived once per `BuildHasher`, not once per key.

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

The C header's 256-bit VAES paths (lanes and the LeMac inline-assembly loop) are not ported;
they change speed, not output. The 4096+ byte LeMac absorber runs on 128-bit AES instructions
here, one state after the other.

## Benchmarks

Intel Core Ultra 7 265KF, one core, `-C target-cpu=native`, criterion medians. Latency is one
hash whose result feeds the next input, in ns (lower is better). Measured on 2.0.0.

Hashing a byte slice directly (one-shot functions):

| bytes | hassh64 | rapidhash v3 |
|---:|---:|---:|
| 8 | 4.6 | 4.0 |
| 16 | 4.7 | 3.1 |
| 32 | 6.2 | 5.5 |
| 64 | 3.5 | 8.5 |
| 128 | 11.9 | 5.1 |
| 256 | 13.4 | 8.1 |
| 1024 | 25.5 | 21.9 |

Hashing a `&[u8]` through `BuildHasher::hash_one` (what a `HashMap<Vec<u8>, _>` does; adds a
length prefix):

| bytes | hassh64 | foldhash fast | ahash |
|---:|---:|---:|---:|
| 8 | 6.0 | 4.2 | 6.4 |
| 16 | 10.6 | 3.2 | 5.1 |
| 32 | 12.1 | 5.0 | 8.5 |
| 64 | 12.7 | 6.4 | 9.7 |
| 128 | 17.3 | 7.9 | 11.7 |
| 256 | 21.5 | 11.9 | 12.7 |
| 1024 | 33.8 | 24.6 | 20.6 |

`HashMap<u64, u64>`, 4096 inserts then 4096 lookups, in µs:

| hassh64 | rapidhash fast | foldhash fast | ahash | std (SipHash) |
|---:|---:|---:|---:|---:|
| 32.3 | 19.9 | 19.7 | 19.5 | 155.3 |

**hassh64 is not the fastest Rust hasher.** As a `HashMap` hasher it is about 1.6x slower than
foldhash, ahash and rapidhash, and about 5x faster than std's SipHash. Its short paths are two
to four dependent multiplies (part of the frozen algorithm, and what keeps SMHasher3's Bitflip
test clean) where the others do one or two, and the C header's VAES path is not ported yet (2.0 cut 1 KiB from 31.5 to 25.5 ns).
As a one-shot function it is the fastest here at 64 bytes and close at 8 and 32. Choose it for
its SMHasher3 result and its identical output across languages and platforms, not for raw
speed in Rust.

## Features

- `std` (default): run-time CPU detection, `RandomState`, `HashMap`, `HashSet`. Without it
  the crate is `#![no_std]` and needs no allocator.

MSRV: Rust 1.72 (aarch64 AES intrinsics).

## Testing

`cargo test` checks all 548 frozen vectors from the C repository's `test/vectors.h` (copied
into `tests/`) on the default path, the forced portable path and the hardware path, at
aligned and unaligned addresses. `cargo bench` compares against rapidhash, foldhash and
ahash.

## License

MIT.

[`SPEC.md`]: https://github.com/zsigisti/hassh64/blob/main/SPEC.md
