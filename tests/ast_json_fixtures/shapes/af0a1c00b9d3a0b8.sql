select
  anyIf(normalizeQuery(query), is_initial_query) q,
  if(
    
    any(Settings['allow_experimental_parallel_reading_from_replicas']) = '1',
    if(any(Settings['cluster_for_parallel_replicas']) = 'parallel_replicas',
      
      max2(count(), 19)::UInt64,
      
      max2(count(), 3)::UInt64,
    ),
    
    if(any(Settings['cluster_for_parallel_replicas']) = 'parallel_replicas',
      max2(count(), 10)::UInt64,
      max2(count(), 2)::UInt64
    )
  ) queries_with_subqueries,
  anyIf(ProfileEvents['DistributedIndexAnalysisScheduledReplicas'] > 0, is_initial_query) distributed_index_analysis_replicas,
  anyIf(ProfileEvents['ParallelReplicasUsedCount'] > 0, is_initial_query) read_with_parallel_replicas
from system.query_log
where
  event_date >= yesterday()
  and type = 'QueryFinish'
  and query_kind = 'Select'
  and Settings['distributed_index_analysis'] = '1'
  
  and endsWith(log_comment, '-' || currentDatabase())
group by initial_query_id
order by min(event_time_microseconds)
format Vertical
