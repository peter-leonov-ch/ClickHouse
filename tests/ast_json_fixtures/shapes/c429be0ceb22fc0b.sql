select 'intersect', countSubstringsCaseInsensitiveUTF8(concat(char(number), repeat('я', 8)), 'яЯ') from numbers(100) where number = 0x41 format CSV
