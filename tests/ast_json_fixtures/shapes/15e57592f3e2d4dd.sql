SELECT DISTINCT number * 1
FROM numbers(10, sipHash64(sipHash64(sipHash64(2), 1), 1, 2, *), sipHash64(sipHash64(29103473, sipHash64(1), '3', sipHash64(1), 1)))
GROUP BY
    1,
    isNullable(1)
    WITH TOTALS
ORDER BY 1 ASC SETTINGS enable_analyzer = 1
