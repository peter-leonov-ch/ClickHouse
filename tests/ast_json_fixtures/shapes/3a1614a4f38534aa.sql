WITH
    ( SELECT sleep(0.0001) FROM system.one ) as a1,
    ( SELECT sleep(0.0001) FROM system.one ) as a2,
    ( SELECT sleep(0.0001) FROM system.one ) as a3,
    ( SELECT sleep(0.0001) FROM system.one ) as a4,
    ( SELECT sleep(0.0001) FROM system.one ) as a5
SELECT '02177_CTE_NEW_ANALYZER', a1, a2, a3, a4, a5 FROM system.numbers LIMIT 100
        FORMAT Null
SETTINGS enable_analyzer = 1
