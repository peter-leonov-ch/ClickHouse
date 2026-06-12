select 
    uid, name
    ,sum(age)
    ,count()
    ,arrayUniq(groupArray(ts))
    ,max(age)
    ,max(ts)
from users
group by grouping sets 
(
    (*),
    ()
)
ORDER BY ALL
