SELECT intDivOrZero(intDivOrZero(toLowCardinality(-128), toLowCardinality(-1)) = 0, materialize(toLowCardinality(4)))
FROM realtimebuff__fuzz_19 GROUP BY materialize(toLowCardinality(-127)), intDivOrZero(0, 0) = toLowCardinality(toLowCardinality(0))
WITH TOTALS ORDER BY ALL DESC NULLS FIRST
FORMAT Null
