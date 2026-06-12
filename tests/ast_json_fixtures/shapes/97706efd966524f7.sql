CREATE TABLE t(x Int, y Int) ORDER BY ()
AS SELECT number as x, number % 2 as y FROM numbers(100)
