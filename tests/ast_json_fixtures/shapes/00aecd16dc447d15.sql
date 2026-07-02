SELECT number
    ,lag(number, 1, 8472) OVER () lag
FROM numbers(5)
ORDER BY number
FORMAT Pretty
