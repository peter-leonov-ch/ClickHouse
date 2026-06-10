select number, intDiv(number, 2) p, mod(number, 3) o, count(number) over w as c
from numbers(31)
window w as (partition by p order by o range unbounded preceding)
order by number
settings max_block_size = 5
