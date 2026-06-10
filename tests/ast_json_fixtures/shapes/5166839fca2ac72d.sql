ALTER TABLE test_alter_fail 
    DROP COLUMN c0, 
    COMMENT COLUMN c0 'this should fail'
