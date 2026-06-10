WITH (SELECT groupBitmapState(number::Nullable(UInt8)) as n from numbers(1)) as n SELECT number as x, bitmapContains(n, x) FROM numbers(10)
