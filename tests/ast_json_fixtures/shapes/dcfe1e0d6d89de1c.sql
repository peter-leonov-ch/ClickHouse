select t.*, s.a, s.b, s.c from t left join s on (s.a = t.a and s.b = t.b) SETTINGS join_use_nulls = 1
