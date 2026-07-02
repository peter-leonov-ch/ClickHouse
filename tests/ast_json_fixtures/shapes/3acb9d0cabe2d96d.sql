DELETE FROM t WHERE x IN (SELECT foo FROM bar) SETTINGS validate_mutation_query = 0
