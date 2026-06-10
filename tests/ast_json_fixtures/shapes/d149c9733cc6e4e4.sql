with (select uuid from system.tables where database = currentDatabase() and table = 'data_02491') as table_uuid_
select
    table_uuid != toUUIDOrDefault(Null),
    event_type,
    merge_reason,
    part_name
from system.part_log
where
    database = currentDatabase() and
    table = 'data_02491' and
    table_uuid = table_uuid_
order by event_time_microseconds
