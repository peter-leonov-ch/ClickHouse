create table t(A Int64) partition by (A % 64) order by A desc settings allow_experimental_reverse_key=1
as select intDiv(number,11111) from numbers(7e5) union all select number from numbers(7e5)
