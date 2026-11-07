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
- **Specified:** [`SPEC.md`](SPEC.md) plus 548 frozen test vectors, so it can be ported to any
  language. A Python port written from the spec alone passes all of them.

For hash tables, checksums and deduplication. **Not** for passwords, MACs, signatures, or keys
an attacker controls (see [Security](#security)).

## Using it

Ready-made packaging for kernels, SQLite, PostgreSQL and C++ containers is in
[`integrations/`](integrations/), each with a test against the same vectors.
Ports into specific projects (melonDS, wren, miku-os, xcc), as patches with their test
results and benchmarks, are in [`projects/`](projects/).

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

**Many keys, one seed (hash tables).** Derive the seed-dependent keys once and reuse them.
The result is always identical to `hassh64()`; each call just does less work:

```c
hassh64_key k;
hassh64_key_init(&k, seed);                 /* once per seed (1072 bytes) */
uint64_t h = hassh64_keyed(&k, data, len);  /* == hassh64(data, len, seed) */
```

**Streaming (files, sockets, chunked data).** Feed the input in pieces of any size; the
digest is exactly `hassh64()` of the whole input. The state is about 4.7 KiB with no pointers,
so it can be copied, and `hassh64_digest` leaves it usable for more data. Past 4 KiB the
LeMac absorber runs as data arrives, at one-shot speed for chunks of 64 KiB and up (115-120
GB/s with VAES here), about 90 GB/s for 4 KiB chunks:

```c
hassh64_state st;
hassh64_init(&st, seed);
while ((n = fread(buf, 1, sizeof buf, f)) > 0) hassh64_update(&st, buf, n);
uint64_t h = hassh64_digest(&st);           /* == hassh64(whole file, size, seed) */
```

Rust: `hassh64::Hassh64Stream` (`update` / `digest`, and `std::io::Write`, so
`std::io::copy(&mut file, &mut stream)` works).

**CMake.**

```cmake
include(FetchContent)
FetchContent_Declare(hassh64 GIT_REPOSITORY https://github.com/zsigisti/hassh64 GIT_TAG v2.3.0)
FetchContent_MakeAvailable(hassh64)
target_link_libraries(your_target PRIVATE hassh64::hassh64)
```

`cmake --install` also installs the header and a `find_package(hassh64)` config.

**Rust.** `cargo add hassh64` ([crates.io](https://crates.io/crates/hassh64)): a native port with the same
output, `no_std`, with a `HashMap` hasher. See [`rust/README.md`](rust/README.md).

**Other languages.** [`SPEC.md`](SPEC.md) defines the algorithm completely, and
[`test/vectors.h`](test/vectors.h) has 548 known answers. A port is correct when it reproduces
all of them. [`ports/python/hassh64.py`](ports/python/hassh64.py) is a slow pure-Python
reference, written by an AI agent from `SPEC.md` alone (without reading the C code) as a check
that the spec is complete. It passes all 548 vectors (it was updated for the 4096+ byte
section the same way, from `SPEC.md` alone).

**Platforms.** Every platform gives bit-identical output:

| platform | path | how |
|---|---|---|
| x86-64, GCC / Clang, `-march=native` or `-maes -msse4.1` | AES-NI (and VAES with `-mvaes -mavx2`) | compile time |
| x86-64, GCC / Clang, default flags | VAES + AVX2 if the CPU has them (not on Windows), else AES-NI, else portable C | run-time check, cached (`HASSH_NO_DISPATCH` turns it off, `HASSH_NO_V3` keeps it to AES-NI) |
| Freestanding (`-ffreestanding`, kernels) | portable C unless built with `-maes -msse4.1` | no run-time check, no library calls, no `<string.h>` |
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

**Speed:** SMHasher3 `--test=Speed`, pinned to one core, every row measured on 2026-11-06 in
one session on the same build. "2.0 lanes only" is 2.0 without the LeMac absorber, for comparison.

| hash | SMHasher3 result | small keys 1–31 B (cycles/hash, lower is better) | bulk 256 KiB (bytes/cycle, higher is better) |
|---|---|---:|---:|
| **hassh64 2.0** | **pass** | **16.5** | **30.4** |
| hassh64 2.0, lanes only | pass | 16.6 | 23.3 |
| a5hash | pass | 18.1 | 3.0 |
| MuseAir.bfast | pass | 18.7 | 10.5 |
| rapidhash | pass | 28.1 | 13.1 |
| MeowHash.64 | pass | 54.2 | 16.7 |
| XXH3-64 | fails 27 tests | 28.1 | 19.7 |
| gxhash-64 | fails 24 tests | 30.7 | 22.9 |

The bulk figure averages the 8 input alignments; 32-byte-aligned input reaches 35.9 bytes/cycle,
unaligned about 29 (unaligned 32-byte loads that split a cache line cost the LeMac loop).

Pass/fail for the other hashes is from SMHasher3's published results table.

**Inlined benchmark** (`make bench`, hassh64 2.0; keys not freshly written; the seed changes on
every call; ns per hash, latency | throughput):

| len | hassh64 | xxh3 | rapidhash | wyhash | gxhash |
|---:|---|---|---|---|---|
| 8 | 4.26 \| 1.07 | 4.44 \| 1.30 | 3.94 \| 0.91 | 5.14 \| 1.16 | 5.02 \| 0.59 |
| 32 | 6.42 \| 1.95 | 4.71 \| 1.68 | 5.32 \| 1.38 | 5.20 \| 1.52 | 6.56 \| 0.79 |
| 64 | 6.32 \| 2.31 | 6.12 \| 2.63 | 8.23 \| 2.38 | 6.80 \| 2.27 | 9.02 \| 1.03 |
| 128 | 11.64 \| 1.78 | 7.47 \| 4.38 | 8.41 \| 3.84 | 9.79 \| 3.89 | 9.49 \| 2.00 |
| 256 | 13.65 \| 3.00 | 18.33 \| 9.83 | 12.08 \| 6.58 | 12.95 \| 7.04 | 12.63 \| 2.81 |
| 512 | 16.93 \| 5.37 | 19.97 \| 11.01 | 19.87 \| 12.83 | 22.41 \| 14.88 | 14.59 \| 4.86 |
| 256 KiB | 115 GB/s | 75 GB/s | 48 GB/s | 30 GB/s | 95 GB/s |

At exactly 4096 bytes, where LeMac starts, its fixed finalization cost shows: 58 GB/s, against
75 GB/s at 1 KiB and 109 GB/s at 16 KiB (the 256 KiB row was re-measured on 2026-11-06; the
short-key rows are from 2026-11-05 and unchanged, since inputs under 4096 bytes did not change).

Against 1.x: bulk +29% from the lanes alone, and throughput for 128–512-byte keys about 40% better (128 B: 2.85 →
1.78 ns, 256 B: 5.21 → 3.00 ns). With a prepared seed (`hassh64_keyed`), 65–256-byte keys
hashed back to back are 17–28% faster than 1.x.

### How far to trust these numbers

* **One CPU.** Everything was measured on a single Intel machine. AMD, older Intel and ARM may
  rank the hashes differently. Without VAES (Intel before Ice Lake, AMD before Zen 3), bulk
  speed roughly halves. ARM was checked for correctness under QEMU, never for speed.
* **The SMHasher3 small-key win is partly harness-specific.** That test writes the key and then
  hashes it straight away. hassh64 is built to avoid the store-forwarding stall this causes
  (a5hash does the same). With keys that were not just written, the inlined benchmark shows
  rapidhash and xxh3 slightly faster on 8–32 bytes.
* **65–128-byte latency is still the weak spot** when the seed changes on every call (11.3–11.7
  ns, against 6.9–10.9 ns for the others). With a fixed seed it is about 10 ns.
* SMHasher3 "cycles" are TSC ticks, not core clock cycles. They are only comparable within
  one session on one machine, which is how every row above was measured.
* The inlined benchmark was written by the same AI that wrote the hash. The gxhash numbers in
  it use a C port of gxhash taken from SMHasher3, not the Rust crate.

## When to use something else

* **Untrusted keys on a server** (HashDoS): use SipHash or another keyed PRF.
* **gxhash** is slower in bulk than hassh64 2.0 here (22.9 vs 30.4 bytes/cycle) but has
  higher throughput on short keys, but it fails 24 SMHasher3
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

**2.x trades some margin for speed on 65–4095 bytes.** With 3 AES rounds per block, the best
3-round differential an AI reviewer found gives fixed pairs of inputs of 65–4095 bytes that
collide for about 2^-51 of seeds, against the ideal 2^-64. That is still far too rare to
exploit (an attacker would need around 2^51 tries against one seed), but 1.x, with 4 rounds per
block, had no such pairs. If that margin matters to you, stay on 1.x (the `v1` branch).

**From 4096 bytes** hassh64 uses LeMac's step function, whose designers proved (by MILP, in a
peer-reviewed paper that was later corrected once) that any differential trail to a collision of
the state has at least 26 active S-boxes. hassh64's own additions around it (the seed-derived
start, two interleaved states, the compression of the final state to 64 bits) have only had AI
review; `test/longstats.c` checks them statistically.

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
  AES rounds of `K_i ^ (s, fold(s))`, which depend on the seed only, so `hassh64_keyed`
  precomputes them. Each 16-byte block runs `acc = P(acc ^ block)`, where P is 3 AES rounds, so
  a data difference passes at least 9 active S-boxes before it can meet another. The lanes are
  XORed together with a length vector (two AES rounds over two seed-and-length folds), then get
  3 more rounds and a 64-bit fold. Those 3 rounds remove the XOR's statistical linearity, but
  they are a public bijection and add no collision resistance. Inputs over 256 bytes use
  256-byte stripes, the last aligned to the end.
* **4096+ bytes, LeMac.** Two independent states of LeMac ([Bariant, David, Leurent, ToSC
  2024](https://tosc.iacr.org/index.php/ToSC/article/view/11619), message schedule as corrected
  in [ToSC 2025](https://tosc.iacr.org/index.php/ToSC/article/view/12089)), nine 128-bit words
  each, take alternate 16-byte words of each 128-byte step: 8 AES rounds per 64 bytes, 2 per 16
  bytes instead of the lanes' 3. One 256-bit VAES register holds the same word of both states.
  After four blank steps every word passes 4 AES rounds under its own key (two different
  nonzero word differences then cancel with probability at most the 4-round AES bound, about
  2^-113), the 18 words are XORed with the length vector, and 3 rounds remove the linearity
  between the two states. The fixed cost of that finalization is why it starts at 4096 bytes.

Things that were tried and fail:

1. **1 or 2 AES rounds per block** fail SMHasher3's Sparse test with real 64-bit collisions:
   a difference through 1 or 2 rounds is predictable enough that two blocks cancel. 2 rounds
   would have been 68% faster in bulk than 1.x.
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
make test        # known-answer vectors on every x86 path, ISA cross-check, ASan bounds, attack suites,
                 # LeMac step vs. the reference code, long-input statistics (quick)
build/longstats  # the long-input statistics at full size (about a minute)
make test-cross  # vectors on i386, musl, s390x, ARM, AArch64 (zig + QEMU_DIR with qemu-s390x/arm/aarch64)
make test-arm    # full suite on AArch64 under QEMU
./fetch-deps.sh  # clones xxHash, rapidhash, wyhash and SMHasher3 into deps/
python3 ports/python/hassh64.py   # the spec-only Python port against the vectors
make bench       # the inlined benchmark above
```

For SMHasher3, copy `smhasher3/hassh.cpp` into `deps/smhasher3/hashes/`, add it to
`hashes/Hashsrc.cmake`, build, and run `./SMHasher3 --extra hassh-64`.

## Versioning

**Stability promise: the 2.x output is final.** No 3.0 is planned. Speed work, new ports and new
instruction-set paths ship as 2.x releases and never change a single output bit; `test/vectors.h`
and the Rust crate's tests enforce that on every build. Only a published break of a documented
property (a practical seed-independent collision attack or a failing SMHasher3 test) could lead
to a 3.0, and even then 2.x would stay maintained and available, so hashes you store today keep
their meaning.

The output for every input is frozen for all 2.x releases, pinned by `test/vectors.h`. Any
output change, on any input, means a new major version. `HASSH_VERSION_MAJOR` tells you which
one you have. 2.x and 1.x agree for inputs of up to 64 bytes and differ above that; 1.x lives on
the [`v1` branch](https://github.com/zsigisti/hassh64/tree/v1) (its [1.0.1 release](https://github.com/zsigisti/hassh64/releases/tag/v1.0.1)); `main` is 2.x.

## License

MIT. See [LICENSE](LICENSE).
