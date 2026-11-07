# wren

Small scripting language, https://github.com/wren-lang/wren (C).

* **Upstream base:** `99d2f0b8fc2686134b32b18166e037639f7e9f2c` (main)
* **Fork branch:** https://github.com/zsigisti/wren/tree/hassh64
* **Pull request:** https://github.com/wren-lang/wren/pull/1252
* **Header goes to:** `src/vm/hassh.h` (also listed in the vs2017/vs2019 project files)

`hashString()` in `src/vm/wren_value.c` hashed every new string a byte at a time with
32-bit FNV-1a. The patch uses hassh64 (seed `0x9e3779b97f4a7c15`) folded to the 32 bits
`ObjString` stores. Numbers and other values hash as before.

## Results

Hash alone, gcc -O2, ns per key:

| keys | FNV-1a | hassh64 |
|---|---|---|
| identifiers in `wren_core.wren` (6.7 B) | 2.08 | 1.72 |
| identifiers in `test/` (10.6 B) | 3.23 | 1.89 |
| `map_string` keys (22.8 B) | 8.24 | 2.65 |
| 44 B strings | 19.44 | 3.04 |
| 132 B strings | 76.84 | 7.80 |
| 1-4 B identifiers | 1.31 | 1.60 |

`test/benchmark`, release_64bit, best of 15 interleaved runs: `map_string` 71.7 -> 69.4 ms
(-3.2%); `binary_trees_gc` -5.3% (barely creates strings, likely code layout); the others
within +-2%.

## Verified

All 866 tests (3861 expectations) pass in the four CI configurations and `release_32bit`
(the portable C path). The raw output of all 886 test scripts is byte-identical before
and after, so map iteration order is not observable in the tests.
