#!/bin/sh
# Builds the extension, stages it under a temporary directory, starts a
# throwaway PostgreSQL 18+ cluster that finds it through extension_control_path
# and dynamic_library_path, and runs sql/hassh64.sql against expected/hassh64.out.
# Nothing is installed system-wide.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
tmp=$(mktemp -d)
trap 'pg_ctl -D "$tmp/db" -m immediate stop >/dev/null 2>&1 || true; rm -rf "$tmp"' EXIT
# with_llvm=no: skip the optional JIT bitcode (llvm-lto crashes in some images).
make -s -C "$here" with_llvm=no >/dev/null
make -s -C "$here" with_llvm=no install DESTDIR="$tmp/stage" >/dev/null
share=$tmp/stage$(pg_config --sharedir)
lib=$tmp/stage$(pg_config --pkglibdir)
initdb -A trust -U postgres -D "$tmp/db" >/dev/null
pg_ctl -D "$tmp/db" -l "$tmp/log" -w start -o "-k $tmp -h '' \
  -c extension_control_path='\$system:$share' -c dynamic_library_path='\$libdir:$lib'" >/dev/null
psql -X -q -h "$tmp" -U postgres -d postgres -a -f "$here/sql/hassh64.sql" > "$tmp/out" 2>&1
if diff -u "$here/expected/hassh64.out" "$tmp/out"; then echo "postgres: ok"; else echo "postgres: FAIL"; exit 1; fi
