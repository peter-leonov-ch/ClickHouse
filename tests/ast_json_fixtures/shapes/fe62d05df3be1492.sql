SELECT
    key,
    value1,
    value2,
    toUInt64(min(time)) AS start_ts
FROM join_inner_table
    PREWHERE (id = '833c9e22-c245-4eb5-8745-117a9a1f26b1') AND (number > toUInt64('1610517366120'))
GROUP BY key, value1, value2
ORDER BY key, value1, value2
LIMIT 10
