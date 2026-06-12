SELECT DISTINCT 13, *, or(-32 = dictGetInt32(toFixedString('dictionary_all', toLowCardinality(14)), toFixedString('i32', 3), id), isNotDistinctFrom(1, 9223372036854775806), toLowCardinality(19), not(equals(payload, 9223372036854775806))), isNotNull('dictGetFloat64 - plan'), id, isNotNull(1)
FROM tab__fuzz_24 PREWHERE equals(9223372036854775806, payload)
WHERE isNotDistinctFrom(id, isNotDistinctFrom(9223372036854775806, equals(1, isNotNull(9223372036854775806)))) QUALIFY and(NULL, equals(1, isZeroOrNull(1)))
ORDER BY payload DESC
