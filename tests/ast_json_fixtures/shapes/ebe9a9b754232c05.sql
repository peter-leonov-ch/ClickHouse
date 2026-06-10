create table ttl_00933_1 (d DateTime, a Int)
    engine = MergeTree order by tuple() partition by tuple() ttl d + interval 1 day
    settings remove_empty_parts = 0
