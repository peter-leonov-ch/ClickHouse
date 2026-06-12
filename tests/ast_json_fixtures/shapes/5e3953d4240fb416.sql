SELECT grp_aggreg FROM data_02295 GROUP BY a, grp_aggreg ORDER BY a SETTINGS optimize_aggregation_in_order = 0 FORMAT JSONEachRow
