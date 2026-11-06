# Experimental: x86-64-v3 / v4 kernels with run-time selection

**Not part of the stable header.** `hassh_x86.h` adds `hassh64_x86()`, which gives exactly the
same output as `hassh64()` but picks the fastest kernel the CPU supports at run time, even in
a build with default compiler flags:

| level | needs | lanes (65–4095 B) | LeMac (4096+ B) |
|---|---|---|---|
| v2 | AES-NI, SSE4.1 | `hassh.h` 128-bit path | `hassh.h` 128-bit path |
| v3 | + AVX2, VAES (OS saves YMM) | 8 ymm, intrinsics | ymm, inline asm (RR re-read from memory: 16 registers) |
| v4 | + AVX-512 F/BW/VL (OS saves ZMM) | 4 zmm, intrinsics, `vpternlogq` fold | ymm, inline asm with 32 registers (RR kept, `vpternlogq`) |

Detection reads CPUID and XGETBV once and caches the result.

## Results (Intel Core Ultra 7 265KF, x86-64-v3, no AVX-512)

`make bench`: built with plain `-O2` (no `-march`), so the stable header can only use its
run-time AES-NI path. Bytes per TSC tick, best of 9; throughput / latency (unaligned chain):

| len | `hassh64()` | `hassh64_x86()` v3 |
|---:|---:|---:|
| 256 | 6.8 / 4.8 | 11.4 / 5.0 |
| 1024 | 11.6 / 9.2 | 20.4 / 11.7 |
| 4096 | 14.0 / 13.7 | 23.4 / 19.9 |
| 64 KiB | 15.9 / 15.9 | 38.2 / 32.4 |
| 1 MiB | 17.1 / 16.0 | 34.3 / 29.1 |

So run-time v3 selection is worth 1.7–2.4x over 1 KiB in default-flags builds (distribution
packages, most Rust/C builds). The same kernels compiled with `-march=native` in `hassh.h`
already reach these numbers; this branch only makes them reachable without it.

**v4 speed is unmeasured.** No machine here has AVX-512. The v4 kernels are checked for
correctness under Intel SDE (Sapphire Rapids, Ice Lake server, Granite Rapids: `make
test-sde`), which does not model timing. Expectations, not measurements: on Intel server cores
and Zen 4 a 512-bit `vaesenc` has half the throughput of a 256-bit one, so the zmm lanes should
match v3; the LeMac kernel saves one load and one XOR per step. Zen 5 (full-width AES) is the
only likely real win, for the lanes.

## Not done / ideas

- Lanes in hand-written asm: the intrinsics (with the header's unroll pragmas) already run at
  ~84% of the 3-rounds-per-16-bytes throughput bound, so little is left there.
- LeMac's per-step latency, not AES throughput, limits one stream on this CPU (about 4.5
  cycles per 128-byte step, bound 4). Unaligned 32-byte loads that split cache lines cost the
  most; `prefetcht0` 1 KiB ahead recovered most of it for L2-resident data.
- Rust: no VAES path yet (`vaes` intrinsics need Rust 1.89; the crate's MSRV is 1.72). With
  `-C target-cpu=native` the Rust 128-bit LeMac path was slower (48 vs 67 GB/s) than the
  default build: worth a look.
- A v4 measurement on real hardware (CI runners are sometimes Ice Lake Xeons).
