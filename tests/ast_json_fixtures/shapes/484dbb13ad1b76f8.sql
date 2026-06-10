create table tp_1 (x Int32, y Int32, projection p (select x, y order by x)) engine = MergeTree order by y partition by intDiv(y, 100) settings old_parts_lifetime=1
