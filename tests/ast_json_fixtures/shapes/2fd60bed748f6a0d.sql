WITH alias_1 AS
   (SELECT c1,c2 FROM distributed_bug_table)
SELECT c1 from alias_1 where c2 IN (SELECT DISTINCT c2 from alias_1)
FORMAT Null
