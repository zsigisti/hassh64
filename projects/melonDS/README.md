# melonDS

Nintendo DS emulator, https://github.com/melonDS-emu/melonDS (C++).

* **Upstream base:** `497aa6cdd26f45f8064b78916b00a773eab4ded7` (master)
* **Fork branch:** https://github.com/zsigisti/melonDS/tree/hassh64
* **Pull request:** https://github.com/melonDS-emu/melonDS/pull/2797
* **Header goes to:** `src/hassh/hassh.h`

`MaskedHash()` in `src/GPU3D_Texcache.h` hashes a texture or palette range when a
cache entry is created and again when its VRAM range is marked dirty. The patch
switches it from XXH64 to hassh64, still chaining wrapped ranges by passing the
previous hash as the seed. The values only live in memory (not in savestates).

## Results

`MaskedHash` on random VRAM contents, best of 7, one core (Core Ultra 7 265KF),
Release `-O3`, no `-march`:

| bytes | XXH64 | hassh64 | XXH3 (seeded, already bundled) |
|---|---|---|---|
| 8 | 2.3 ns | 2.6 ns | 1.7 ns |
| 32 | 5.1 ns | 3.5 ns | 2.2 ns |
| 512 | 24.3 ns | 14.6 ns | 12.5 ns |
| 2 KiB | 85.8 ns | 42.7 ns | 44.9 ns |
| 8 KiB | 322 ns | 150 ns | 172 ns |
| 32 KiB | 1.27 us | 0.55 us | 0.68 us |
| 128 KiB | 5.06 us | 2.15 us | 2.71 us |

With `-march=native` (VAES): hassh64 118 GB/s at 128 KiB, XXH3 79 GB/s.

## Verified, and not

* The core library (`-DBUILD_QT_SDL=OFF`, Release) builds cleanly before and after;
  no warnings from the header under gcc and clang `-Wall -Wextra`.
* **Not run in a game:** the Qt frontend needs libenet (not installed) and no ROMs
  were available. End-to-end gain is probably small, since the dirty bitmap already
  limits how often `MaskedHash` runs.
