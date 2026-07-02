select uid, windowFunnel(100, 'strict_order')(dt, event='a', event='b', event='c') as res
from funnel_test_reentry where uid = 2 group by uid format JSONCompactEachRow
