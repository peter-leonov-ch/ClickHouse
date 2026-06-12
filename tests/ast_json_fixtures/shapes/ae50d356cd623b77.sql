CREATE TABLE t4
(
    `n` Int8
)
ENGINE = Kafka
SETTINGS
    kafka_broker_list = 'localhost:10000',
    kafka_topic_list = 'test',
    kafka_group_name = 'test',
    kafka_format = 'JSONEachRow'
COMMENT 'this is a Kafka table'
