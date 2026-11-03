/* Hash every length 0..2048 from an exactly-sized heap block, so ASan flags
 * any read past either end. Run with -fsanitize=address. */
#include "../hassh.h"
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    uint64_t acc = 0;
    for (size_t len = 0; len <= 2048; len++) {
        uint8_t *b = malloc(len ? len : 1);
        for (size_t i = 0; i < len; i++) b[i] = (uint8_t)(i * 31 + len);
        acc ^= hassh64(b, len, len);
        free(b);
    }
    printf("bounds ok %016llx\n", (unsigned long long)acc);
    return 0;
}
