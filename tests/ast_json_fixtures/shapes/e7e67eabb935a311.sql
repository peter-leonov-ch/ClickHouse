select indexOf((select arraySort(groupUniqArray(tid)) from system.transactions_info_log where database=currentDatabase() and table='txn_counters'), tid),
       type,
       thread_id!=0,
       length(query_id)=length(queryID()) or type='Commit' and query_id='',  
       tid_hash!=0,
       csn=0,
       part
from system.transactions_info_log
where tid in (select tid from system.transactions_info_log where database=currentDatabase() and table='txn_counters' and not (tid.1=1 and tid.2=1))
or (database=currentDatabase() and table='txn_counters') order by event_time
