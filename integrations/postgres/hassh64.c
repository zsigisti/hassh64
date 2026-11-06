/* PostgreSQL extension: hassh64(bytea|text [, seed bigint]) -> bigint.
 *
 *   CREATE EXTENSION hassh64;
 *   SELECT hassh64('hello'::text);           -- seed 0
 *   SELECT hassh64(payload, 42) FROM t;      -- bytea
 *
 * text is hashed as its bytes in the database encoding (UTF-8 in practice).
 * The result is the 64-bit hash as a signed bigint (PostgreSQL has no
 * unsigned type): the same bits as hassh64() in C, so
 * hassh64('hello'::text) = -1297528511316857414 = 0xedfe40b18d046dba. */
#include "postgres.h"
#include "fmgr.h"
#if PG_VERSION_NUM >= 160000
#include "varatt.h"   /* VARDATA_ANY moved here in PostgreSQL 16 */
#endif
/* PGXS compiles with -Wdeclaration-after-statement; hassh.h is C99. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeclaration-after-statement"
#include "../../hassh.h"
#pragma GCC diagnostic pop

PG_MODULE_MAGIC;

static Datum hash_varlena(struct varlena *v, uint64_t seed) {
    struct varlena *d = pg_detoast_datum_packed(v);
    return Int64GetDatum((int64)hassh64(VARDATA_ANY(d), VARSIZE_ANY_EXHDR(d), seed));
}

PG_FUNCTION_INFO_V1(hassh64_varlena);
Datum hassh64_varlena(PG_FUNCTION_ARGS) {
    return hash_varlena(PG_GETARG_VARLENA_PP(0), 0);
}

PG_FUNCTION_INFO_V1(hassh64_varlena_seed);
Datum hassh64_varlena_seed(PG_FUNCTION_ARGS) {
    return hash_varlena(PG_GETARG_VARLENA_PP(0), (uint64_t)PG_GETARG_INT64(1));
}
