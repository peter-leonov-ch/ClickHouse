SELECT *
FROM remote('127.{1,2}', view(
    SELECT number%20 number
    FROM numbers(40)
    WHERE (number % 2) = (shardNum() - 1)
), number)
GROUP BY number
ORDER BY number ASC
LIMIT 1 BY number
LIMIT 5, 5
SETTINGS
    distributed_group_by_no_merge=2,
    distributed_push_down_limit=1
