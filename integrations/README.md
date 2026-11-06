# Integrations

Ready-to-use packaging of hassh64 for specific kinds of projects. Every
integration uses the same `hassh.h` from the repository root and has a test
that checks it against the same frozen values, so a key hashed in a kernel, in
SQLite, in PostgreSQL, in C++, in Rust or in Python gives the same 64 bits.

| Folder | For | What you get | Test |
|---|---|---|---|
| [`kernel/`](kernel/) | OS kernels, bootloaders, firmware, any `-ffreestanding` code | `hassh.h` works as-is: no library calls, no `<string.h>`, no SSE/AES unless you enable it. Guide for C and Rust (`no_std`) kernels | `make -C integrations/kernel check` (x86-64, AArch64, RISC-V, i686, RV32, ARMv7) |
| [`sqlite/`](sqlite/) | SQLite | Loadable extension: `hassh64(value [, seed])` | `make -C integrations/sqlite test` |
| [`postgres/`](postgres/) | PostgreSQL 13+ | Extension: `hassh64(text\|bytea [, seed])` | `integrations/postgres/check.sh` (PG 18+, throwaway cluster) |
| [`cpp/`](cpp/) | C++ hash containers | `hassh::hash<T>`, `hassh::string_hash` for `std::unordered_map` / `unordered_set` | `make -C integrations/cpp test` |
| [`../rust/`](../rust/) | Rust | Crate [`hassh64`](https://crates.io/crates/hassh64): `BuildHasher`, `HashMap` aliases, `no_std` | `cargo test` |
| [`../ports/python/`](../ports/python/) | Python | Pure-Python port | `python3 ports/python/hassh64.py` |

`make integrations` from the repository root runs all of them that have their
tools installed.

## When hassh64 is the wrong choice

Measured while trying it in real projects, so you don't have to:

* **The hash value is stored or specified.** On-disk directory hashes,
  protocol fields, partitioning that other systems must reproduce: those need
  the exact function they were defined with.
* **Integer keys in a `hash % size` table.** FNV-1a and identity-like hashes
  happen to spread sequential integers (PIDs, offsets, IPv4 addresses) with no
  collisions at all; hassh64, like any good hash, gives random-function
  collisions instead, and the extra probes can cost more than the faster hash
  saves. Use it for byte strings, or for integer keys that are not sequential.
* **Hostile keys with a public seed.** It is not a defence against hash
  flooding when attackers know the seed (see the main README, Security).
* **The table is never hot.** Swapping a 10-line FNV for a 1,100-line header
  in code that hashes a few names per second adds code (about 17 KiB of text
  for the portable long-input path) without a visible gain.
