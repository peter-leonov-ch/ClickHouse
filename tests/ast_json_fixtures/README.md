# SQL → JSON-AST fixtures

A static corpus mapping SQL queries to their parsed AST as produced by
`EXPLAIN AST json = 1` (see `../../AST.md`). Intended as golden files for
verifying an alternative parser / AST serializer against the reference
ClickHouse implementation.

## Layout

```
cases/
  01_literals.sql       01_literals.json
  02_identifiers.sql    02_identifiers.json
  ...
  41_showcase.sql       41_showcase.json
generate.sh
```

Each case is a pair sharing a basename:

- `<name>.sql`  — one input statement (the source of truth).
- `<name>.json` — the expected AST: the exact, pretty-printed output of
  `EXPLAIN AST json = 1 <that query>`.

The corpus is deliberately broad: literals and operators, functions /
lambdas / parametric aggregates, window functions (named and inline),
every SELECT clause, joins (ON / USING / GLOBAL ANY / CROSS / COMMA),
ARRAY JOIN, table functions and subqueries, SAMPLE / FINAL, CTEs and set
operations, asterisk transformers and `COLUMNS(...)`, and one maximal
`41_showcase` query that exercises most node types at once.

## Output conventions

The JSON shape is documented in `../../AST.md`. In short: each node has a
`type`; sub-nodes appear under named slots; `children` survives only on
homogeneous lists (`ExpressionList`, `TablesInSelectQuery`, and the column
lists under `COLUMNS` / `EXCEPT` / `REPLACE`). Absent optional fields are
omitted (no `null`s). The queries only need to parse — referenced tables /
columns do not have to exist.

## Verifying your parser

For each `.sql`, run your parser, serialize to the same JSON shape, and
compare against the matching `.json`. Compare structurally (parse both
sides and diff the trees) rather than byte-for-byte, so key order and
whitespace don't cause spurious failures:

```bash
for sql in cases/*.sql; do
    name=$(basename "$sql" .sql)
    mine=$(my-parser --json "$sql")
    if ! diff <(jq -S . <<<"$mine") <(jq -S . "cases/$name.json") >/dev/null; then
        echo "MISMATCH: $name"
    fi
done
```

(`jq -S` sorts object keys for order-insensitive comparison.)

## Regenerating

The `.json` files are generated; regenerate them when the serializer
changes:

```bash
CLICKHOUSE_BINARY=/path/to/clickhouse ./generate.sh
```

Add a new case by adding one `emit <name> "<sql>"` line to `generate.sh`
and rerunning. These fixtures are a standalone corpus and are not wired
into ClickHouse CI.
