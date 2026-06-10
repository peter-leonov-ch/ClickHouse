SELECT
    number,
    grouping(number, number % 2) + 3 as gr
FROM remote('127.0.0.{2,3}', numbers(10))
GROUP BY
    CUBE(number, number % 2)
HAVING grouping(number) != 0
ORDER BY
    number, gr
SETTINGS force_grouping_standard_compatibility=0
