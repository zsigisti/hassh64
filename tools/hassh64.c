/* hassh64 command-line tool.
 *
 *   hassh64 "some text"          hash the text (exact bytes, no newline added)
 *   hassh64 -s 42 "some text"    with a seed (decimal or 0x hex; default 0)
 *   hassh64 -f file1 file2 ...   hash files' contents, one line per file
 *   printf text | hassh64        hash standard input (no arguments)
 *
 * Prints the 64-bit hash as 16 hex digits. The same input and seed give the
 * same hash on every machine: text arguments are hashed as UTF-8 and input is
 * read as raw bytes, on Windows too. NOT a cryptographic hash: use sha256sum
 * for anything security-related.
 *
 * Exit status: 0 success, 1 an input could not be read or the output could
 * not be written, 2 usage error. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#  include <fcntl.h>
#  include <io.h>
#  include <windows.h>
#  include <shellapi.h>
#endif
#include "../hassh.h"

static void usage(void) {
    fprintf(stderr,
            "usage: hassh64 [-s SEED] TEXT...\n"
            "       hassh64 [-s SEED] -f FILE...\n"
            "       hassh64 [-s SEED] < input\n"
            "Prints the hassh64 hash (16 hex digits). SEED is a decimal number or 0x-prefixed\n"
            "hex, 0 to 2^64-1, default 0. Not cryptographic; use sha256sum for security.\n");
}

/* Decimal, or hex after 0x/0X, nothing else: no sign, no spaces, no octal. */
static int parse_seed(const char *s, uint64_t *out) {
    int base = 10;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        s += 2;
    }
    if (*s == '\0') return 0;
    for (const char *c = s; *c; c++) {
        int digit = (*c >= '0' && *c <= '9') ||
                    (base == 16 && ((*c >= 'a' && *c <= 'f') || (*c >= 'A' && *c <= 'F')));
        if (!digit) return 0;
    }
    char *end;
    errno = 0;
    unsigned long long v = strtoull(s, &end, base);
    if (errno || *end) return 0;
    *out = (uint64_t)v;
    return 1;
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

/* stdout must really have been written: `hassh64 x > /dev/full` is an error. */
static int finish_output(int rc) {
    if (fflush(stdout) != 0 || ferror(stdout)) {
        perror("hassh64: stdout");
        return 1;
    }
    return rc;
}

#ifdef _WIN32
/* Windows passes main() its arguments in the local code page. Rebuild argv as
 * UTF-8 from the UTF-16 command line, so text hashes the same bytes as on
 * Linux and macOS; keep the UTF-16 form for opening files. */
static wchar_t **g_wargv;

static char **utf8_argv(int *argc) {
    int n;
    g_wargv = CommandLineToArgvW(GetCommandLineW(), &n);
    if (!g_wargv) return NULL;
    char **argv = calloc((size_t)n + 1, sizeof *argv);
    if (!argv) return NULL;
    for (int i = 0; i < n; i++) {
        int len = WideCharToMultiByte(CP_UTF8, 0, g_wargv[i], -1, NULL, 0, NULL, NULL);
        if (len <= 0 || !(argv[i] = malloc((size_t)len))) return NULL;
        WideCharToMultiByte(CP_UTF8, 0, g_wargv[i], -1, argv[i], len, NULL, NULL);
    }
    *argc = n;
    return argv;
}

static FILE *open_input(int i, const char *path) {
    (void)path;
    return _wfopen(g_wargv[i], L"rb");
}
#else
static FILE *open_input(int i, const char *path) {
    (void)i;
    return fopen(path, "rb");
}
#endif

int main(int argc, char **argv) {
#ifdef _WIN32
    /* Raw bytes in: text mode would turn CRLF into LF and stop at 0x1A. */
    _setmode(_fileno(stdin), _O_BINARY);
    argv = utf8_argv(&argc);
    if (!argv) {
        fprintf(stderr, "hassh64: cannot read the command line\n");
        return 2;
    }
#endif
    uint64_t seed = 0;
    int files = 0, i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) { usage(); return 0; }
        if (!strcmp(argv[i], "-f")) { files = 1; continue; }
        if (!strcmp(argv[i], "-s") && i + 1 < argc) {
            if (!parse_seed(argv[++i], &seed)) {
                fprintf(stderr, "hassh64: bad seed '%s' (use decimal or 0x hex)\n", argv[i]);
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
        return finish_output(0);
    }

    int rc = 0;
    for (; i < argc; i++) {
        if (!files) {
            printf("%016llx\n", (unsigned long long)hassh64(argv[i], strlen(argv[i]), seed));
            continue;
        }
        FILE *f = open_input(i, argv[i]);
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
    return finish_output(rc);
}
