select -44 as offset, number + 40 as length, 'Hello' as s,       subString(bin(s), offset , length), bin(bitSlice(s, offset, length)) from numbers(9)
