/* SQLite loadable extension: hassh64(value [, seed]).
 *
 *   .load ./hassh64          (sqlite3 shell)
 *   SELECT hassh64('hello');            -- seed 0
 *   SELECT hassh64(blob_col, 42);
 *
 * TEXT is hashed as its UTF-8 bytes, BLOB as its bytes, integers and reals
 * as their text form (SQLite's usual conversion). NULL gives NULL. The
 * result is the 64-bit hash as a signed INTEGER (SQLite has no unsigned
 * type): the same bits as hassh64() in C, so
 * hassh64('hello') = -1297528511316857414 = 0xedfe40b18d046dba. */
#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1
#include "../../hassh.h"

static void hassh64_fn(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL) return;
    uint64_t seed = 0;
    if (argc == 2) {
        if (sqlite3_value_type(argv[1]) == SQLITE_NULL) return;
        seed = (uint64_t)sqlite3_value_int64(argv[1]);
    }
    const void *p;
    int n;
    if (sqlite3_value_type(argv[0]) == SQLITE_BLOB) {
        p = sqlite3_value_blob(argv[0]);
        n = sqlite3_value_bytes(argv[0]);
    } else {
        p = sqlite3_value_text(argv[0]);
        n = sqlite3_value_bytes(argv[0]);
    }
    if (!p) p = "";   /* zero-length blob */
    sqlite3_result_int64(ctx, (sqlite3_int64)hassh64(p, (size_t)n, seed));
}

#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_hassh_init(sqlite3 *db, char **err, const sqlite3_api_routines *api) {
    (void)err;
    SQLITE_EXTENSION_INIT2(api);
    int flags = SQLITE_UTF8 | SQLITE_DETERMINISTIC | SQLITE_INNOCUOUS;
    int rc = sqlite3_create_function(db, "hassh64", 1, flags, 0, hassh64_fn, 0, 0);
    if (rc == SQLITE_OK) rc = sqlite3_create_function(db, "hassh64", 2, flags, 0, hassh64_fn, 0, 0);
    return rc;
}
