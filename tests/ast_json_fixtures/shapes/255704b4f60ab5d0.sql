WITH iterations AS ( SELECT number AS n FROM system.numbers LIMIT 5 )
SELECT hex(keccak256('consistent')), hex(keccak256(toString(n))) FROM iterations
ORDER BY n
