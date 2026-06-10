WITH
    t as (select number from numbers(5))
SELECT *
FROM numbers(8)
WHERE number IN t
