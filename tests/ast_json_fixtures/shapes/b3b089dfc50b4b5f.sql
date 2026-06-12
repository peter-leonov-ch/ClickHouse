CREATE TABLE ttl (d DateTime) ENGINE = MergeTree ORDER BY tuple() TTL d + INTERVAL 10 DAY SETTINGS remove_empty_parts=0
