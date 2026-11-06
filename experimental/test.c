/* Every kernel level the CPU supports must equal the portable reference. */
#include "hassh_x86.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    int maxlvl = hx_level();
    if (argc > 1) maxlvl = atoi(argv[1]);      /* force a level (under SDE) */
    static uint8_t buf[70000 + 64];
    uint64_t x = 0x9e3779b97f4a7c15ull;
    for (size_t i = 0; i < sizeof buf; i++) { x ^= x << 13; x ^= x >> 7; x ^= x << 17; buf[i] = (uint8_t)x; }
    long bad = 0, n = 0;
    int levels[] = {HX_V2, HX_V3, HX_V4};
    for (int li = 0; li < 3; li++) {
        int lvl = levels[li];
        if (lvl > maxlvl) continue;
        long b0 = bad;
        for (size_t len = 0; len <= 70000; len += len < 4500 ? 1 : 997)
            for (int s = 0; s < 2; s++) {
                size_t off = (len + (size_t)s * 7) % 64;
                uint64_t seed = s ? x : 0;
                uint64_t want = hassh64_x86_level(buf + off, len, seed, HX_PORTABLE);
                uint64_t got = hassh64_x86_level(buf + off, len, seed, lvl);
                n++;
                if (got != want && bad++ - b0 < 5)
                    printf("v%d MISMATCH len=%zu off=%zu %016llx vs %016llx\n", lvl, len, off,
                           (unsigned long long)got, (unsigned long long)want);
            }
        printf("level v%d: %s\n", lvl, bad == b0 ? "ok" : "MISMATCH");
    }
    printf("detected level: v%d; %ld checks, %ld mismatches\n", hx_level(), n, bad);
    return bad != 0;
}
