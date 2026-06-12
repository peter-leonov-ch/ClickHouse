with
(
    select groupNumericIndexedVectorStateIf(uin, value, ds = '2023-12-20')
    from uin_value_details
) as vec_1,
(
    select groupNumericIndexedVectorStateIf(uin, value, ds = '2023-12-21')
    from uin_value_details
) as vec_2
, numericIndexedVectorPointwiseDivide(vec_1, vec_2) as vec_3
select arrayJoin([
    numericIndexedVectorShortDebugString(vec_1)
    , toString(numericIndexedVectorAllValueSum(vec_1))
    , numericIndexedVectorShortDebugString(vec_2)
    , toString(numericIndexedVectorAllValueSum(vec_2))
    , numericIndexedVectorShortDebugString(vec_3)
    , toString(numericIndexedVectorAllValueSum(vec_3))
])
