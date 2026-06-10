WITH
    (SELECT uuid FROM system.tables WHERE database = currentDatabase() AND table = 't_ind_merge_1') AS uuid,
    extractAllGroupsVertical(message, 'containing (\\d+) columns \((\\d+) merged, (\\d+) gathered\)')[1] AS groups
SELECT
    groups[1] AS total,
    groups[2] AS merged,
    groups[3] AS gathered
FROM system.text_log
WHERE ((query_id = uuid || '::all_1_2_1') OR (query_id = currentDatabase() || '.t_ind_merge_1::all_1_2_1')) AND notEmpty(groups)
ORDER BY event_time_microseconds
