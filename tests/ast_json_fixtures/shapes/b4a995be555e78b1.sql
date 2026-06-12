INSERT INTO TABLE FUNCTION file(database() || '.test-data.json', JSON)
    SELECT number numeric FROM numbers(10)
