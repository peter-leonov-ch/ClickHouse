SELECT (1 as a) ? (number + 2 as b) : (number + 3 as c) as d, a, b, c, d FROM system.numbers LIMIT 1 FORMAT TSKV
