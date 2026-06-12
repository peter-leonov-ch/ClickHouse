# SQL → JSON-AST fixtures

A static, vendorable corpus mapping SQL queries to their parsed AST as
produced by `EXPLAIN AST json = 1` (see `../../AST.md`). Intended as golden
files for verifying an alternative parser / AST serializer against the
reference ClickHouse implementation.

The `.json` files are the full document, `{ "version": N, "ast": {...} }`;
pin against `version` so a schema break is detectable. 64-bit integer
literal values are JSON strings (see `../../AST.md`).

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
homogeneous lists (`ExpressionList`, `TablesInSelectQuery`). Absent optional
fields are omitted (no `null`s). The queries only need to parse — referenced
tables / columns do not have to exist.

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

## Bulk corpus from the stateless tests

The 41 curated cases above are hand-picked for readability. For a much
larger, real-world corpus you can reuse the existing stateless test suite:
`tests/queries/0_stateless/*.sql` holds ~124k statements. `harvest_stateless.py`
extracts them, runs each through `EXPLAIN AST json = 1`, and writes the
SQL → JSON pairs that parse:

```bash
CLICKHOUSE_BINARY=/path/to/clickhouse ./harvest_stateless.py --write --out harvested
```

- ~94k statements remain after dropping query-parameter placeholders
  (`{x:UInt8}`, which the reference parser cannot substitute without values)
  and de-duplicating. Essentially all of them parse — observed yield ~100%.
- It batches statements through one `clickhouse local` process per batch
  (`--batch`, default 500) with `--multiquery --ignore-error`; since
  `EXPLAIN AST` only parses and never executes, this is side-effect-free and
  resynchronises past any reject. A full harvest takes well under a minute.
- Output filenames are content hashes; pass `--limit N` for a quick sample
  and `--batch 1` to fall back to one process per statement.

The full harvested corpus is **not committed** (tens of thousands of files);
generate it locally when you need it. Note that non-SELECT statements
(DDL/DML such as `InsertQuery`, `CreateQuery`) appear too — their nodes are
not specially enriched yet, so they fall back to the positional `children`
array, but the JSON is still valid and reflects the reference parser.

### Shape dedup — `shapes/`

Text de-dup still leaves ~86k statements (`SELECT a` and `SELECT b` are
distinct). `--dedupe shape` instead keeps one representative per distinct
*AST shape*: a structural signature of node types, slot keys, enum/flag
scalars and literal `value_type`, dropping leaf values (names, literals,
aliases, settings contents). The representative kept is the shortest
statement for that shape, so the selection is deterministic.

`--shape-depth D` caps the signature depth for coarser dedup. Observed
counts over the stateless corpus:

| depth | distinct shapes |
|------:|----------------:|
| 2 | 155 |
| 3 | 1,440 |
| 4 | 5,141 |
| 5 | 9,797 |
| ∞ (0) | 16,733 |

The committed `shapes/` directory is the **depth-3** set — 1,440
structurally-distinct, shortest-representative pairs (~12 MB), a compact
high-coverage corpus for parser conformance. Regenerate with:

```bash
CLICKHOUSE_BINARY=/path/to/clickhouse \
    ./harvest_stateless.py --dedupe shape --shape-depth 3 --out shapes --write
```

Use a larger `--shape-depth` (or `0`) for finer coverage, smaller for a
quick smoke set.
