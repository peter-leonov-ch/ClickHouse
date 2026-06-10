WITH neighbor(toString(number), toInt64(rand64())) AS x SELECT * FROM system.numbers WHERE NOT ignore(x)
