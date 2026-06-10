WITH
    4096 AS w, 4096 AS h, w * h AS pixels,
    arrayJoin(coverage) AS num,
    num DIV (32768 * 32768 DIV pixels) AS idx,
    mortonDecode(2, idx) AS coord,
    255 AS b,
    least(255, uniq(test_name)) AS r,
    255 * uniq(test_name) / (max(uniq(test_name)) OVER ()) AS g
SELECT r::UInt8, g::UInt8, b::UInt8
FROM test
GROUP BY coord
ORDER BY coord.2 * w + coord.1
WITH FILL FROM 0 TO 10
