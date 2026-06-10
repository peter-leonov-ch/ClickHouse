create table x1 (i Nullable(int)) engine MergeTree order by i desc primary key i settings allow_nullable_key = 1, index_granularity = 2, allow_experimental_reverse_key = 1
