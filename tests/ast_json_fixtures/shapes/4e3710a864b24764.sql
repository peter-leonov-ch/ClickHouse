select (number % 2 ? null : '{"a" : 42}')::Nullable(JSON) as a from numbers(4) group by 1 order by a
