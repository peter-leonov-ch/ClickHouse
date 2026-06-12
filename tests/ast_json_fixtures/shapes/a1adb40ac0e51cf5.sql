CREATE TABLE mytable
(
    operand Float64,
    low     Float64,
    high     Float64,
    count   UInt64,
    PRIMARY KEY (operand, low, high, count)
) ENGINE = MergeTree()
