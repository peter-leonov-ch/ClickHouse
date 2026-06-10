select number, count(*) over (partition by p)
    from window_mt order by number limit 10 settings optimize_read_in_order = 0
