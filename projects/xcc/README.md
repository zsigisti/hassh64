# xcc

Small C compiler (x86-64, AArch64, RISC-V and WebAssembly backends),
https://github.com/tyfkda/xcc.

* **Upstream base:** `d7bb34e5bd1f159c04545706f744f1fad226415e` (main)
* **Fork branch:** https://github.com/zsigisti/xcc/tree/hassh64
* **Pull request:** none
* **Header goes to:** `src/util/hassh.h`

Two commits:

1. **Reserve frame space for initialized flexible array member in wcc**
   (`src/wcc/gen_wasm.c`). An existing bug, also in upstream: wcc's frame layout
   ignored the initializer of a local struct with a flexible array member, so
   valtest's `struct FlexibleArrayMember local = {111, {222,333,444,555}}` wrote past
   its frame onto data address 8192, the first global. Upstream only gets away with it
   because that address holds an already-printed string. The fix sizes the slot as the
   native backend's `alloc_locals_onto_stack_frame()` does. Worth sending upstream on
   its own.
2. **Use hassh64 for the name table hash** (`src/util/table.c`): FNV-1a 32-bit
   replaced by hassh64 truncated to 32 bits. The new hash moved which global lands at
   address 8192, which is how the bug above surfaced.

## Results

Whole compiles (taskset, best of 15): sqlite3.c amalgamation `xcc -c` 571.7 -> 569.5 ms;
xcc's own 38 sources 278.7 -> 278.5 ms. No measurable change: half of all identifiers are
1-4 bytes, where hassh64 is slower (5.0 vs 4.1 ns), cancelling its gain on longer names
(9-16 B: 6.4 -> 2.9 ns). Over all 207k identifier tokens of sqlite3.c: 6.2 -> 5.9 ns.
Binary size: +19 KB (gcc build), +57 KB (self-hosted).

## Verified

`make test`, `test-gen2`, `test-ssa`, `test-wcc`, `test-wcc-gen2`: the same 229 suites OK
before and after. The assembly generated for sqlite3.c is identical apart from local
label numbers. (`make diff-gen23` fails on unmodified upstream too.)
