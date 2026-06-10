CREATE TABLE test_table (EventDate Date, CounterID UInt32,  UserID UInt64,  EventTime DateTime('America/Los_Angeles'), UTCEventTime DateTime('UTC')) PARTITION BY EventDate PRIMARY KEY CounterID
