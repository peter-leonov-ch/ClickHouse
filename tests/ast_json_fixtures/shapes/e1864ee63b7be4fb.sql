CREATE TABLE hits_buffer AS hits_dst ENGINE = Buffer(current_database(), hits_dst, 8, 600, 600, 1000000, 1000000, 100000000, 1000000000)
