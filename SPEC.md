# hassh64 2.x specification

This document defines hassh64 completely, so it can be implemented in any language without
reading `hassh.h`. An implementation is correct when it reproduces every entry of
`test/vectors.h`. The message for each vector is the first `len` bytes of
`byte[i] = (i * 31 + 7) mod 256`.

## Notation

- All integers are unsigned 64-bit; `+`, `*` and `<<` wrap modulo 2^64. `^` is XOR, `|` is OR.
- Precedence as in C: `*`, then `+`/`-`, then `<<`/`>>`, then comparisons, then `&`, `^`, `|`. So
  `r16(0) | p[2] << 16` is `r16(0) | (p[2] << 16)`. Bytes are unsigned and zero-extended to
  64 bits before any shift.
- `len` is the input length in bytes, as a 64-bit integer. `p[i]` is input byte `i`.
- `r64(i)`: the 8 bytes `p[i..i+8)` as a little-endian integer. `r32(i)`: 4 bytes. `r16(i)`: 2 bytes.
- `mul(a, b)` → `(lo, hi)`: the full 128-bit product of `a` and `b`, split into its low and high
  64-bit halves.
- `fold(a, b) = lo ^ hi` where `(lo, hi) = mul(a, b)`.

## Constants

`P[0..7]`: the SHA-512 initial hash values.

```
P = 6a09e667f3bcc908 bb67ae8584caa73b 3c6ef372fe94f82b a54ff53a5f1d36f1
    510e527fade682d1 9b05688c2b3e6c1f 1f83d9abfb41bd6b 5be0cd19137e2179
```

`K[0..63]`: the first 64 of the 80 SHA-512 round constants, in order.

```
428a2f98d728ae22 7137449123ef65cd b5c0fbcfec4d3b2f e9b5dba58189dbbc
3956c25bf348b538 59f111f1b605d019 923f82a4af194f9b ab1c5ed5da6d8118
d807aa98a3030242 12835b0145706fbe 243185be4ee4b28c 550c7dc3d5ffb4e2
72be5d74f27b896f 80deb1fe3b1696b1 9bdc06a725c71235 c19bf174cf692694
e49b69c19ef14ad2 efbe4786384f25e3 0fc19dc68b8cd5b5 240ca1cc77ac9c65
2de92c6f592b0275 4a7484aa6ea6e483 5cb0a9dcbd41fbd4 76f988da831153b5
983e5152ee66dfab a831c66d2db43210 b00327c898fb213f bf597fc7beef0ee4
c6e00bf33da88fc2 d5a79147930aa725 06ca6351e003826f 142929670a0e6e70
27b70a8546d22ffc 2e1b21385c26c926 4d2c6dfc5ac42aed 53380d139d95b3df
650a73548baf63de 766a0abb3c77b2a8 81c2c92e47edaee6 92722c851482353b
a2bfe8a14cf10364 a81a664bbc423001 c24b8b70d0f89791 c76c51a30654be30
d192e819d6ef5218 d69906245565a910 f40e35855771202a 106aa07032bbd1b8
19a4c116b8d2d0c8 1e376c085141ab53 2748774cdf8eeb99 34b0bcb5e19b48a8
391c0cb3c5c95a63 4ed8aa4ae3418acb 5b9cca4f7763e373 682e6ff3d6b2b8a3
748f82ee5defb2fc 78a5636f43172f60 84c87814a1f0ab72 8cc702081a6439ec
90befffa23631e28 a4506cebde82bde9 bef9a3f7b2c67915 c67178f2e372532b
```

`L[0..71]`: the first 64 bits of the fractional parts of the cube roots of the 81st to 152nd
primes, in order (SHA-512's 80 round constants come from the first 80 primes; `L` continues them).

```
7ba0ea2d98160007 7eabf2d0c21f964a 8dbe8d038b409545 90bb1721582e8285
99a2ad45936d4e61 9f86e289fe03e739 a84c4472faa9a82f b3df34fce89e0532
b99bb8d7b173534f bc76cbab1aea1f9c c226a69a780f3cc3 d304f19aa233957d
de1be20a212129dd e39bb43755141950 ee84927cea48ddd2 f3edd2773c523b67
fbfdfe53a8d32f2a 0bee2c7ab77e9e25 0e90181cf1b09e56 25f57204c725bed8
2da45582cd598b32 3a52c34c203bfcf3 41dc0172cd1991c1 495796fcb33cc1c0
4bd31fc693f9f16e 533cde2115f5a9a0 5f7abfe36e99c1d3 66c206b310a57e6f
6dfcc6bc39603f61 7062f20f86fd1052 778d51277adec865 7eaba3cc25da7048
8363eccc37a5be05 85be1c253beba54e 93c04028f348bbc5 9f4a205fd05b2148
a19535651ca6d2de a627bb0fbf027bc7 acfa80891da2f06b b3c29b23031a7f9d
b602f6fac7d3d74d c36cee0a10c7ba49 c7dc81eea9ebad4f ce7b8471b0f809df
d740288c84df269c e21dba7ac2290607 eabbff66be175964 f56a9e60f62cea92
fde41d729d126eab 0434d0970e42e781 0a7cb752a3f1cd86 0ea7d22d6bcd7382
16f2987f9495a5ee 1d20cdcd45b8de1e 213af85a39b0c320 2964505c52a2f35b
2d738e114181e082 3b8cea0e71c58aaf 4584e6ae9f54016e 515f4356903dccc2
5356112ddfd5a8e9 5d1bc3edbe2c897a 5f0da9f8ed53548b 62ef0be4d5492e78
64de896eace0be7f 6e801ba3078ae05f 7bdb3595cdadf50a 7fa5377856834c98
818916bad3d008a8 854e959f834021a7 926a82c27137e2c6 9622c7ba7d179197
```

## Seed keys (every length)

```
s  = seed ^ fold(seed ^ P[0], P[1])
s2 = fold(s ^ len ^ P[6], P[7])
t  = (s ^ P[5] ^ (len << 56)) | 1
```

## Length 0 to 16

Build two words `a` and `b`:

| len | a | b |
|---|---|---|
| 0 | 0 | 0 |
| 1 | `p[0]` | 0 |
| 2 | `r16(0)` | 0 |
| 3 | `r16(0) \| p[2] << 16` | 0 |
| 4 | `r32(0)` | 0 |
| 5..7 | `r32(0)` | `tail3(4, len - 4)` |
| 8 | `r64(0)` | 0 |
| 9..11 | `r64(0)` | `tail3(8, len - 8)` |
| 12..16 | `r64(0)` | `r32(8) \| r32(len - 4) << 32` (a 64-bit shift) |

where `tail3(i, n) = p[i] << 16 | p[i + (n >> 1)] << 8 | p[i + n - 1]` (for `n` = 1..3).

```
(lo, hi) = mul(a ^ P[2] ^ s, b ^ P[3] ^ s2 ^ len)
return fold(lo ^ P[4], hi ^ t)
```

## Length 17 to 32

```
w0 = r64(0)
w1 = r64(8)
w2 = r64(len < 20 ? 4 : len - 16)      (overlapping reads are intended)
w3 = r64(len - 8)
x  = fold(w0 ^ P[2] ^ s, w1 ^ P[3] ^ s2)
y  = fold(w2 ^ P[4] ^ s, w3 ^ P[5] ^ s2)
a  = fold(x ^ P[6], y ^ P[7] ^ len)
return fold(a ^ P[4], t)
```

## Length 33 to 64

```
s3 = s ^ s2
x0 = fold(r64(0)        ^ P[2] ^ s,  r64(8)        ^ P[3] ^ s2)
x1 = fold(r64(16)       ^ P[4] ^ s,  r64(24)       ^ P[5] ^ s2)
x2 = fold(r64(len - 32) ^ P[6] ^ s3, r64(len - 24) ^ P[7] ^ s)
x3 = fold(r64(len - 16) ^ P[0] ^ s3, r64(len - 8)  ^ P[1] ^ s)
return fold(x0 ^ x2 ^ P[4], x1 ^ x3 ^ t)
```

## Length 65 and up (AES)

Lengths 65..4095 use the lanes below; 4096 and up use the LeMac absorber (next section).
Both share the block definitions and `lv`.

### Blocks and the AES round

A block is 16 bytes. `blk(lo, hi)` is the block whose bytes 0..7 are `lo` little-endian and
bytes 8..15 are `hi` little-endian. `ld(i)` is the block `p[i..i+16)`. `key(j) = blk(K[2j], K[2j+1])`.

`aesenc(x, k)` is one AES encryption round as in FIPS-197, and identical to the x86 `AESENC`
instruction: `MixColumns(ShiftRows(SubBytes(x))) ^ k`. Byte `4c + r` of a block is row `r`,
column `c` of the AES state. Explicitly, with the AES S-box `S` and `xt(v) = (v << 1) ^ (v >> 7) * 0x1b`
(in 8 bits):

```
u[4c + r] = S[x[4((c + r) mod 4) + r]]                  for c, r in 0..3   (SubBytes + ShiftRows)
for each column c, with a_r = u[4c + r] and all = a0 ^ a1 ^ a2 ^ a3:
    out[4c + r] = a_r ^ all ^ xt(a_r ^ a_(r+1 mod 4)) ^ k[4c + r]          (MixColumns + key)
```

Check your round against the Intel AESENC example: state `7b5b54657374566563746f725d53475d`,
round key `48692853686179295b477565726f6e5d` (both as bytes 0..15 in hex) give
`a8311c2f9fdba3c58b104b58ded7e595`.

Derived functions:

```
perm(x) = aesenc(aesenc(aesenc(x, key(16)), key(17)), key(18))
fin(x)  = aesenc(aesenc(aesenc(x, key(24)), key(25)), key(26))
sv      = blk(s, fold(s ^ P[2], P[3]))
init(i) = aesenc(aesenc(key(i) ^ sv, key(30)), key(31))
lv      = aesenc(aesenc(blk(fold(s ^ len ^ P[4], P[5]), fold(s ^ len ^ P[6], P[7])), key(30)), key(31))
out(x)  = lo ^ hi, where x = blk(lo, hi)
```

### Lanes

Lane `i` starts as `acc[i] = init(i)`. Absorbing block `d` into lane `i` means
`acc[i] = perm(acc[i] ^ d)`.

- **65..128 bytes**: 8 lanes. Lane `i` absorbs `ld(16i)` for `i` = 0..3, and `ld(len - 16(8 - i))`
  for `i` = 4..7.
- **129..256 bytes**: 16 lanes. Lane `i` absorbs `ld(16i)` for `i` = 0..7, and
  `ld(len - 16(16 - i))` for `i` = 8..15.
- **257..4095 bytes**: 16 lanes, in 256-byte stripes:

  ```
  off = 0
  loop:
      if off + 256 > len: off = len - 256
      for i in 0..15: lane i absorbs ld(off + 16i)
      if off + 256 >= len: stop
      off = off + 256
  ```

  The final stripe is aligned to the end of the input and may overlap the previous one.

Finally:

```
x = acc[0] ^ acc[1] ^ ... ^ acc[lanes - 1] ^ lv
return out(fin(x))
```

### Length 4096 and up (LeMac)

Two independent states, each of nine blocks `S[0..8]` plus four blocks `RR, R0, R1, R2`. The step
function is LeMac's (Bariant, David, Leurent, ToSC 2024; message schedule as corrected in ToSC
2025), unchanged:

```
step(M0, M1, M2, M3):            (all old values on the right-hand side)
    S'[0] = S[0] ^ S[8] ^ M2
    S'[1] = aesenc(S[0], M3)
    S'[2] = aesenc(S[1], M3)
    S'[3] = aesenc(S[2], R1 ^ R2)
    S'[4] = aesenc(S[3], M0)
    S'[5] = aesenc(S[4], M0)
    S'[6] = aesenc(S[5], M1)
    S'[7] = aesenc(S[6], M1)
    S'[8] = aesenc(S[7], M3)
    R2' = R1,  R1' = R0,  R0' = RR ^ M1,  RR' = M2
```

Note the message words are the *round keys* of `aesenc`.

With `lkey(w) = blk(L[2w], L[2w+1])` (w = 0..35):

```
state j (j = 0, 1):
    S[i] = aesenc(aesenc(lkey(9j + i) ^ sv, key(30)), key(31))     for i = 0..8
    RR = R0 = R1 = R2 = 0

nsteps = ceil(len / 128)
for k in 0 .. nsteps - 1:
    q = 128k if k < nsteps - 1 else len - 128       (the final step is aligned to the end)
    for j in 0, 1:
        state j: step(ld(q + 16j), ld(q + 32 + 16j), ld(q + 64 + 16j), ld(q + 96 + 16j))

for j in 0, 1: state j: four blank steps, step(0, 0, 0, 0)

comp(w, x) = aesenc(aesenc(aesenc(aesenc(x ^ lkey(18 + w), key(16)), key(17)), key(18)), key(19))
x = lv
for j in 0, 1: for i in 0..8: x = x ^ comp(9j + i, S[i] of state j)
return out(fin(x))
```

State 0 absorbs the 16-byte words at offsets 0, 32, 64, 96 of every 128-byte step and state 1
those at 16, 48, 80, 112, so one 256-bit register can hold word `i` of both states. The states are
independent until the final XOR: an implementation may run them one after the other.

## Porting notes

- `mul` needs the full 128-bit product: `u128` in Rust, `bits.Mul64` in Go,
  `Math.unsignedMultiplyHigh` (Java 18+) in Java, BigInt in JavaScript.
- Java: use `>>>`, never `>>`, and mask bytes with `& 0xff`. JavaScript BigInt: mask every
  result with `& 0xffffffffffffffffn`, and the AES `xt` result with `& 0xff`.
- Rust: use `wrapping_*` operations; plain overflow panics in debug builds.
- Byte `4c + r` of a block is row `r`, column `c` (column-major). A row-major textbook layout
  gets ShiftRows wrong; the AESENC example above catches it.

## Versioning

The output for every input is frozen for all 2.x releases. 2.x differs from 1.x only for inputs
longer than 64 bytes: 3 rounds per block instead of 4, lane starts that depend on the seed alone,
the length vector `lv`, and the LeMac absorber from 4096 bytes. Any change to any output, on any
input, makes a new major version.

This specification is final. No 3.0 is planned: implementations written against this document
and its 548 vectors stay correct for every 2.x release. A new major version would only follow a
published break of a documented property, and 2.x would remain available alongside it.
