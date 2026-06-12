WITH [
(1600000000, 10),
(1600000010, 20),
(1600000020, 30),
(1600000030, 40),
(1600000040, 10),
(1600000050, 70),
(1600000060, 90),
(1600000270, 20),
(1600000330, 10)
]::Array(Tuple(UInt32, Float64)) AS data
SELECT * FROM (
    SELECT 'delta' as name, timeSeriesDeltaToGrid(1600000010, 1600000320, 10, 300)(data.1, data.2)
    UNION ALL
    SELECT 'idelta' as name, timeSeriesInstantDeltaToGrid(1600000010, 1600000320, 10, 300)(data.1, data.2)
) ORDER BY name
