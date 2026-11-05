# Changelog

All notable changes to this project are documented here. hassh64 follows
[Semantic Versioning](https://semver.org/): any change to the hash output is a new major version.

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

[1.0.1]: https://github.com/zsigisti/hassh64/releases/tag/v1.0.1
[1.0.0]: https://github.com/zsigisti/hassh64/releases/tag/v1.0.0
