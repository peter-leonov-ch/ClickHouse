INSERT INTO TABLE FUNCTION file(database() || '_test.csv', CSV, 'a Int, b Int DEFAULT 77') SELECT number, if(number%2=1, NULL, number) FROM numbers(3)
