# hassh64 for SQLite

A loadable extension adding `hassh64(value)` and `hassh64(value, seed)`.

```sh
make                      # builds hassh64.so (hassh64.dylib on macOS)
sqlite3
sqlite> .load ./hassh64
sqlite> SELECT hassh64('hello'), hassh64(payload, 42) FROM t;
```

* TEXT hashes its UTF-8 bytes, BLOB its bytes; numbers hash their text form
  (SQLite's usual conversion). A NULL argument gives NULL.
* The result is the 64-bit hash as a signed INTEGER, since SQLite has no
  unsigned type. The bits are those of `hassh64()` in C: `hassh64('hello')`
  is `-1297528511316857414`, which is `0xedfe40b18d046dba`.
* Registered as deterministic and innocuous, so it can be used in indexes,
  generated columns and views.

`make test` builds the extension and checks it against the C test vectors.
