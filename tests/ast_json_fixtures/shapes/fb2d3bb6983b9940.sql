SELECT sum(a) FROM remote('127.0.0.4', currentDatabase(), '02863_delayed_source') WITH TOTALS SETTINGS extremes = 1
