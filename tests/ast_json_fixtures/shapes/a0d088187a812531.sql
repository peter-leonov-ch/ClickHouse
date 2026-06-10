SELECT toInt8(number / 5 + 100) AS x FROM remote('127.0.0.1', system.numbers) LIMIT 2 BY x LIMIT 5
