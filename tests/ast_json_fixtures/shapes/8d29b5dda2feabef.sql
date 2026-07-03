CREATE TABLE x UUID 'aaaaaaaa-1111-2222-3333-aaaaaaaaaaaa' (key Int) ENGINE = ReplicatedMergeTree('/tables/{database}/{uuid}', 'r1') ORDER BY tuple()
