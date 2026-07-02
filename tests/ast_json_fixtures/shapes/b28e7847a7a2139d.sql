select t1.a, t2.a, t2.b, t3.b, t3.c, t5.a, t5.b, t5.c
from table1 as t1
join table2 as t2 on t1.a = t2.a
join table3 as t3 on t2.b = t3.b
join table5 as t5 on t3.c = t5.c
ORDER BY t1.a
FORMAT PrettyCompactNoEscapes
