/* Regression test: cross-length and lane-start attacks on the AES paths.
 *
 * The attacker computes, for the CURRENT lane-start function, the start-state
 * difference between two lengths (or two lanes) at seed 0, plants it in the
 * message, and hopes it holds for every seed. With a weak lane start (e.g.
 * one AES round over raw (seed, len): SubBytes never mixes seed bytes with
 * length bytes) it does, giving cross-length pairs and 256-way
 * multicollisions under every seed. Seed 0 collides by construction; the test
 * checks that no OTHER seed does.
 *
 * Built against the live header, so it re-runs the attack against whatever
 * lane start the code currently has. Exit 0 = attacks fail. */
#define HASSH_WANT_PORTABLE
#include "../hassh.h"
#include <stdio.h>
#include <stdlib.h>

static uint64_t rng = 0x243f6a8885a308d3ull;
static uint64_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng * 0x2545F4914F6CDD1Dull; }

static hassh_blk start(int i, uint64_t seed, size_t L) { (void)L; return hassh_init_p(i, hassh_sv_p(seed)); }
/* Attacker's guess of start_i(L1) ^ start_i(L2), computed at seed 0. */
static hassh_blk D(int i, size_t L1, size_t L2) { return hassh_x_p(start(i, 0, L1), start(i, 0, L2)); }

/* Offset of lane i's first block for length L (mirrors hassh_large_portable). */
static size_t first_off(int i, size_t L) {
    int lanes = L <= 128 ? 8 : 16;
    if (L > 256) return 16 * (size_t)i;
    int half = lanes / 2;
    return i < half ? 16 * (size_t)i : L - 16 * (size_t)(lanes - i);
}

static void build(uint8_t *A, size_t L1, const uint8_t *B, size_t L2) {
    int lanes = L1 <= 128 ? 8 : 16;
    if (L1 > 256) memcpy(A + 256, B + L2 - 256, 256);   /* last stripe identical */
    for (int i = 0; i < lanes; i++) {
        hassh_blk a = hassh_x_p(hassh_ld_p(B + first_off(i, L2)), D(i, L1, L2));
        memcpy(A + first_off(i, L1), a.b, 16);
    }
}

static int fails = 0;
static void report(const char *name, int collide, int n) {
    printf("%-46s %s (%d/%d nonzero seeds collide)\n", name, collide ? "COLLIDES" : "ok", collide, n);
    if (collide) fails++;
}

int main(void) {
    static uint8_t A[4096], B[4096];
    const int N = 2000;
    struct { size_t L1, L2; const char *n; } cases[] = {
        {128, 100, "cross-length 8-lane   128 vs 100"}, {128, 65, "cross-length 8-lane   128 vs 65"},
        {256, 200, "cross-length 16-lane  256 vs 200"}, {256, 129, "cross-length 16-lane  256 vs 129"},
        {512, 300, "cross-length 2-stripe 512 vs 300"}, {512, 257, "cross-length 2-stripe 512 vs 257"},
    };
    for (size_t c = 0; c < sizeof cases / sizeof *cases; c++) {
        for (size_t i = 0; i < cases[c].L2; i++) B[i] = (uint8_t)rnd();
        memset(A, 0, sizeof A);
        build(A, cases[c].L1, B, cases[c].L2);
        int eq = 0;
        for (int k = 0; k < N; k++) {
            uint64_t seed = k == 0 ? ~0ull : rnd() | 1;
            eq += hassh64(A, cases[c].L1, seed) == hassh64(B, cases[c].L2, seed);
        }
        report(cases[c].n, eq, N);
    }

    /* 256-way multicollision attempt, lengths 513..768. */
    {
        static uint8_t M[256][768];
        uint8_t S[256];
        for (int i = 0; i < 256; i++) S[i] = (uint8_t)rnd();
        for (int k = 0; k < 256; k++) {
            size_t L = 513 + (size_t)k;
            memset(M[k], 0xAB, sizeof M[k]);
            for (int i = 0; i < 16; i++) {
                hassh_blk b = hassh_x_p(hassh_ld_p(S + 16 * i), start(i, 0, L));
                memcpy(M[k] + 16 * i, b.b, 16);
            }
        }
        int bad = 0;
        for (int t = 0; t < 200; t++) {
            uint64_t seed = rnd() | 1;
            uint64_t h0 = hassh64(M[0], 513, seed);
            int same = 0;
            for (int k = 1; k < 256; k++) same += hassh64(M[k], 513 + (size_t)k, seed) == h0;
            bad += same > 0;
        }
        report("256-way multicollision, lengths 513..768", bad, 200);
    }

    /* Lane swap with the start difference guessed at seed 0. */
    {
        for (int i = 0; i < 128; i++) A[i] = (uint8_t)rnd();
        memcpy(B, A, 128);
        hassh_blk d = hassh_x_p(start(0, 0, 128), start(4, 0, 128));
        hassh_blk b0 = hassh_x_p(hassh_ld_p(A + 64), d), b4 = hassh_x_p(hassh_ld_p(A), d);
        memcpy(B, b0.b, 16); memcpy(B + 64, b4.b, 16);   /* 128 B: lane 4 reads e-64 = 64 */
        int eq = 0;
        for (int k = 0; k < N; k++) { uint64_t seed = rnd() | 1; eq += hassh64(A, 128, seed) == hassh64(B, 128, seed); }
        report("lane swap 0<->4, start diff guessed at seed 0", eq, N);
    }

    /* Long inputs (LeMac path), from the 2026-11-06 review. Each pair absorbs
     * identical or related blocks; none may collide for any tested seed. */
    {
        static uint8_t X[8192], Y[8192];
        struct { size_t lx, ly; int kind; const char *n; } lc[] = {
            {4097, 4224, 0, "LeMac: zeros 4097 vs 4224 (same blocks)"},
            {4100, 4101, 0, "LeMac: zeros 4100 vs 4101 (same blocks)"},
            {4097, 4100, 1, "LeMac: period-3 4097 vs 4100 (same blocks)"},
            {4096, 4224, 2, "LeMac: M vs M + 128 zero bytes"},
            {4096, 4096, 3, "LeMac: state 0 <-> state 1 word swap"},
            {4096, 4096, 4, "LeMac: same difference in both states"},
            {4096, 4096, 5, "LeMac: M2 at step t vs M1 at t+1 (R0 cancel)"},
            {4160, 4160, 6, "LeMac: byte absorbed twice by the overlap"},
            {4096, 4095, 7, "LeMac/lanes: prefix 4096 vs 4095"},
        };
        for (size_t c = 0; c < sizeof lc / sizeof *lc; c++) {
            for (size_t i = 0; i < sizeof X; i++) X[i] = (uint8_t)rnd();
            switch (lc[c].kind) {
            case 0: memset(X, 0, sizeof X); memset(Y, 0, sizeof Y); break;
            case 1: for (size_t i = 0; i < sizeof X; i++) X[i] = Y[i] = (uint8_t)(i % 3 * 85); break;
            case 2: memcpy(Y, X, 4096); memset(Y + 4096, 0, 128); break;
            case 3: memcpy(Y, X, 4096);
                    for (size_t q = 0; q < 4096; q += 32) { memcpy(Y + q, X + q + 16, 16); memcpy(Y + q + 16, X + q, 16); }
                    break;
            case 4: memcpy(Y, X, 4096); Y[1000 / 32 * 32 + 3] ^= 0x5a; Y[1000 / 32 * 32 + 19] ^= 0x5a; break;
            case 5: memcpy(Y, X, 4096); Y[1024 + 64 + 5] ^= 0x77; Y[1024 + 128 + 32 + 5] ^= 0x77; break;
            case 6: memcpy(Y, X, 4160); Y[4100] ^= 1; break;           /* bytes 4032..4095 are read twice */
            case 7: memcpy(Y, X, 4096); break;
            }
            int eq = 0;
            for (int k = 0; k < N; k++) {
                uint64_t seed = k == 0 ? 0 : rnd();
                eq += hassh64(X, lc[c].lx, seed) == hassh64(Y, lc[c].ly, seed);
            }
            report(lc[c].n, eq, N);
        }
    }

    printf(fails ? "FAIL: %d attack(s) still work\n" : "all cross-length / lane-start attacks fail\n", fails);
    return fails != 0;
}
