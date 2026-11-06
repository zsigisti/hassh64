"""hassh64 2.x reference implementation in pure Python.

AI-generated: written by an AI agent from SPEC.md alone, without reading hassh.h, as a
check that the spec is complete. Slow (software AES); meant for porting and testing,
not production. Run directly to check it against test/vectors.h:

    python3 ports/python/hassh64.py

    from hassh64 import hassh64
    hassh64(b"hello", 0)
"""
import os, re, sys

M = (1 << 64) - 1

P = [int(x, 16) for x in """6a09e667f3bcc908 bb67ae8584caa73b 3c6ef372fe94f82b a54ff53a5f1d36f1
510e527fade682d1 9b05688c2b3e6c1f 1f83d9abfb41bd6b 5be0cd19137e2179""".split()]
K = [int(x, 16) for x in """
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
90befffa23631e28 a4506cebde82bde9 bef9a3f7b2c67915 c67178f2e372532b""".split()]
assert len(P) == 8 and len(K) == 64
L = [int(x, 16) for x in """
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
818916bad3d008a8 854e959f834021a7 926a82c27137e2c6 9622c7ba7d179197""".split()]
assert len(L) == 72


def _sbox():
    # Derive the AES S-box: multiplicative inverse in GF(2^8) mod 0x11b, then affine map.
    def gmul(a, b):
        r = 0
        while b:
            if b & 1:
                r ^= a
            a = ((a << 1) ^ (0x11B if a & 0x80 else 0)) & 0xFF
            b >>= 1
        return r
    inv = [0] * 256
    for a in range(1, 256):
        for b in range(1, 256):
            if gmul(a, b) == 1:
                inv[a] = b
                break
    s = []
    for a in range(256):
        x = inv[a]
        y = x
        for k in range(1, 5):
            y ^= ((x << k) | (x >> (8 - k))) & 0xFF
        s.append(y ^ 0x63)
    return s


S = _sbox()
assert S[0] == 0x63 and S[1] == 0x7C and S[0x53] == 0xED


def mul(a, b):
    p = a * b
    return p & M, p >> 64


def fold(a, b):
    lo, hi = mul(a, b)
    return lo ^ hi


def xt(v):
    return ((v << 1) & 0xFF) ^ ((v >> 7) * 0x1B)


def blk(lo, hi):
    return lo.to_bytes(8, "little") + hi.to_bytes(8, "little")


def bxor(x, y):
    return bytes(a ^ b for a, b in zip(x, y))


def aesenc(x, k):
    u = [S[x[4 * ((c + r) % 4) + r]] for c in range(4) for r in range(4)]
    out = bytearray(16)
    for c in range(4):
        a = u[4 * c:4 * c + 4]
        al = a[0] ^ a[1] ^ a[2] ^ a[3]
        for r in range(4):
            out[4 * c + r] = a[r] ^ al ^ xt(a[r] ^ a[(r + 1) % 4]) ^ k[4 * c + r]
    return bytes(out)


def key(j):
    return blk(K[2 * j], K[2 * j + 1])


def lkey(w):
    return blk(L[2 * w], L[2 * w + 1])


def lemac(p, n, sv, lv, k16, k17, k18, k19, k30, k31):
    """LeMac absorber for n >= 4096; returns the pre-fin block x."""
    Z = bytes(16)
    nsteps = -(-n // 128)
    x = lv
    for j in range(2):
        S = [aesenc(aesenc(bxor(lkey(9 * j + i), sv), k30), k31) for i in range(9)]
        RR = R0 = R1 = R2 = Z

        def step(M0, M1, M2, M3):
            nonlocal S, RR, R0, R1, R2
            S = [bxor(bxor(S[0], S[8]), M2), aesenc(S[0], M3), aesenc(S[1], M3),
                 aesenc(S[2], bxor(R1, R2)), aesenc(S[3], M0), aesenc(S[4], M0),
                 aesenc(S[5], M1), aesenc(S[6], M1), aesenc(S[7], M3)]
            R2, R1, R0, RR = R1, R0, bxor(RR, M1), M2

        for k in range(nsteps):
            q = 128 * k if k < nsteps - 1 else n - 128
            o = q + 16 * j
            step(p[o:o + 16], p[o + 32:o + 48], p[o + 64:o + 80], p[o + 96:o + 112])
        for _ in range(4):
            step(Z, Z, Z, Z)
        for i in range(9):
            c = aesenc(aesenc(aesenc(aesenc(bxor(S[i], lkey(18 + 9 * j + i)), k16), k17), k18), k19)
            x = bxor(x, c)
    return x


def hassh64(p, seed=0):
    """64-bit hash of the bytes-like p with a 64-bit seed."""
    p = bytes(p)
    seed &= M
    n = len(p)
    r64 = lambda i: int.from_bytes(p[i:i + 8], "little")
    r32 = lambda i: int.from_bytes(p[i:i + 4], "little")
    r16 = lambda i: int.from_bytes(p[i:i + 2], "little")
    L = n & M

    s = seed ^ fold(seed ^ P[0], P[1])
    s2 = fold(s ^ L ^ P[6], P[7])
    t = (s ^ P[5] ^ ((L << 56) & M)) | 1

    if n <= 16:
        def tail3(i, k):
            return p[i] << 16 | p[i + (k >> 1)] << 8 | p[i + k - 1]
        if n == 0: a, b = 0, 0
        elif n == 1: a, b = p[0], 0
        elif n == 2: a, b = r16(0), 0
        elif n == 3: a, b = r16(0) | p[2] << 16, 0
        elif n == 4: a, b = r32(0), 0
        elif n <= 7: a, b = r32(0), tail3(4, n - 4)
        elif n == 8: a, b = r64(0), 0
        elif n <= 11: a, b = r64(0), tail3(8, n - 8)
        else: a, b = r64(0), r32(8) | r32(n - 4) << 32
        lo, hi = mul(a ^ P[2] ^ s, b ^ P[3] ^ s2 ^ L)
        return fold(lo ^ P[4], hi ^ t)

    if n <= 32:
        w0, w1 = r64(0), r64(8)
        w2 = r64(4 if n < 20 else n - 16)
        w3 = r64(n - 8)
        x = fold(w0 ^ P[2] ^ s, w1 ^ P[3] ^ s2)
        y = fold(w2 ^ P[4] ^ s, w3 ^ P[5] ^ s2)
        a = fold(x ^ P[6], y ^ P[7] ^ L)
        return fold(a ^ P[4], t)

    if n <= 64:
        s3 = s ^ s2
        x0 = fold(r64(0) ^ P[2] ^ s, r64(8) ^ P[3] ^ s2)
        x1 = fold(r64(16) ^ P[4] ^ s, r64(24) ^ P[5] ^ s2)
        x2 = fold(r64(n - 32) ^ P[6] ^ s3, r64(n - 24) ^ P[7] ^ s)
        x3 = fold(r64(n - 16) ^ P[0] ^ s3, r64(n - 8) ^ P[1] ^ s)
        return fold(x0 ^ x2 ^ P[4], x1 ^ x3 ^ t)

    k16, k17, k18 = key(16), key(17), key(18)
    k24, k25, k26, k30, k31 = key(24), key(25), key(26), key(30), key(31)
    perm = lambda x: aesenc(aesenc(aesenc(x, k16), k17), k18)
    fin = lambda x: aesenc(aesenc(aesenc(x, k24), k25), k26)
    sv = blk(s, fold(s ^ P[2], P[3]))
    lv = aesenc(aesenc(blk(fold(s ^ L ^ P[4], P[5]), fold(s ^ L ^ P[6], P[7])), k30), k31)
    init = lambda i: aesenc(aesenc(bxor(key(i), sv), k30), k31)
    ld = lambda i: p[i:i + 16]

    if n >= 4096:
        x = lemac(p, n, sv, lv, k16, k17, k18, key(19), k30, k31)
        f = fin(x)
        return int.from_bytes(f[:8], "little") ^ int.from_bytes(f[8:], "little")

    if n <= 128:
        acc = [init(i) for i in range(8)]
        for i in range(4):
            acc[i] = perm(bxor(acc[i], ld(16 * i)))
        for i in range(4, 8):
            acc[i] = perm(bxor(acc[i], ld(n - 16 * (8 - i))))
    elif n <= 256:
        acc = [init(i) for i in range(16)]
        for i in range(8):
            acc[i] = perm(bxor(acc[i], ld(16 * i)))
        for i in range(8, 16):
            acc[i] = perm(bxor(acc[i], ld(n - 16 * (16 - i))))
    else:
        acc = [init(i) for i in range(16)]
        off = 0
        while True:
            if off + 256 > n:
                off = n - 256
            for i in range(16):
                acc[i] = perm(bxor(acc[i], ld(off + 16 * i)))
            if off + 256 >= n:
                break
            off += 256

    x = bytes(16)
    for a in acc:
        x = bxor(x, a)
    f = fin(bxor(x, lv))
    return int.from_bytes(f[:8], "little") ^ int.from_bytes(f[8:], "little")


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    src = open(os.path.join(here, "..", "..", "test", "vectors.h")).read()
    vecs = [(int(a), int(b, 16), int(c, 16)) for a, b, c in
            re.findall(r"\{(\d+),\s*0x([0-9a-fA-F]+)ull,\s*0x([0-9a-fA-F]+)ull\}", src)]
    maxlen = max(v[0] for v in vecs)
    msg = bytes((i * 31 + 7) % 256 for i in range(maxlen))
    ok = 0
    fails = []
    for n, seed, h in vecs:
        got = hassh64(msg[:n], seed)
        if got == h:
            ok += 1
        else:
            fails.append((n, seed, h, got))
    print(f"{ok}/{len(vecs)} pass, lengths {min(v[0] for v in vecs)}..{maxlen}")
    for f in fails[:40]:
        print("FAIL len=%d seed=%016x want=%016x got=%016x" % f)
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main())
