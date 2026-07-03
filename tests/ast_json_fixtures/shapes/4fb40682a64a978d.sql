SELECT c1, toTypeName(c1), c2, toTypeName(c2) FROM format('CSV', '1,Hello World!\n2,"[1,2,3]"\n3,"2020-01-01"\n') FORMAT Pretty
