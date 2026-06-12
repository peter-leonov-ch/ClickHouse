WITH
   (a > b) as cte,
   query AS
    (
        SELECT count()
        FROM test
        WHERE cte
    )
SELECT *
FROM query
