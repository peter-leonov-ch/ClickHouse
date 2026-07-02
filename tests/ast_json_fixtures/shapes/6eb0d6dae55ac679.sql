CREATE TABLE ".inner_id.e15f3ab5-6cae-4df3-b879-f40deafd82c2" UUID '3bd68e3c-2693-4352-ad66-a66eba9e345e' (n Int32, n2 Int64) ENGINE = MergeTree PARTITION BY n % 10 ORDER BY n
