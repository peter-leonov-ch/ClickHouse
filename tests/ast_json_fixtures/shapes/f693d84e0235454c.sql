SELECT (SELECT hasColumnInTable('system', 'numbers', 'not_existing')) ? not_existing : 42 as not_existing FROM system.numbers LIMIT 1 FORMAT TSKV
