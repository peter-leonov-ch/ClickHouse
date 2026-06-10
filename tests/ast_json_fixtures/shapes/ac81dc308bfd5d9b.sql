SELECT k, sipHash64(v) FROM t1 order by k limit 5 offset 998 SETTINGS optimize_read_in_order=1, log_comment='02898_inorder_190aed82-2423-413b-ad4c-24dcca50f65b'
