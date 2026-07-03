select number as x, number % 3 as y, 'Hello' as z from numbers(5) format SQLInsert settings output_format_sql_insert_use_replace=1
