select * from test group by i having i in (10, 11, 12) order by i limit 1 FORMAT JSONCompact
