# Ports into other projects

hassh64 applied to real projects, kept here so each port survives independently of
any fork or pull request. Each folder has the change as a patch against a named
upstream commit, the test results and benchmarks from when it was made, and the
pull request if one was opened.

```sh
git clone https://github.com/wren-lang/wren && cd wren
git checkout 99d2f0b8fc2686134b32b18166e037639f7e9f2c
../hassh64/projects/apply.sh wren .
```

`apply.sh` runs `git am` with the patch, then vendors the `hassh.h` of this checkout
(with its MIT notice) where the project keeps it. The patches leave the header out,
so a port always gets the hassh64 version you have.

| Project | What hassh64 hashes | Result | Fork branch | Upstream PR |
|---|---|---|---|---|
| [melonDS](melonDS/) (DS emulator, C++) | texture-cache invalidation, 8 B-128 KB of VRAM | `MaskedHash` 2.3x faster than XXH64 on texture sizes, 1.2x XXH3 (1.5x with VAES); not run in a game | [hassh64](https://github.com/zsigisti/melonDS/tree/hassh64) | [#2797](https://github.com/melonDS-emu/melonDS/pull/2797) |
| [wren](wren/) (scripting language, C) | every new string, map lookups | hash 1.2-10x faster on real keys; `map_string` -3.2%; all 866 tests, output byte-identical | [hassh64](https://github.com/zsigisti/wren/tree/hassh64) | [#1252](https://github.com/wren-lang/wren/pull/1252) |
| [miku-os](miku-os/) (Rust kernel) | VFS directory-entry names | names 16.6 -> 9.6 ns; selftests identical; names > 64 B about 2x slower; +55 KB | [hassh64-hash](https://github.com/zsigisti/miku-os/tree/hassh64-hash) | [#2](https://github.com/alunwrd/miku-os/pull/2) |
| [xcc](xcc/) (C compiler) | identifier table | no measurable compile-time change (half the identifiers are 1-4 B); found and fixes a wcc frame-layout bug | [hassh64](https://github.com/zsigisti/xcc/tree/hassh64) | none |

Projects that were tried and left out, and why, are listed in
[`integrations/README.md`](../integrations/README.md#when-hassh64-is-the-wrong-choice)
(stored or specified hash values, sequential integer keys, tables that are never hot).
