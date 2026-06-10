WITH flt AS (
    SELECT number FROM numbers(10) WHERE number = shardNum()
)
SELECT shardNum(), number
FROM remote('127.0.0.{1..3}', numbers(100))
WHERE number IN (flt)
ORDER BY 1, 2
