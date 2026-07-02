WITH (
        SELECT sleepEachRow(3)
    ) AS res
SELECT *
FROM system.one
FORMAT Null
SETTINGS max_execution_time = 2
