SELECT d, dynamicType(d), d.Int64, d.String, d.Date, d.Float64, d.DateTime, d.`Array(Int64)`, d.`Array(String)`
FROM test_rapid_schema FORMAT PrettyCompactMonoBlock
