# Changelog

All notable changes to this project are documented here. hassh64 follows
[Semantic Versioning](https://semver.org/): any change to the hash output is a new major version.

## [2.2.0] - 2026-11-07

No output change: all 548 vectors are identical.

### Added
- Streaming API: `hassh64_init` / `hassh64_update` / `hassh64_digest` in C and
  `Hassh64Stream` (with `std::io::Write`) in Rust. The digest equals the one-shot hash for every
  length and every way of splitting the input (`test/stream.c`, `rust/tests/stream.rs`: 33,551
  length/chunking cases on the VAES, AES-NI, dispatch and portable paths, and under ASan).
  Inputs under 4 KiB are buffered; past that the LeMac absorber runs as data arrives, absorbing
  large updates straight from the caller's buffer: 1 MiB at 64 KiB chunks hashes at one-shot
  speed (about 115 GB/s with VAES), 4 KiB chunks at about 90 GB/s.
- `integrations/`: ready-to-use packaging with tests that check the same output everywhere:
  `kernel/` (freestanding guide for C and Rust kernels; build check for x86-64, AArch64,
  RISC-V, i686, RV32 and ARMv7 with no undefined symbols and no SIMD), `sqlite/` (loadable
  extension), `postgres/` (extension, PostgreSQL 13+), `cpp/` (`hassh::hash<T>` and
  `hassh::string_hash` for `std::unordered_map`). `make integrations` runs them. The README
  there also lists when hassh64 is the wrong choice, from trying it in real kernels.

### Fixed
- Rust: `hassh64::VERSION` said "2.0.0"; it now follows the crate version. CMake's project
  version said 2.0.0 too.

## [2.1.1] - 2026-11-06

No output change.

### Fixed
- Freestanding builds (`-ffreestanding`, which implies `-fno-builtin`, as kernels use): the
  fixed-size `memcpy` loads compiled to library calls, making short keys 4-9x slower than
  FNV-1a instead of faster. `memcpy`/`memset` now go through `__builtin_memcpy` /
  `__builtin_memset` on GCC and Clang, so no calls remain.
- Rust crate 2.1.1 (crates.io) carries 2.1.0's faster no-AES path; the C fixes above do not
  apply to it.
- Freestanding builds no longer include `<string.h>` (a kernel's include path may resolve it to
  libc's userspace header) and never dispatch to AES-NI at run time (a kernel may not save SSE
  state). Hosted builds are unchanged.

## [2.1.0] - 2026-11-06

No output change: all 548 vectors are identical.

### Added
- Stability promise (README, SPEC): the 2.x output is final and no 3.0 is planned. Future
  releases change speed and platform support only, never output.

### Changed
- Portable (no AES instructions) path for inputs over 64 bytes: the software AES round is a
  32-bit T-table round (one 1 KiB table) on blocks held as two 64-bit words, instead of a
  byte-wise S-box with byte-wise MixColumns. C, built without SSE as a kernel would be:
  128 bytes 953 -> 126 ns, 4096 bytes about 1.8 us (Arrow Lake, 1 core); with a prepared seed
  (`hassh64_keyed`) it is faster than FNV-1a from 128 bytes up, and 2-8x faster at 4-64 bytes
  as before. Rust `no_std` / no-AES builds: 3x faster (4096 bytes 5.6 -> 1.8 us).
- Holding a block as four 32-bit words was 2.5x slower: the compiler read pairs of them back
  with 64-bit loads, which stalls store forwarding.

## [2.0.0] - 2026-11-06

**Output changes for inputs longer than 64 bytes.** Inputs of up to 64 bytes hash exactly as in
1.x. New test vectors in `test/vectors.h` (548, now up to 20000 bytes); `SPEC.md` describes 2.x.

### Changed
- Inputs of 4096 bytes and more use a new absorber built on LeMac's step function (Bariant,
  David, Leurent, ToSC 2024; message schedule as corrected in ToSC 2025): two interleaved
  LeMac states, 2 AES rounds per 16 bytes, and the designers' bound of at least 26 active
  S-boxes for any differential trail to a state collision. Bulk speed with VAES +45-60% over
  the 2.0 lanes for aligned input, +20-35% for unaligned; AES-NI without VAES about +25%.
  Defined in `SPEC.md`; `test/lemac_ref.c` checks the step against the LeMac reference code and
  `test/longstats.c` adds avalanche, differential and sparse-key tests for long inputs.
- The VAES long-input loop is inline assembly (GCC and Clang): with intrinsics, GCC 16 at -O3
  spilled the LeMac state and ran at 15 bytes/cycle instead of 35.
- 3 AES rounds per 16-byte block instead of 4: bulk speed +29% (18.7 -> 24.1 bytes/cycle in
  SMHasher3), throughput on 128-512-byte keys about 40% better. Cost: fixed input pairs that
  collide for about 2^-51 of seeds (1.x: none; ideal: 2^-64). See README, Security.
- Lane starts depend on the seed only; the length enters through a length vector (two AES
  rounds over two seed-and-length folds) XORed in before the final rounds.
- `hassh64_key` also precomputes the 16 lane starts: 65-256-byte keys with a prepared seed are
  17-28% faster than 1.x.
- Passes SMHasher3 `--extra` 252/252, and every attack regression test.

### Tried and rejected
- Four LeMac states (256-byte steps) for AVX-512: twice the finalization cost, and with 16
  ymm registers the AVX2 build spilled; two states fill exactly one ymm register per word.
- LeMac below 4096 bytes: its finalization (4 blank steps, 4 rounds per word) costs more than
  it saves until about 4-6 KiB.
- 2 AES rounds per block (68% faster in bulk): real 64-bit collisions in SMHasher3's Sparse test.
- 2 final rounds instead of 3: passes 252/252, but saves only about 0.7 ns above 64 bytes.

## [1.1.0] - not released separately (included in 2.0.0)

### Added
- Prepared-seed API: `hassh64_key_init` / `hassh64_keyed`, identical output to `hassh64()`.

## [1.0.1] - 2026-11-05

No hash output changes: every 1.0.0 test vector still passes.

### Fixed
- `hassh.h`: clang-cl with `/arch:AVX2` failed to compile (it was treated like MSVC, but
  clang does not enable AES there). clang-cl now uses run-time detection. `HASSH_ASSUME_AESNI`
  is now MSVC-only, as documented.
- `hassh.h`: configuration macros (`HASSH_INLINE`, `HASSH_SIMD`, `HASSH_DISPATCH`, ...) no
  longer leak into code that includes the header.
- CLI on Windows: standard input was read in text mode, so CRLF became LF and byte 0x1A
  ended the input; piped data hashed differently from `-f` and from other systems. Text
  arguments are now hashed as UTF-8, and file names with non-ASCII characters open.
- CLI: a failed write to standard output (for example a full disk) now exits with status 1
  instead of 0.
- CLI: `-s` accepted octal (`010` meant 8), negative numbers and leading spaces. It now takes
  only decimal or `0x` hex, as documented.

### Added
- Native Rust crate in `rust/` (not yet on crates.io), with CI on Linux, macOS, Windows and
  ARM, and a minimum-Rust-version check.

## [1.0.0] - 2026-11-04

First release.

- `hassh.h`: single-header C99 / C++11 implementation with AES-NI, VAES, ARMv8 AES and portable
  C paths, all bit-identical.
- Run-time AES-NI detection for x86-64 GCC/Clang builds with default flags; MSVC support.
- `SPEC.md` and `test/vectors.h` (532 known answers) freezing the 1.x output.
- `hassh64` command-line tool, with prebuilt binaries for Linux, macOS and Windows.
- CMake package (`hassh64::hassh64`), pure-Python reference port, CI on Linux, macOS and Windows.

[2.0.0]: https://github.com/zsigisti/hassh64/tree/v2
[1.0.1]: https://github.com/zsigisti/hassh64/releases/tag/v1.0.1
[1.0.0]: https://github.com/zsigisti/hassh64/releases/tag/v1.0.0
