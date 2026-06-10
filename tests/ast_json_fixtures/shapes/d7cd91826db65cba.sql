WITH
  test AS (
    SELECT
      *,
      count() OVER () AS c
    FROM numbers(10)
  )
SELECT * FROM test
ORDER BY toString(number)
