select t1.*, t2.*, t3.*
from table1 as t1
join table2 as t2 on table1.a = table2.a
join table3 as t3 on table2.b = table3.b
ORDER BY t1.a
FORMAT PrettyCompactNoEscapes
