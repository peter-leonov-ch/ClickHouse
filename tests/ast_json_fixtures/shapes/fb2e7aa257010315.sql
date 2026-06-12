WITH subquery AS (SELECT []) SELECT t.* FROM 02834_t AS t JOIN subquery ON arrayExists(x -> x = 1, t.arr) ORDER BY t.id
