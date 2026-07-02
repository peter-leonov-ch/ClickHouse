WITH
    arrayMap(x -> toFloat32(x) / 2, range(384)) AS a32,
    arrayMap(x -> toBFloat16(x) / 2, range(384)) AS a16,
    arrayMap(x -> x + 1, a32) AS a32_1,
    arrayMap(x -> x + 1, a16) AS a16_1
SELECT a32, a16, a32_1, a16_1,
    dotProduct(a32, a32_1), dotProduct(a16, a16_1),
    cosineDistance(a32, a32_1), cosineDistance(a16, a16_1),
    L2Distance(a32, a32_1), L2Distance(a16, a16_1),
    L1Distance(a32, a32_1), L1Distance(a16, a16_1),
    LinfDistance(a32, a32_1), LinfDistance(a16, a16_1),
    LpDistance(a32, a32_1, 5), LpDistance(a16, a16_1, 5)
FORMAT Vertical
