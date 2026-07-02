SELECT
    part_name,
    arraySort(groupArrayIf(table, event_type = 'MergeParts')) AS mergers,
    arraySort(groupArrayIf(table, event_type = 'DownloadPart')) AS fetchers
FROM system.part_log
WHERE (event_time > (now() - 120))
  AND (table LIKE 'execute\\_on\\_single\\_replica\\_r%')
  AND (part_name NOT LIKE '%\\_0')
  AND (database = currentDatabase())
GROUP BY part_name
ORDER BY part_name
FORMAT Vertical
