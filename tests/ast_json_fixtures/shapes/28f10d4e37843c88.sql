CREATE TABLE arr_tests_visits
(
    CounterID        UInt32,
    StartDate        Date,
    Sign             Int8,
    VisitID          UInt64,
    UserID           UInt64,
    VisitVersion     UInt16,
    `Test.BannerID` Array(UInt64),
    `Test.Load`     Array(UInt8),
    `Test.PuidKey`  Array(Array(UInt8)),
    `Test.PuidVal`  Array(Array(UInt32))
) ENGINE = MergeTree() PARTITION BY toMonday(StartDate) ORDER BY (CounterID, StartDate, intHash32(UserID), VisitID) SAMPLE BY intHash32(UserID) SETTINGS index_granularity = 8192
