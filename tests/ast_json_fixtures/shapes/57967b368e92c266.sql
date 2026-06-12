INSERT INTO TABLE FUNCTION file(database() || '_test.csv', CSV, 'a Int, b Int DEFAULT 77') VALUES (3, 3), (4, NULL)
