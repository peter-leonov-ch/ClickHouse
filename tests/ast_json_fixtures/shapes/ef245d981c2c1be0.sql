with
    (select 25) as something
select *, something
from numbers(toUInt64(assumeNotNull(something)))
