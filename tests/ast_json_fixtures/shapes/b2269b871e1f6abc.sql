CREATE TABLE nums_buf AS nums ENGINE = Buffer(currentDatabase(), nums, 1, 10, 100, 1, 3, 10000000, 100000000)
