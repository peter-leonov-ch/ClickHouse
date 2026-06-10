SELECT count(), sum(v) FROM t_lightweight_mut_6 PREWHERE id % 5 = 0 SETTINGS apply_mutations_on_fly = 0
