/* Out-of-bounds checks.
 * 1. Every length 0..2048, and 4096..4400 (the long-input path), from an
 *    exactly-sized heap block, so ASan flags any read past either end. Run
 *    with -fsanitize=address.
 * 2. POSIX only: lengths 4096..6144 placed against an inaccessible guard page
 *    on either side. ASan cannot see the inline-assembly VAES loop; a read
 *    past the buffer there faults here. */
#include "../hassh.h"
#include <stdio.h>
#include <stdlib.h>
#if defined(__unix__) || defined(__APPLE__)
#  include <sys/mman.h>
#  include <unistd.h>
#  define HAVE_GUARD 1
#endif
int main(void) {
    uint64_t acc = 0;
    for (size_t len = 0; len <= 4400; len = len == 2048 ? 4096 : len + 1) {
        uint8_t *b = malloc(len ? len : 1);
        for (size_t i = 0; i < len; i++) b[i] = (uint8_t)(i * 31 + len);
        acc ^= hassh64(b, len, len);
        free(b);
    }
#ifdef HAVE_GUARD
    size_t pg = (size_t)sysconf(_SC_PAGESIZE), maxl = 6144, npg = (maxl + pg - 1) / pg;
    uint8_t *m = mmap(0, (npg + 2) * pg, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (m == MAP_FAILED || mprotect(m, pg, PROT_NONE) || mprotect(m + (npg + 1) * pg, pg, PROT_NONE)) {
        printf("guard pages unavailable\n");
        return 1;
    }
    uint8_t *lo = m + pg, *hi = m + (npg + 1) * pg;
    hassh64_key k;
    hassh64_key_init(&k, 12345);
    for (size_t len = 4096; len <= maxl; len++)
        for (int side = 0; side < 2; side++) {
            uint8_t *p = side ? hi - len : lo;      /* ends at / starts at a guard page */
            for (size_t i = 0; i < len; i++) p[i] = (uint8_t)(i * 131 + len * 7 + (i >> 8));
            uint64_t a = hassh64(p, len, len), b = hassh64_keyed(&k, p, len);
            acc ^= a + (b ^ (b >> 1));
        }
    munmap(m, (npg + 2) * pg);
#endif
    printf("bounds ok %016llx\n", (unsigned long long)acc);
    return 0;
}
