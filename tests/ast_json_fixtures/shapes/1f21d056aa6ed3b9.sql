select number-8 as offset, 8 as length, 'Hello' as s,        subString(bin(s), offset , length), bin(bitSlice(s, offset, length)) from numbers(9)
