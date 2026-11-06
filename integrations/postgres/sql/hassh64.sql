CREATE EXTENSION hassh64;
SELECT hassh64(''::text) AS empty, hassh64('hello'::text) AS hello, hassh64('hello'::text, 42) AS hello42;
SELECT hassh64('hello'::bytea) = hassh64('hello'::text) AS bytea_matches_text;
SELECT hassh64(repeat('0123456789', 1000)) AS len10000;
SELECT hassh64(NULL::text) IS NULL AS null_in_null_out;
