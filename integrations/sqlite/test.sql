.load ./hassh64
.bail on
CREATE TEMP TABLE expect(got, want);
INSERT INTO expect VALUES
  (hassh64(''), -7048456233233775799),
  (hassh64('hello'), -1297528511316857414),
  (hassh64('hello', 42), -9055888138546785625),
  (hassh64(CAST('hello' AS BLOB)), -1297528511316857414),
  (hassh64(replace(printf('%.1000c', 'x'), 'x', '0123456789')), 6070299042740712857),
  (hassh64(NULL), NULL);
SELECT CASE WHEN count(*) = 0 THEN 'sqlite: ok' ELSE 'sqlite: FAIL ' || count(*) END
FROM expect WHERE got IS NOT want AND want IS NOT NULL;
