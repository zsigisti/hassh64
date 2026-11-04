<h1 align="center">hassh64</h1>

<p align="center">
  A fast, seeded, non-cryptographic 64-bit hash. One C header. Same output on every platform.
</p>

<p align="center">
  <a href="https://github.com/zsigisti/hassh64/actions/workflows/ci.yml"><img alt="CI" src="https://github.com/zsigisti/hassh64/actions/workflows/ci.yml/badge.svg"></a>
  <a href="https://github.com/zsigisti/hassh64/releases/latest"><img alt="Release" src="https://img.shields.io/github/v/release/zsigisti/hassh64"></a>
  <a href="LICENSE"><img alt="License: MIT" src="https://img.shields.io/badge/license-MIT-blue"></a>
  <img alt="SMHasher3" src="https://img.shields.io/badge/SMHasher3-252%2F252-brightgreen">
  <img alt="C99 / C++11" src="https://img.shields.io/badge/C99%20%2F%20C%2B%2B11-header--only-informational">
  <img alt="AI-generated" src="https://img.shields.io/badge/AI--generated-Claude-orange">
</p>

> [!IMPORTANT]
> **AI-generated.** The hash design, the code, the tests, the benchmarks and this README were
> written by an AI (Anthropic's Claude, via Claude Code), with a human directing it. The
> security review was also done by AI agents, not human cryptographers. Every number here was
> measured, but nothing has been checked by an independent human expert. Treat it accordingly.

```c
#include "hassh.h"
uint64_t h = hassh64(data, len, seed);
```

- **Fast:** on SMHasher3's speed test it beats every hash we measured that also passes the
  suite, on both short keys and bulk data ([numbers and caveats](#results)).
- **Clean:** passes all 252 SMHasher3 `--extra` quality tests.
- **Portable:** C99 / C++11, no dependencies. Uses AES-NI, VAES or ARMv8 AES when present
  (detected at run time on x86-64) and portable C everywhere else, with bit-identical output.
- **Specified:** [`SPEC.md`](SPEC.md) plus 532 frozen test vectors, so it can be ported to any
  language. A Python port written from the spec alone passes all of them.

For hash tables, checksums and deduplication. **Not** for passwords, MACs, signatures, or keys
an attacker controls (see [Security](#security)).

## Using it

**Command line.** Download a binary from the
[release page](https://github.com/zsigisti/hassh64/releases/latest) (Linux, macOS and Windows,
x86-64 and ARM64), or build it with `make cli`:

```
$ hassh64 hello
edfe40b18d046dba
$ hassh64 -s 42 hello          # with a seed (decimal or 0x hex)
825305b7d83ed2a7
$ hassh64 -f README.md         # hash files
$ printf hello | hassh64       # hash standard input
edfe40b18d046dba
```

Text arguments are hashed exactly as given, with no trailing newline. (`echo hello | hassh64`
hashes `hello\n`, a different input.) The same input and seed print the same hash on every
machine.

**C / C++.** Copy `hassh.h` into your project. It needs C99 or C++11, has no dependencies, and
compiles warning-free under GCC and Clang (`-Wall -Wextra -pedantic`, and Clang `-Weverything`
apart from style-only warnings).

**CMake.**

```cmake
include(FetchContent)
FetchContent_Declare(hassh64 GIT_REPOSITORY https://github.com/zsigisti/hassh64 GIT_TAG v1.0.0)
FetchContent_MakeAvailable(hassh64)
target_link_libraries(your_target PRIVATE hassh64::hassh64)
```

`cmake --install` also installs the header and a `find_package(hassh64)` config.

**Other languages.** [`SPEC.md`](SPEC.md) defines the algorithm completely, and
[`test/vectors.h`](test/vectors.h) has 532 known answers. A port is correct when it reproduces
all of them. [`ports/python/hassh64.py`](ports/python/hassh64.py) is a slow pure-Python
reference, written by an AI agent from `SPEC.md` alone (without reading the C code) as a check
that the spec is complete. It passes all 532 vectors.

**Platforms.** Every platform gives bit-identical output:

| platform | path | how |
|---|---|---|
| x86-64, GCC / Clang, `-march=native` or `-maes -msse4.1` | AES-NI (and VAES with `-mvaes -mavx2`) | compile time |
| x86-64, GCC / Clang, default flags | AES-NI if the CPU has it, else portable C | run-time check, cached (`HASSH_NO_DISPATCH` turns it off) |
| x86-64, MSVC | AES-NI with `/arch:AVX2` or `HASSH_ASSUME_AESNI`, else portable C | compile time |
| AArch64 with the crypto extension (Apple targets, `-march=armv8-a+crypto`) | ARMv8 AES | compile time |
| anything else: 32-bit, big-endian, RISC-V, WebAssembly, ... | portable C | |

The known-answer vectors pass on real hardware in CI: Linux x86-64 (GCC with run-time
dispatch, `-march=native`, portable), Linux AArch64 with AES, macOS on Apple silicon, and
Windows MSVC (portable and `/arch:AVX2`). Locally they also pass on VAES, musl, 32-bit x86,
and, under QEMU, s390x (big-endian), 32-bit ARM and AArch64 without AES. RISC-V, WebAssembly
(WASI) and MinGW are compile-checked only.

The portable C path is much slower for inputs over 64 bytes (software AES). Inputs up to 64
bytes never use AES, so they are equally fast everywhere.

## Results

Measured on an Intel Core Ultra 7 265KF (Arrow Lake), one P-core, Linux, GCC 16, `-march=native`.

**Quality:** SMHasher3 `--extra` (the test set behind SMHasher3's published results table):
**252 / 252 pass.**

**Speed:** SMHasher3 `--test=Speed`, pinned to one core. Rows marked * are the median of 3
back-to-back runs, which agreed to within 0.1%. The other two rows are one run each, from
the same day and machine.

| hash | SMHasher3 result | small keys 1–31 B (cycles/hash, lower is better) | bulk 256 KiB (bytes/cycle, higher is better) |
|---|---|---:|---:|
| **hassh64** * | **pass** | **16.3** | **18.2** |
| a5hash * | pass | 18.1 | 3.0 |
| MuseAir.bfast | pass | 18.7 | 10.4 |
| rapidhash * | pass | 28.1 | 13.1 |
| MeowHash.64 * | pass | 54.2 | 16.7 |
| XXH3-64 | fails 27 tests | 28.1 | 19.7 |
| gxhash-64 * | fails 24 tests | 30.7 | 23.1 |

Pass/fail for the other hashes is from SMHasher3's published results table.

**Inlined benchmark** (`make bench`; keys not freshly written; ns per hash, latency | throughput):

| len | hassh64 | xxh3 | rapidhash | wyhash | gxhash |
|---:|---|---|---|---|---|
| 8 | 4.39 \| 1.12 | 4.77 \| 1.32 | 4.01 \| 0.94 | 5.36 \| 1.18 | 5.10 \| 0.59 |
| 32 | 6.68 \| 1.97 | 4.82 \| 1.79 | 5.49 \| 1.48 | 5.33 \| 1.53 | 6.68 \| 0.81 |
| 64 | 6.31 \| 2.34 | 6.11 \| 2.61 | 8.15 \| 2.39 | 6.92 \| 2.21 | 9.07 \| 1.10 |
| 128 | 11.24 \| 2.85 | 7.41 \| 4.40 | 8.43 \| 3.76 | 10.30 \| 4.05 | 9.53 \| 1.98 |
| 256 | 13.73 \| 5.21 | 17.91 \| 10.01 | 12.17 \| 6.63 | 13.22 \| 7.05 | 13.26 \| 2.84 |
| 512 | 17.98 \| 8.82 | 20.05 \| 11.15 | 19.58 \| 12.71 | 23.07 \| 14.76 | 14.51 \| 4.87 |
| 256 KiB | 69 GB/s | 75 GB/s | 48 GB/s | 29 GB/s | 97 GB/s |

### How far to trust these numbers

* **One CPU.** Everything was measured on a single Intel machine. AMD, older Intel and ARM may
  rank the hashes differently. Without VAES (Intel before Ice Lake, AMD before Zen 3), bulk
  speed roughly halves. ARM was checked for correctness under QEMU, never for speed.
* **The SMHasher3 small-key win is partly harness-specific.** That test writes the key and then
  hashes it straight away. hassh64 is built to avoid the store-forwarding stall this causes
  (a5hash does the same). With keys that were not just written, the inlined benchmark shows
  rapidhash and xxh3 slightly faster on 8–32 bytes.
* **65–128 bytes is the weak spot** (11.2 ns latency, against 7.4–10.3 ns for the others).
* SMHasher3 "cycles" are TSC ticks, not core clock cycles. They are only comparable within
  one session on one machine, which is how every row above was measured.
* The inlined benchmark was written by the same AI that wrote the hash. The gxhash numbers in
  it use a C port of gxhash taken from SMHasher3, not the Rust crate.

## When to use something else

* **Untrusted keys on a server** (HashDoS): use SipHash or another keyed PRF.
* **gxhash** is faster in bulk (about 23 vs 18 bytes/cycle here), but it fails 24 SMHasher3
  tests. Those include real 64-bit collisions on long sparse keys (2048-byte keys with up to 2
  bits set) and on 4096-byte keys with a single non-zero byte. On all-zero keys, it gave 4,160
  exact collisions across 4,160 seeds where 0 are expected, so the seed does not do its job
  there. It also needs AES hardware: there is no portable fallback. It is a good choice if all
  of that is acceptable for your data.
* **XXH3** is everywhere, well reviewed and fast in bulk, but it fails 27 SMHasher3 tests. It is
  a better choice if you need a hash that many other tools already speak.
* **rapidhash** passes SMHasher3 and is portable plain C with no AES. It is a better choice
  when most inputs are over 64 bytes and the CPU has no AES unit.

## Security

hassh64 has **no HashDoS-resistance claim**. The seed protects against accidental collisions
and casual attacks, not against a determined attacker.

What has been done: AI agents tried to build inputs that collide under every seed, and the
attacks they found against earlier drafts are now regression tests. `test/seedindep.c` and
`test/xlen.c` rebuild them against the current code (zeroed multiply operands, swapped
operands, swapped AES lanes, length changes, 256-way multicollisions). Every pair must not
collide under any of 65,541 seeds. Passing rules out those attacks, nothing more. No human
cryptographer has analysed it.

Known residual weaknesses, each affecting about 2^-64 of seeds:

* If `s`, `s2` or `s ^ s2` happens to be 0, one operand is keyed only by constants for that seed.
* `fold` absorbs both 0 and ~0 (`fold(~0, b) == ~0` for any b ≠ 0). Forcing an operand to
  either value needs the seed.
* The seed is 64 bits, and the seed-to-key map is not injective (about 2^63.3 distinct functions).

## Design

* **0–32 bytes, scalar.** 64×64→128 multiplies folded to 64 bits (the wyhash / rapidhash
  family). Both operands of every first-level multiply are keyed by the seed, with keys that are
  nonlinear in each other (`s`, and `s2 = f(s, len)`), so forcing an operand to 0 or ~0, or
  swapping operands, needs the seed. For short keys both halves of the first product feed the
  second multiply. Bytes 0..3 are only read by loads that start at the key, so a freshly
  written key forwards from the store buffer without a stall.
* **33–64 bytes, scalar.** Four parallel multiplies, then one. The two pairs that are XORed use
  key pairs `(s, s2)` and `(s^s2, s)`, which differ by a seed-dependent value even when commuted.
* **65+ bytes, AES.** 8 lanes (65–128 bytes) or 16 lanes of 128 bits. Each lane starts from two
  AES rounds of `K_i ^ (s, fold(s ^ len))`. Each 16-byte block runs `acc = P(acc ^ block)`, where
  P is 4 AES rounds, so a data difference passes at least 25 active S-boxes before it can meet
  another. The lanes are XORed, then get 3 more rounds and a 64-bit fold. Those 3 rounds remove
  the XOR's statistical linearity, but they are a public bijection and add no collision
  resistance. Inputs over 256 bytes use 256-byte stripes, the last aligned to the end.

Things that were tried and fail:

1. **1 AES round per block** (gxhash-style) fails SMHasher3's Sparse test. A 1-round
   difference has about 8 bits of entropy, so two blocks cancel by birthday.
2. **Plain XOR to combine lanes, without a finalizer,** fails the differential-distribution
   test, because h(0)⊕h(A)⊕h(B)⊕h(A⊕B) = 0.
3. **Multiplying short keys only by constants** gives a small Bitflip bias on 4-byte keys.
4. **Wide loads over bytes 0..3** cause ~12-cycle store-forwarding stalls. GCC also merges two
   adjacent 4-byte loads into one 8-byte load; an empty `asm` on the head load stops it.
5. **A seed that enters only linearly** (XORed into a constant-keyed operand or a lane start)
   allows collisions for every seed. "Passes SMHasher3" says nothing about this: statistical
   suites never build structured collisions.

## Testing

```
make test        # known-answer vectors on every x86 path, ISA cross-check, ASan bounds, attack suites
make test-cross  # vectors on i386, musl, s390x, ARM, AArch64 (zig + QEMU_DIR with qemu-s390x/arm/aarch64)
make test-arm    # full suite on AArch64 under QEMU
./fetch-deps.sh  # clones xxHash, rapidhash, wyhash and SMHasher3 into deps/
python3 ports/python/hassh64.py   # the spec-only Python port against the vectors
make bench       # the inlined benchmark above
```

For SMHasher3, copy `smhasher3/hassh.cpp` into `deps/smhasher3/hashes/`, add it to
`hashes/Hashsrc.cmake`, build, and run `./SMHasher3 --extra hassh-64`.

## Versioning

The output for every input is frozen for all 1.x releases, pinned by `test/vectors.h`. Any
output change, on any input, means a new major version. `HASSH_VERSION_MAJOR` tells you which
one you have.

## License

MIT. See [LICENSE](LICENSE).
