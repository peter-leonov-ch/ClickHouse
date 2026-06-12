WITH (x, y) -> y AS lambda
SELECT arrayMap(lambda(tuple(x), x + 1), [1, 2, 3]), lambda(tuple(x), x + 1), 1 AS x
