# Contributing

Bug reports, ports to other languages, benchmark results from other CPUs, and analysis are all
welcome.

## Ground rules

- **The 2.x output is frozen.** `test/vectors.h` must keep passing. A change that alters any hash
  value is a 3.0 discussion, not a pull request.
- **Every path must agree.** Changes to `hassh.h` must pass `make test` (all x86 paths, sanitizers,
  attack suites) and, when possible, `make test-cross` and `make test-arm`.
- **Benchmarks need context:** CPU model, compiler and flags, and the exact command. Run all
  compared hashes in the same session; machine speed drifts between days.

## Ports

A port is correct when it reproduces every entry in `test/vectors.h` (message: byte `i` is
`(i * 31 + 7) mod 256`). Follow [`SPEC.md`](SPEC.md); `ports/python/hassh64.py` is a readable
reference. Open an issue if the spec is unclear anywhere.

## Testing

```
make test         # x86: vectors on every path, cross-check, ASan bounds, attack suites
make test-cross   # vectors on i386, musl, s390x, ARM, AArch64 (needs zig and qemu-user)
make cli          # build the command-line tool
```
