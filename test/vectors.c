/* Known-answer test: hassh64 must reproduce test/vectors.h exactly, on every
 * ISA path and platform. These vectors freeze the 1.x output. */
#include <stdio.h>
#include <stdint.h>
#ifndef HASSH_HEADER
#  define HASSH_HEADER "../hassh.h"
#endif
#include HASSH_HEADER
#include "vectors.h"

int main(void) {
    static uint8_t m[4096];
    for (size_t i = 0; i < sizeof m; i++) m[i] = (uint8_t)(i * 31 + 7);
    size_t n = sizeof HASSH_VECTORS / sizeof HASSH_VECTORS[0], bad = 0;
    for (size_t i = 0; i < n; i++) {
        uint64_t h = hassh64(m, HASSH_VECTORS[i].len, HASSH_VECTORS[i].seed);
        hassh64_key k;
        hassh64_key_init(&k, HASSH_VECTORS[i].seed);
        if (hassh64_keyed(&k, m, HASSH_VECTORS[i].len) != h) {
            if (bad++ < 5) printf("KEYED MISMATCH len=%u\n", HASSH_VECTORS[i].len);
        }
        if (h != HASSH_VECTORS[i].hash) {
            if (bad++ < 5)
                printf("MISMATCH len=%u seed=%016llx got %016llx want %016llx\n", HASSH_VECTORS[i].len,
                       HASSH_VECTORS[i].seed, (unsigned long long)h, HASSH_VECTORS[i].hash);
        }
    }
    printf("vectors: %zu/%zu match\n", n - bad, n);
    return bad != 0;
}
