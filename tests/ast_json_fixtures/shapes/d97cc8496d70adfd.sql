select number k, repeat(toString(number), 11) v1, repeat(toString(number), 12) v2 from numbers(3e6) order by k limit 400e3 settings remerge_sort_lowered_memory_bytes_ratio=1.9 format Null
