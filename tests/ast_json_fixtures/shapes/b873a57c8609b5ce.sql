WITH (WatchID % 2 == 0) AS predicate
SELECT
    minIf(WatchID, predicate) as min_watch_id,
    maxIf(WatchID, predicate),
    sumIf(WatchID, predicate),
    avgIf(WatchID, predicate),
    avgWeightedIf(WatchID, CounterID, predicate),
    countIf(WatchID, predicate),
    groupBitOrIf(WatchID, predicate),
    groupBitAndIf(WatchID, predicate),
    groupBitXorIf(WatchID, predicate)
FROM test.hits
ORDER BY min_watch_id
DESC LIMIT 20
