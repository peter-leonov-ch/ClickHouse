CREATE DICTIONARY IF NOT EXISTS decimal_dict (
	KeyField UInt64 DEFAULT 9999999,
	Decimal32_ Decimal(5,4) DEFAULT 0.11,
	Decimal64_ Decimal(18,8) DEFAULT 0.11,
	Decimal128_ Decimal(25,8) DEFAULT 0.11

)
PRIMARY KEY KeyField
SOURCE(CLICKHOUSE(HOST 'localhost' PORT tcpPort() USER 'default' TABLE 'table_decimal_dict' DB current_database()))
LIFETIME(0) LAYOUT(SPARSE_HASHED)
