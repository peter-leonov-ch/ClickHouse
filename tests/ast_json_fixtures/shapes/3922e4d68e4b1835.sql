select
    min(number) over wa, min(number) over wo,
    max(number) over wa, max(number) over wo
from
    (select number, intDiv(number, 3) p, mod(number, 5) o
        from numbers(31))
window
    wa as (partition by p order by o
        range between unbounded preceding and unbounded following),
    wo as (partition by p order by o
        rows between unbounded preceding and unbounded following)
settings max_block_size = 2
