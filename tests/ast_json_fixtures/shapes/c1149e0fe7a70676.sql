select part_name, ProfileEvents['MarkCacheHits'] hits, ProfileEvents['MarkCacheMisses'] misses
  from system.part_log
  where database = currentDatabase() and event_type = 'MergeParts'
  order by event_time_microseconds
  format CSVWithNames
