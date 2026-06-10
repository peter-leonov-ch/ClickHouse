SELECT a, c, 'original' as original FROM with_fill_staleness ORDER BY a ASC WITH FILL INTERPOLATE (c)
