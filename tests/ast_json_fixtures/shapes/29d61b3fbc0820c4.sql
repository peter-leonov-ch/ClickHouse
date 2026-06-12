CREATE DICTIONARY IF NOT EXISTS dict (x UInt32, y UInt32) primary key x source(clickhouse(table 'tab')) LAYOUT(FLAT()) LIFETIME(MIN 0 MAX 1000)
