/* Regression tests for seed-independent collisions.
 *
 * Each test builds a pair (or family) of distinct inputs that would collide
 * under EVERY seed in a design with the listed flaw, and checks that they do
 * not collide in hassh64. Statistical suites such as SMHasher3 never
 * construct these on purpose, so passing SMHasher3 says nothing about them.
 *
 *   1. zero-operand: a fold whose second operand was keyed only by constants
 *      can be forced to 0, making the first operand irrelevant.
 *   2. pair swap (scalar 17..64): two folds keyed identically and XORed.
 *   3. lane swap (AES): lanes keyed linearly by the seed and XORed; swapping
 *      two lanes' blocks (adjusted by K_i ^ K_j) left the XOR unchanged.
 *
 * Build against any header: -DHASSH_HEADER='"path"' (default ../hassh.h).
 * Exit status 0 = no seed-independent collision found. */
#ifndef HASSH_HEADER
#  define HASSH_HEADER "../hassh.h"
#endif
#include HASSH_HEADER
#include <stdio.h>
#include <string.h>

static const uint64_t SEEDS[] = {0, 1, 0x0123456789abcdefull, ~0ull, 0xdeadbeefcafef00dull};
#define NS (sizeof SEEDS / sizeof SEEDS[0])
#define NRAND 65536   /* random seeds per pair, on top of the fixed ones */

static int failures = 0;
static uint64_t rng_state = 0x9e3779b97f4a7c15ull;
static uint64_t rnd64(void) {
    uint64_t z = (rng_state += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull; z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

/* ANY collision over the fixed seeds plus NRAND random seeds fails: an
 * attack that works for a 2^-4 fraction of seeds must not print "ok".
 * Random 64-bit pairs collide with probability
 * ~2^-48 here, so a false failure is not a practical concern. */
static void check_pair2(const char *name, const uint8_t *a, size_t la, const uint8_t *b, size_t lb) {
    long same = 0, n = 0;
    for (size_t i = 0; i < NS; i++, n++) same += hassh64(a, la, SEEDS[i]) == hassh64(b, lb, SEEDS[i]);
    for (int i = 0; i < NRAND; i++, n++) { uint64_t sd = rnd64(); same += hassh64(a, la, sd) == hassh64(b, lb, sd); }
    printf("%-44s %s (%ld/%ld seeds collide)\n", name, same ? "COLLIDES" : "ok", same, n);
    if (same) failures++;
}
static void check_pair(const char *name, const uint8_t *a, const uint8_t *b, size_t len) {
    check_pair2(name, a, len, b, len);
}

static void put64(uint8_t *p, uint64_t v) { for (int i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (8 * i)); }
static void put128(uint8_t *p, int k) { put64(p, HASSH_K[2 * k]); put64(p + 8, HASSH_K[2 * k + 1]); }
static void xor16(uint8_t *d, const uint8_t *a) { for (int i = 0; i < 16; i++) d[i] ^= a[i]; }

int main(void) {
    uint8_t a[1024], b[1024];

    /* 1. 16 bytes, bytes 8..15 = P[3] ^ 16: second fold operand was 0. */
    memset(a, 0, 16); memset(b, 0, 16);
    put64(a + 8, HASSH_P[3] ^ 16); put64(b + 8, HASSH_P[3] ^ 16);
    put64(a, 0x1111111111111111ull); put64(b, 0x2222222222222222ull);
    check_pair("zero-operand, 16 bytes", a, b, 16);

    /* 1b. 32 bytes: w1 = bytes 8..15, second operand was w1 ^ P[3]. Forcing
     * it to 0 made w0 irrelevant; vary w0. */
    memset(a, 0x33, 32); memcpy(b, a, 32);
    put64(a + 8, HASSH_P[3]); put64(b + 8, HASSH_P[3]);
    put64(a, 0x1111111111111111ull); put64(b, 0x2222222222222222ull);
    check_pair("zero-operand, 32 bytes", a, b, 32);

    /* 2. 64 bytes (scalar mid): pair 0 = words 0,1, pair 2 = words 4,5 (e-32,
     * e-24). Keyed by constants (P2,P3) vs (P6,P7) and the same seed key,
     * then XORed, they could be swapped, adjusted by the constant difference. */
    for (int i = 0; i < 64; i++) a[i] = (uint8_t)(i * 7 + 1);
    memcpy(b, a, 64);
    {
        uint64_t w0, w1, w4, w5;
        memcpy(&w0, a, 8); memcpy(&w1, a + 8, 8); memcpy(&w4, a + 32, 8); memcpy(&w5, a + 40, 8);
        put64(b + 0,  w4 ^ HASSH_P[6] ^ HASSH_P[2]);
        put64(b + 8,  w5 ^ HASSH_P[7] ^ HASSH_P[3]);
        put64(b + 32, w0 ^ HASSH_P[2] ^ HASSH_P[6]);
        put64(b + 40, w1 ^ HASSH_P[3] ^ HASSH_P[7]);
    }
    check_pair("pair swap, 64 bytes", a, b, 64);

    /* 2b. 16 bytes, commute the two fold operands (the trap in the obvious
     * fix of XORing the same s into both sides). */
    {
        uint64_t wa = 0x0102030405060708ull, wb = 0x1112131415161718ull;
        put64(a, wa); put64(a + 8, wb);
        put64(b, wb ^ HASSH_P[3] ^ HASSH_P[2] ^ 16); put64(b + 8, wa ^ HASSH_P[2] ^ HASSH_P[3] ^ 16);
        check_pair("commuted operands, 16 bytes", a, b, 16);
    }

    /* 2c. 32 bytes: swap pair (w0,w1) with pair (w2,w3), plain and commuted. */
    for (int i = 0; i < 32; i++) a[i] = (uint8_t)(i * 11 + 9);
    {
        uint64_t w0, w1, w2, w3;
        memcpy(&w0, a, 8); memcpy(&w1, a + 8, 8); memcpy(&w2, a + 16, 8); memcpy(&w3, a + 24, 8);
        memcpy(b, a, 32);
        put64(b, w2 ^ HASSH_P[4] ^ HASSH_P[2]); put64(b + 8, w3 ^ HASSH_P[5] ^ HASSH_P[3]);
        put64(b + 16, w0 ^ HASSH_P[2] ^ HASSH_P[4]); put64(b + 24, w1 ^ HASSH_P[3] ^ HASSH_P[5]);
        check_pair("pair swap, 32 bytes", a, b, 32);
    }

    /* 2d. 64 bytes: pair 2 = commuted copy of pair 0 and vice versa. */
    for (int i = 0; i < 64; i++) a[i] = (uint8_t)(i * 5 + 2);
    memcpy(b, a, 64);
    {
        uint64_t w0, w1, w4, w5;
        memcpy(&w0, a, 8); memcpy(&w1, a + 8, 8); memcpy(&w4, a + 32, 8); memcpy(&w5, a + 40, 8);
        put64(b + 32, w1 ^ HASSH_P[3] ^ HASSH_P[6]);
        put64(b + 40, w0 ^ HASSH_P[2] ^ HASSH_P[7]);
        put64(b + 0,  w5 ^ HASSH_P[7] ^ HASSH_P[2]);
        put64(b + 8,  w4 ^ HASSH_P[6] ^ HASSH_P[3]);
        check_pair("commuted pair swap, 64 bytes", a, b, 64);
    }

    /* 3. 128 bytes: lane i reads block i. Swap lanes 0 and 1. */
    for (int i = 0; i < 128; i++) a[i] = (uint8_t)(i * 13 + 5);
    memcpy(b, a, 128);
    {
        uint8_t k[16], k1[16];
        put128(k, 0); put128(k1, 1); xor16(k, k1);          /* k = K0 ^ K1 */
        memcpy(b, a + 16, 16); xor16(b, k);
        memcpy(b + 16, a, 16); xor16(b + 16, k);
    }
    check_pair("lane swap, 128 bytes", a, b, 128);

    /* 3b. 512 bytes (two 256-byte stripes): swap the whole block streams of
     * lanes 0 and 1; only the first block of each needs the key adjustment. */
    for (int i = 0; i < 512; i++) a[i] = (uint8_t)(i * 29 + 3);
    memcpy(b, a, 512);
    {
        uint8_t k[16], k1[16];
        put128(k, 0); put128(k1, 1); xor16(k, k1);
        memcpy(b, a + 16, 16); xor16(b, k);
        memcpy(b + 16, a, 16); xor16(b + 16, k);
        memcpy(b + 256, a + 272, 16);
        memcpy(b + 272, a + 256, 16);
    }
    check_pair("lane swap, 512 bytes", a, b, 512);

    /* 4. Cross-length pairs whose pre-final state would be equal for every
     * seed if length reached these paths only via the top byte of t and a
     * b-operand XOR the attacker can cancel. */
    {
        /* 8 vs 11 bytes: same first 8 bytes; 11-byte tail encodes b = 8 ^ 11 = 3,
         * cancelling the length XOR in the b operand. */
        for (int i = 0; i < 8; i++) a[i] = (uint8_t)(i * 37 + 11);
        memcpy(b, a, 8); b[8] = 0; b[9] = 0; b[10] = 3;
        check_pair2("cross-length 8 vs 11 (len cancelled in b)", a, 8, b, 11);

        /* 40 vs 48 bytes: words with period 8 from byte 8, so both lengths
         * load the same eight words. */
        for (int i = 0; i < 8; i++) a[i] = (uint8_t)(i * 3 + 1);
        for (int i = 8; i < 64; i++) a[i] = (uint8_t)(0xC0 + (i & 7));
        memcpy(b, a, 64);
        check_pair2("cross-length 40 vs 48 (periodic words)", a, 40, b, 48);
        check_pair2("cross-length 48 vs 64 (periodic words)", a, 48, b, 64);
    }

    printf(failures ? "FAIL: %d seed-independent collision(s)\n" : "no seed-independent collisions\n", failures);
    return failures != 0;
}
