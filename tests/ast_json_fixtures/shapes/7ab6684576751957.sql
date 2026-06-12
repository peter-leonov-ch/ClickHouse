SELECT distinct(marks) d from system.parts WHERE table = 'six_rows_per_granule' and database=currentDatabase() and active=1 having d > 0
