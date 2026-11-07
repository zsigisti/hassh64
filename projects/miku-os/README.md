# miku-os

Hobby x86-64 kernel in Rust, https://github.com/alunwrd/miku-os.

* **Upstream base:** `7af272ddc1d9b924fa77974fb5fc7d8db9517935` (main)
* **Fork branch:** https://github.com/zsigisti/miku-os/tree/hassh64-hash
* **Pull request:** https://github.com/alunwrd/miku-os/pull/2
* **Header:** none; uses the `hassh64` crate from crates.io (`version = "2.1"`,
  `default-features = false`, so `no_std`, no allocation, no SIMD)

`name_hash()` in `kernel/fs/vfs/hash.rs` places a name in its directory's
open-addressed table on every lookup, insert, negative entry and removal. It was
32-bit FNV-1a; the patch uses hassh64 (low 32 bits, fixed seed). The hash lives only
in the in-memory tables, never on disk or across the ABI.

## Results

Built like the kernel (`x86_64-unknown-none`, no SSE, opt-level 2, LTO), hassh64 2.1.1,
ns per hash, each input depending on the previous result:

| name length | 2 | 3 | 4 | 8 | 16 | 32 | 64 | 96 | 255 |
|---|---|---|---|---|---|---|---|---|---|
| FNV-1a | 2.9 | 3.9 | 5.0 | 8.0 | 13.9 | 25.3 | 49.8 | 72.9 | 191.6 |
| hassh64 | 3.8 | 4.2 | 3.9 | 4.2 | 4.7 | 6.9 | 6.0 | 149.8 | 274.4 |

The 916 distinct names on the built root disk (mean 15.3 B): 16.6 -> 9.6 ns chained,
4.1 -> 2.0 ns back to back. Costs: 2-3 B names 0.3-1 ns slower, names over 64 B about 2x
slower, kernel image +55 KB, `getdents` order changes.

## Verified

`run=vfs_test;ext_test;mm_test;tty_test;cred_test;elf_test` (headless KVM boot) gives
identical results before and after: vfs 107/107, ext 12/12, mm 11 + 2 skipped,
tty 67/67, cred 39 + 1 skipped, elf with only the known `elf/relro-outside-image`
failure. Shell checks give byte-identical output; a desktop boot reaches the compositor
and a DHCP lease.
