# hassh64 in a kernel

`hassh.h` needs nothing from the kernel but `<stdint.h>` and `<stddef.h>`
(which GCC and Clang provide even with `-nostdinc -isystem $(cc
-print-file-name=include)`). Copy it into your tree and include it:

```c
#include "hassh.h"

#define NAME_SEED 0x6b65726e656c0001ull   /* any constant; or a boot-time random value */

static inline size_t name_hash(const char *name, size_t len) {
    return (size_t)hassh64(name, len, NAME_SEED);
}
```

What it does under `-ffreestanding` (GCC and Clang), with no defines needed:

* **No library calls.** Loads go through `__builtin_memcpy`, which inlines even
  under `-fno-builtin`. The object file has no undefined symbols on x86-64,
  AArch64, RISC-V, i686, RV32 and ARMv7 (checked by `make check` here). The
  only thing your distro compiler may add is `__stack_chk_fail` if it
  defaults to `-fstack-protector`; kernels pass `-fno-stack-protector` or
  provide it.
* **No SIMD.** Without `-maes` it is portable C on general-purpose registers
  only, so it builds with `-mgeneral-regs-only` / `-mno-sse` and never
  switches to AES-NI at run time (a kernel may not save SSE state). If your
  kernel does enable SSE and saves it, build with `-maes -msse4.1` for the
  AES-NI path; the output is the same.
* **Same output** as every other build, pinned by the 548 test vectors.

Speed without SSE (Arrow Lake, one core, ns per hash, each hash feeding the
next): 4 bytes 3.6 vs FNV-1a 5.0; 16 bytes 4.6 vs 14.1; 64 bytes 6.0 vs 50.
Over 64 bytes the portable AES path runs: 128 bytes 126 ns (82 with the
prepared-seed API below) vs FNV-1a 98; 4 KiB 1.8 us vs 3.1 us.

## Long keys: prepare the seed once

If keys can be longer than 64 bytes (path names, file contents), derive the
seed's lane starts once at boot and hash through the key. It is about 1 KiB:

```c
static hassh64_key path_key;          /* in .bss */

void vfs_hash_init(uint64_t seed) { hassh64_key_init(&path_key, seed); }

uint64_t path_hash(const char *p, size_t len) {
    return hassh64_keyed(&path_key, p, len);   /* == hassh64(p, len, seed) */
}
```

## Rust kernels

```toml
[dependencies]
hassh64 = { version = "2", default-features = false }   # no_std, no allocator
```

```rust
const NAME_SEED: u64 = 0x6b65726e656c0001;
fn name_hash(name: &[u8]) -> u32 { hassh64::hassh64(name, NAME_SEED) as u32 }

// Or as the hasher for hashbrown / alloc maps:
type Map<K, V> = hashbrown::HashMap<K, V, hassh64::Hassh64BuildHasher>;
```

On a soft-float target such as `x86_64-unknown-none` the crate uses its
portable path (no SIMD instructions), with the same output.

## Check your build

`make check` in this folder compiles `kernel_check.c` with kernel flags for
six targets, fails on any undefined symbol or (x86-64) any SSE/AES
instruction, and runs the 548 vectors from a freestanding build.

Before swapping an existing hash, read
[When hassh64 is the wrong choice](../README.md#when-hassh64-is-the-wrong-choice).
