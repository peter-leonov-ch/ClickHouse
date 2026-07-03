select key, groupArray(repeat('a', 200)), count() from data_01513 group by key with totals format Null settings optimize_aggregation_in_order=1
