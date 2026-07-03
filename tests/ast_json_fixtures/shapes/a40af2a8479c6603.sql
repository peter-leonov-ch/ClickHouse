select key, untuple(argMax((* except (key),), v1)) from kv group by key order by key format TSVWithNames
