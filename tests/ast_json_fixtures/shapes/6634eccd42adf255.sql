with now() - interval 1 hour as cutoff_time,
query_ids as
(
    select query_id from system.query_log where current_database=currentDatabase() and event_time>=cutoff_time
)
select type, has_watch, op_num, replace(path, toString(serverUUID()), '<uuid>'), is_ephemeral, is_sequential, if(startsWith(path, '/clickhouse/sessions'), 1, version), requests_size, request_idx, error, watch_type,
       watch_state, path_created, stat_version, stat_cversion, stat_dataLength, stat_numChildren
from system.zookeeper_log
where event_time>=cutoff_time and (session_id, xid) in (
    select session_id, xid from system.zookeeper_log where event_time>=cutoff_time
    and path='/test/01158/' || currentDatabase() || '/rmt/replicas/1/parts/all_0_0_0'
    and (query_id='' or query_id in query_ids)
)
order by xid, type, request_idx
