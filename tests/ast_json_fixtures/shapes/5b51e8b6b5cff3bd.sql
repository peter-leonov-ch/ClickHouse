select number k, repeat(toString(number), 11) v1, repeat(toString(number), 12) v2 from numbers(3e6) order by v1, v2 limit 400e3 format Null
