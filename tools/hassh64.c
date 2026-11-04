/* hassh64 command-line tool.
 *
 *   hassh64 "some text"          hash the text (exact bytes, no newline added)
 *   hassh64 -s 42 "some text"    with a seed (decimal or 0x hex; default 0)
 *   hassh64 -f file1 file2 ...   hash files' contents, one line per file
 *   echo -n text | hassh64       hash standard input (no arguments)
 *
 * Prints the 64-bit hash as 16 hex digits. The same input and seed give the
 * same hash on every machine. NOT a cryptographic hash: use sha256sum for
 * anything security-related. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../hassh.h"

static void usage(void) {
    fprintf(stderr,
            "usage: hassh64 [-s SEED] TEXT...\n"
            "       hassh64 [-s SEED] -f FILE...\n"
            "       hassh64 [-s SEED] < input\n"
            "Prints the hassh64 hash (16 hex digits). SEED is decimal or 0x-hex, default 0.\n"
            "Not cryptographic; use sha256sum for security.\n");
}

/* Reads a whole stream; returns NULL on error. */
static unsigned char *slurp(FILE *f, size_t *n) {
    size_t cap = 1 << 16, len = 0;
    unsigned char *buf = malloc(cap);
    if (!buf) return NULL;
    for (;;) {
        if (len == cap) {
            unsigned char *nb = cap > SIZE_MAX / 2 ? NULL : realloc(buf, cap *= 2);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        size_t r = fread(buf + len, 1, cap - len, f);
        len += r;
        if (r == 0) {
            if (ferror(f)) { free(buf); return NULL; }
            break;
        }
    }
    *n = len;
    return buf;
}

int main(int argc, char **argv) {
    uint64_t seed = 0;
    int files = 0, i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) { usage(); return 0; }
        if (!strcmp(argv[i], "-f")) { files = 1; continue; }
        if (!strcmp(argv[i], "-s") && i + 1 < argc) {
            char *end;
            errno = 0;
            seed = strtoull(argv[++i], &end, 0);
            if (errno || *end || !*argv[i]) {
                fprintf(stderr, "hassh64: bad seed '%s'\n", argv[i]);
                return 2;
            }
            continue;
        }
        usage();
        return 2;
    }

    if (i == argc) {
        if (files) { usage(); return 2; }
        size_t n;
        unsigned char *buf = slurp(stdin, &n);
        if (!buf) { perror("hassh64: stdin"); return 1; }
        printf("%016llx\n", (unsigned long long)hassh64(buf, n, seed));
        free(buf);
        return 0;
    }

    int rc = 0;
    for (; i < argc; i++) {
        if (!files) {
            printf("%016llx\n", (unsigned long long)hassh64(argv[i], strlen(argv[i]), seed));
            continue;
        }
        FILE *f = fopen(argv[i], "rb");
        size_t n;
        unsigned char *buf = f ? slurp(f, &n) : NULL;
        if (!buf) {
            fprintf(stderr, "hassh64: %s: %s\n", argv[i], strerror(errno));
            rc = 1;
        } else {
            printf("%016llx  %s\n", (unsigned long long)hassh64(buf, n, seed), argv[i]);
        }
        free(buf);
        if (f) fclose(f);
    }
    return rc;
}
