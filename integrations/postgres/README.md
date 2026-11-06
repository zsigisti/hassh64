# hassh64 for PostgreSQL

An extension adding `hassh64(text)`, `hassh64(bytea)` and the seeded forms
`hassh64(text, bigint)` / `hassh64(bytea, bigint)`, all returning `bigint`.

```sh
make && sudo make install          # PGXS; needs the server headers (postgresql-server-dev / postgresql-devel)
psql -c 'CREATE EXTENSION hassh64'
psql -c "SELECT hassh64('hello'), hassh64(payload, 42) FROM t"
```

* `text` hashes its bytes in the database encoding (UTF-8 in practice), so
  `hassh64('hello'::text) = hassh64('hello'::bytea)`.
* The result is the 64-bit hash as a signed `bigint` (PostgreSQL has no
  unsigned type), with the bits of `hassh64()` in C: `hassh64('hello')` is
  `-1297528511316857414`, which is `0xedfe40b18d046dba`.
* `IMMUTABLE STRICT PARALLEL SAFE`: usable in expression indexes, generated
  columns and parallel queries; NULL in gives NULL out.

`./check.sh` builds the extension, starts a throwaway PostgreSQL 18+ cluster
that loads it from a temporary directory (nothing is installed), and compares
`sql/hassh64.sql` with `expected/hassh64.out`. With an installed extension,
`make installcheck` runs the same test through `pg_regress`.
