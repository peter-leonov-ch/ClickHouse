SELECT prime
FROM system.primes
WHERE bitAnd(prime, prime + 1) = 0
LIMIT 4 OFFSET 3
