SELECT a, b
FROM numbers(3)
GROUP BY number as a, (number + number) as b WITH CUBE
ORDER BY a, b
