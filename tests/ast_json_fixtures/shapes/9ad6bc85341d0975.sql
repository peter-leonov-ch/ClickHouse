create table test_in_tuple as test_in_tuple_1 engine = Merge(currentDatabase(), '^test_in_tuple_[0-9]+$')
