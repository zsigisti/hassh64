# Security

hassh64 is a **non-cryptographic** hash. It makes no claim of collision resistance against an
attacker and no claim of HashDoS resistance. Do not use it for passwords, MACs, signatures,
integrity checks against tampering, or hash tables keyed by untrusted input. Use SHA-256,
BLAKE3 or SipHash for those.

hassh64 was designed and reviewed by AI. No human cryptographer has analysed it.

## Reporting

Findings are welcome, especially inputs that collide for every seed (or for a large fraction of
seeds), seed recovery, or statistical failures in SMHasher3 or similar suites.

- Open a GitHub issue. Because hassh64 makes no security claim, public reports are fine.
- Include the inputs and seeds, or a program that produces them, and the hassh64 version.

A confirmed attack will be added to `test/seedindep.c` or `test/xlen.c` as a regression test and
documented in the README. A fix that changes the output is a new major version.
