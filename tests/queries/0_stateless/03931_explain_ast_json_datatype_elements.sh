#!/usr/bin/env bash

CURDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=../shell_config.sh
. "$CURDIR"/../shell_config.sh

# `EXPLAIN AST json = 1` data-type elements (format v2): the named slots that
# carry an enum's values and a named tuple's element names — ASTEnumDataType
# (`values`) and ASTTupleDataType (`element_names`) — plus the Nested
# NameTypePair elements and the auto-assigned-enum fallback to a generic
# DataType.

ast() {
    $CLICKHOUSE_CLIENT --format TSVRaw -q "EXPLAIN AST json = 1 $1"
}

echo "-- explicit Enum8 / Enum16 carry values"
ast "CREATE TABLE t (e8 Enum8('a' = 1, 'b' = 2), e16 Enum16('x' = -1, 'y' = 100))" \
    | jq -c '.. | objects | select(.type == "EnumDataType")'

echo "-- auto-assigned enum falls back to a generic DataType with literal args"
ast "CREATE TABLE t (e Enum8('a', 'b'))" \
    | jq -c '.. | objects | select(.type == "DataType" and .name == "Enum8")'

echo "-- named tuple carries element_names"
ast "CREATE TABLE t (c Tuple(a UInt8, b String))" \
    | jq -c '.. | objects | select(.type == "TupleDataType")'

echo "-- unnamed tuple omits element_names"
ast "CREATE TABLE t (c Tuple(UInt8, String))" \
    | jq -c '.. | objects | select(.type == "TupleDataType")'

echo "-- mixed tuple keeps a placeholder for the unnamed element"
ast "CREATE TABLE t (c Tuple(a UInt8, String))" \
    | jq -c '.. | objects | select(.type == "TupleDataType") | .element_names'

echo "-- Nested elements are NameTypePair nodes"
ast "CREATE TABLE t (n Nested(k UInt8, v String))" \
    | jq -c '.. | objects | select(.type == "DataType" and .name == "Nested") | [.arguments[] | {type, name, data_type: .data_type.name}]'

echo "-- Dynamic(max_types = N) argument"
ast "CREATE TABLE t (d Dynamic(max_types = 5))" \
    | jq -c '.. | objects | select(.type == "Function" and .name == "equals")'
