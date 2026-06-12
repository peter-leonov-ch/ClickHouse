create table m as mt1 engine = Merge(currentDatabase(), '^(mt1|b)$')
