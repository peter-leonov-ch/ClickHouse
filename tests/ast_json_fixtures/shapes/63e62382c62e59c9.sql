WITH (WatchID % 2 == 0) AS predicate
SELECT
    CounterID,
    minIf(WatchID,predicate),
    maxIf(WatchID, predicate),
    sumIf(WatchID, predicate),
    avgIf(WatchID, predicate),
    avgWeightedIf(WatchID, CounterID, predicate),
    countIf(WatchID, predicate),
    groupBitOrIf(WatchID, predicate),
    groupBitAndIf(WatchID, predicate),
    groupBitXorIf(WatchID, predicate)
FROM test.hits
GROUP BY CounterID ORDER BY count() DESC LIMIT 20
