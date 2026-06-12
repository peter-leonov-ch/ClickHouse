with now() - interval 1 hour as cutoff_time,
query_ids as
(
    select query_id from system.query_log where current_database=currentDatabase() and event_time>=cutoff_time
)
select type, has_watch, op_num, path, is_ephemeral, is_sequential, version, requests_size, request_idx, error, watch_type,
       watch_state, path_created, stat_version, stat_cversion, stat_dataLength, stat_numChildren
from system.zookeeper_log
where event_time>=cutoff_time and (session_id, xid) in (
    select session_id, xid from system.zookeeper_log where event_time>=cutoff_time
    and path like '/test/01158/' || currentDatabase() || '/rmt/blocks/%'
    and op_num not in (1, 12, 500)
    and (query_id='' or query_id in query_ids)
)
order by xid, type, request_idx
