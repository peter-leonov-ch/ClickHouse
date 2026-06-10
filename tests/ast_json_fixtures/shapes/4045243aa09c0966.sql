with queries as (
    select query_id, row_number() over(order by event_time_microseconds) as ordinal
    from system.query_log
    where type = 'QueryFinish'
     and current_database = currentDatabase()
     and query_kind = 'Select'
     and Settings['min_bytes_to_use_direct_io'] = '1'
)
select queries.ordinal, thread_name, sum(ProfileEvents['OSReadBytes']) > 0 as disk_reading
from system.query_thread_log qtl
    join queries
        on queries.query_id = qtl.query_id
where current_database = currentDatabase()
  and query_id in (select query_id from queries)
  and ProfileEvents['OSReadBytes'] > 0
group by 1, 2
order by 1, 2
