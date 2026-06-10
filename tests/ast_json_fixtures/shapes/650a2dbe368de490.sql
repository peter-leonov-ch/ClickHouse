WITH RECURSIVE q AS
(
    SELECT *
    FROM department__fuzz_0
    UNION ALL
    (
        WITH RECURSIVE x AS
        (
            SELECT *
            FROM department__fuzz_0
            UNION ALL
            (
                SELECT *
                FROM q
                WHERE least(toFixedString('world', 5), 5, 5, inf, 58, nan, NULL)
                UNION ALL
                SELECT *
                FROM x
                WHERE sipHash128(toLowCardinality('world'), toLowCardinality(materialize(5)), toUInt128(greatest(1, nan, NULL), toUInt128(5)), toUInt128(5), 5, toUInt128(5), materialize(5))
            )
        )
        SELECT *
        FROM x
    )
)
SELECT 1
FROM q
