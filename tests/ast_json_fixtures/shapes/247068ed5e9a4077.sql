CREATE TABLE IF NOT EXISTS merge_hits AS test.hits ENGINE = Merge(test, '^hits$')
