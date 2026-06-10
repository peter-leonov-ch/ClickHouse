SELECT
    max(a) AS max_a,
    max(b) AS max_b
FROM `03611_t_nullsafe`
HAVING (max_a <=> max_b) OR (max_a IS DISTINCT FROM max_b)
ORDER BY max_a
