WITH
    x -> (lambda1(x) + 1) AS lambda,
    lambda AS lambda1
SELECT lambda(1)
