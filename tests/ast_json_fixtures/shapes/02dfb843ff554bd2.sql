WITH (
        SELECT CAST(toFixedString(toFixedString(materialize(toFixedString('111111111111111111111111111111111111111', 39)), 39), 39), 'UInt128')
    ) AS v
SELECT
    coalesce(materialize(toLowCardinality(toNullable(1))), 10, NULL),
    max(v)
FROM remote('127.0.0.{1,2}', currentDatabase(), test__fuzz_21)
GROUP BY
    coalesce(NULL),
    coalesce(1, 10, 10, materialize(NULL))
