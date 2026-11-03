#define HASSH_WANT_PORTABLE
#include "../hassh.h"
#include <stdio.h>
#include <stdlib.h>
/* Compares the compiled SIMD path against the portable reference spec,
 * and prints a digest so separately-compiled builds can be compared. */
int main(void) {
    static uint8_t buf[5000];
    uint64_t x = 0x9e3779b97f4a7c15ull, digest = 0;
    for (size_t i = 0; i < sizeof buf; i++) { x ^= x << 13; x ^= x >> 7; x ^= x << 17; buf[i] = (uint8_t)x; }
    long bad = 0;
    for (size_t len = 0; len <= 4096; len++)
        for (int s = 0; s < 4; s++) {
            uint64_t seed = s == 0 ? 0 : s == 1 ? ~0ull : x * (uint64_t)s;
            size_t off = (len * 7 + s) % 64;
            uint64_t h = hassh64(buf + off, len, seed);
            uint64_t r = len <= 32 ? hassh_small(buf + off, len, seed) : len <= 64 ? hassh_mid(buf + off, len, seed)
                                   : hassh_large_portable(buf + off, len, seed);
            if (h != r && bad++ < 5) printf("MISMATCH len=%zu seed=%d %016llx vs %016llx\n", len, s,
                                           (unsigned long long)h, (unsigned long long)r);
            digest = (digest ^ h) * 0x100000001b3ull;
        }
    printf("mismatches=%ld digest=%016llx\n", bad, (unsigned long long)digest);
    return bad != 0;
}
