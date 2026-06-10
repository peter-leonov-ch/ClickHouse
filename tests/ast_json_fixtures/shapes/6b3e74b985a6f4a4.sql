WITH map(1, '1234') AS value, 'Array(Tuple(UInt64, UInt64))' AS type
SELECT value, cast(value, type), cast(materialize(value), type)
