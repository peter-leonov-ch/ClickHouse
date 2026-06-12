select number as offset, -number as length, 'Hello' as s,       subString(bin(s), offset , length), bin(bitSlice(s, offset, length)) from numbers(16)
