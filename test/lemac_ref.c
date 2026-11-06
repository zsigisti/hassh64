/* hassh64's long-input step must be LeMac's step function exactly (the
 * corrected 2025 version), or LeMac's 26-active-S-box bound does not apply.
 * This compares HASSH_LM_STEP (portable AES) against the ROUND macro of the
 * LeMac reference implementation, copied verbatim below (CC0, Augustin
 * Bariant and Gaetan Leurent, github.com/AugustinBariant/Implementations_LeMac_PetitMac,
 * lemac.c), over random states and messages. x86-64 with AES-NI only. */
#define HASSH_WANT_PORTABLE
#include "../hassh.h"
#include <immintrin.h>
#include <stdio.h>

typedef struct { __m128i S[9]; } state;
#define ROUND(S, M0, M1, M2, M3, RR, R0, R1, R2) do {           \
    __m128i T = S.S[8];						\
    S.S[8] = _mm_aesenc_si128(S.S[7],M3);			\
    S.S[7] = _mm_aesenc_si128(S.S[6],M1);			\
    S.S[6] = _mm_aesenc_si128(S.S[5],M1);			\
    S.S[5] = _mm_aesenc_si128(S.S[4],M0);			\
    S.S[4] = _mm_aesenc_si128(S.S[3],M0);			\
    S.S[3] = _mm_aesenc_si128(S.S[2],R1 ^ R2);			\
    S.S[2] = _mm_aesenc_si128(S.S[1],M3);			\
    S.S[1] = _mm_aesenc_si128(S.S[0],M3);			\
    S.S[0] = S.S[0] ^ T ^ M2;					\
    R2 = R1;                                                    \
    R1 = R0;                                                    \
    R0 = RR ^ M1;                                               \
    RR = M2;                                                    \
  } while (0);

static uint64_t rng = 0x243f6a8885a308d3ull;
static uint64_t next(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }
static hassh_blk rb(void) { return hassh_set_p(next(), next()); }
static __m128i tox(hassh_blk b) { uint8_t t[16]; hassh_st_p(t, b); return _mm_loadu_si128((const __m128i *)t); }
static int eq(hassh_blk a, __m128i b) { uint8_t t[16]; _mm_storeu_si128((__m128i *)t, b); uint8_t u[16]; hassh_st_p(u, a); return !memcmp(u, t, 16); }

int __attribute__((target("aes,sse4.1"))) main(void) {
    long bad = 0;
    for (int trial = 0; trial < 2000; trial++) {
        typedef hassh_blk hassh_lm_t;
        HASSH_LM_VARS(hassh_blk, v);
        state S;
        __m128i RR = _mm_setzero_si128(), R0 = RR, R1 = RR, R2 = RR;
        hassh_blk *w[9] = { &v0, &v1, &v2, &v3, &v4, &v5, &v6, &v7, &v8 };
        for (int i = 0; i < 9; i++) { *w[i] = rb(); S.S[i] = tox(*w[i]); }
        vq = vr0 = vr1 = vr2 = hassh_set_p(0, 0);
        int steps = 1 + trial % 12;
        for (int k = 0; k < steps + 4; k++) {           /* message steps, then 4 blank ones */
            hassh_blk m[4];
            for (int i = 0; i < 4; i++) m[i] = k < steps ? rb() : hassh_set_p(0, 0);
            HASSH_LM_STEP(v, m[0], m[1], m[2], m[3], hassh_aesenc_p, hassh_x_p);
            ROUND(S, tox(m[0]), tox(m[1]), tox(m[2]), tox(m[3]), RR, R0, R1, R2);
            for (int i = 0; i < 9; i++) bad += !eq(*w[i], S.S[i]);
            bad += !eq(vq, RR) + !eq(vr0, R0) + !eq(vr1, R1) + !eq(vr2, R1 ^ R2);
        }
    }
    printf(bad ? "LeMac step MISMATCH (%ld)\n" : "LeMac step matches the reference%.0ld\n", bad);
    return bad != 0;
}
