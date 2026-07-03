SELECT 'optimize_trivial_count_query', count() FROM adaptive_table SETTINGS
    optimize_trivial_count_query=1
FORMAT CSV
