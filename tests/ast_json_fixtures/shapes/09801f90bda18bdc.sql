INSERT INTO 02356_destination (a, b) SELECT * FROM generateRandom('a Int64, b String') LIMIT 100 SETTINGS max_threads=1, max_block_size=100
