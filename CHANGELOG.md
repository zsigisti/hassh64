# Changelog

All notable changes to this project are documented here. hassh64 follows
[Semantic Versioning](https://semver.org/): any change to the hash output is a new major version.

## [1.0.0] - 2026-11-04

First release.

- `hassh.h`: single-header C99 / C++11 implementation with AES-NI, VAES, ARMv8 AES and portable
  C paths, all bit-identical.
- Run-time AES-NI detection for x86-64 GCC/Clang builds with default flags; MSVC support.
- `SPEC.md` and `test/vectors.h` (532 known answers) freezing the 1.x output.
- `hassh64` command-line tool, with prebuilt binaries for Linux, macOS and Windows.
- CMake package (`hassh64::hassh64`), pure-Python reference port, CI on Linux, macOS and Windows.

[1.0.0]: https://github.com/zsigisti/hassh64/releases/tag/v1.0.0
