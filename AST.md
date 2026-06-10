# `EXPLAIN AST json = 1` — JSON dump of the parsed AST

`EXPLAIN AST json = 1 <query>` serializes the parsed (not analyzed) AST of
a query to JSON. It is implemented by `formatASTAsJSON` in
`src/Parsers/DumpASTNode.cpp` and wired into `InterpreterExplainQuery`.

This document is the contributor reference: how to run it, the output
contract, the per-class schema, and how to extend it to new AST nodes. It
consolidates the earlier design notes (formerly `AST2.md` / `AST3.md`).

## Running it

```bash
clickhouse local  --format TSVRaw -q "EXPLAIN AST json = 1 SELECT * FROM foo WHERE x = 1"
clickhouse client --format TSVRaw -q "EXPLAIN AST json = 1 SELECT * FROM foo WHERE x = 1"
```

`--format TSVRaw` matters: the explain result is a single string cell, and
the default TSV format escapes the embedded newlines as `\n`. `TSVRaw`
prints them literally so you get readable multi-line JSON. Pipe into `jq`
for querying:

```bash
clickhouse local --format TSVRaw -q "EXPLAIN AST json = 1 SELECT * FROM foo WHERE x = 1" \
  | jq '.selects[0].where'
```

The query only has to **parse** — referenced tables and columns need not
exist. `json` is mutually exclusive with the `graph` option of
`EXPLAIN AST`.

## Output contract

Every node is a JSON object with:

- `type` — the short class id (e.g. `"Function"`, `"Identifier"`,
  `"Literal"`), computed by `astTypeName`: `IAST::getID(' ')` trimmed at the
  first space (getID packs auxiliary data after a space delimiter). A few
  classes return a descriptive getID that *itself* contains a space
  (`"Dictionary lifetime"`); trimming those would collapse the five
  `Dictionary*` classes onto one `"Dictionary"`, so the sub-elements are
  spelled out explicitly in `astTypeName`.
- `alias` — only when the node has an alias.

`type` tells you the shape. Beyond that, a node exposes its sub-nodes in
one of two ways:

1. **Named slots** — most classes expose each meaningful sub-node under a
   named key (`arguments`, `where`, `table_expression`, ...). When a class
   does this it does **not** also emit `children`.
2. **`children` array** — kept only for genuinely *homogeneous lists*,
   where positionality carries no hidden meaning:
   - `ExpressionList`
   - `TablesInSelectQuery` (one element per JOIN)
   - the column lists under `COLUMNS(...)` / `* EXCEPT (...)` / `* REPLACE (...)`

So: if `type` is `"Function"` you read `arguments` / `parameters`; if it is
`"ExpressionList"` you read `children`.

Absent optional slots are omitted entirely (no `null`s). List-shaped
clauses **inline** their inner `ExpressionList` wrapper rather than emit it,
because the wrapper is a parser-internal detail.

## Implementation

`formatASTAsJSON(const IAST &)` builds the tree recursively. For each node
it writes `type` and `alias`, then calls:

```cpp
bool enrichNode(JSONBuilder::JSONMap & node, const IAST & ast);
```

`enrichNode` adds the per-class fields and returns `handled_children`. When
it returns `true`, `formatASTAsJSON` skips the generic `children` walk for
that node (everything interesting is already under a named slot). The
default is `return false`, so any class not special-cased keeps its
positional `children` array.

Two helpers (anonymous namespace):

- `inlineExpressionList(list)` — emit an `ASTExpressionList`'s children as a
  JSON array, dropping the wrapper. A null list yields `[]`.
- `addNodeSlot(node, key, child)` — emit a single sub-node under `key`, only
  when present.

### Dispatch ordering caveat

`enrichNode` is an `if / else if` chain of `dynamic_cast`s. Derived classes
must be matched **before** their base. In particular
`ASTSelectIntersectExceptQuery` derives from `ASTSelectQuery` but keeps its
operand selects in `children` (not in the `Expression` slots), so it is
matched first and deliberately returns `false` to keep `children`. Adding a
new subclass of an already-handled class without ordering it first will
silently route it through the base branch.

## Schema by node class

Scalar flags below are emitted only when set/non-default unless noted.

### Expressions

- **Function** (`arguments` always present, even if empty):
  `name`, `is_operator`, `is_window_function`, `is_lambda_function`,
  `kind`, `nulls_action`, `arguments` (inlined), `parameters` (inlined,
  parametric aggregates), `window_definition` *or* `window_name`.
  Operators and lambdas are ordinary functions: `a + b` → `name: "plus"`;
  a lambda's parameter-tuple and body come through `arguments`.
- **Identifier**: `name`, `name_parts` (when compound).
- **TableIdentifier**: `name` (short), `database` (when qualified).
- **Literal**: `value_type` (the `Field` type), `value` (native JSON for
  Null / Bool / UInt64 / Int64 / Float64 / String; stringified otherwise).
- **Asterisk**: `expression`, `transformers` (plain `*` is bare).
- **QualifiedAsterisk**: `qualifier`, `transformers`.
- **ColumnsRegexpMatcher**: `pattern`, `expression`, `transformers`.
- **ColumnsListMatcher**: `expression`, `columns` (inlined), `transformers`.
- **ColumnsApplyTransformer**: `func_name`, `parameters`, `lambda`,
  `lambda_arg`, `column_name_prefix`.
- **ColumnsExceptTransformer** / **ColumnsReplaceTransformer**: `is_strict`
  (columns / replacements stay in `children`).
- **ColumnsReplaceTransformer::Replacement**: `name` (expr stays as child).

### Clauses and queries

- **SelectQuery**: scalar flags `distinct`, `group_by_all`,
  `group_by_with_totals` / `_rollup` / `_cube` / `_grouping_sets`,
  `order_by_all`, `recursive_with`; then one slot per clause —
  `with`, `select`, `tables`, `prewhere`, `where`, `group_by`, `having`,
  `window`, `qualify`, `order_by`, `limit_by_offset`, `limit_by_length`,
  `limit_by`, `limit_offset`, `limit_length`, `settings`, `interpolate`.
  List-shaped clauses are inlined; the rest are nodes. `ALIASES` /
  `CTE_ALIASES` are analyzer state and never present on parsed ASTs, so
  they are omitted.
- **SelectWithUnionQuery**: `union_mode` (non-default only), `selects`
  (inlines `list_of_selects`).
- **SelectIntersectExceptQuery**: `operator`; operand selects stay in
  `children`.
- **Subquery**: `cte_name`, `query` (its single child).
- **WithElement**: `name`, `subquery`, `aliases`.

### FROM / JOIN

- **TablesInSelectQueryElement**: `table_join`, `table_expression`,
  `array_join`.
- **TableExpression**: `database_and_table_name` / `table_function` /
  `subquery` (one of), `final`, `sample_size`, `sample_offset`,
  `column_aliases`.
- **TableJoin**: `kind` (always), `strictness`, `locality`, `using`
  (inlined), `on`.
- **ArrayJoin**: `kind` (`INNER` / `LEFT`), `expressions` (inlined).

### ORDER BY / WINDOW / misc

- **OrderByElement**: `expression`, `direction`, `nulls_first`, `with_fill`,
  `collation`, `fill_from`, `fill_to`, `fill_step`, `fill_staleness`.
- **WindowListElement**: `name`, `definition`.
- **WindowDefinition**: `parent_window_name`, `partition_by` (inlined),
  `order_by` (inlined), and — when the frame is non-default — `frame_type`,
  `frame_begin_type`, `frame_begin_offset`, `frame_begin_preceding`,
  `frame_end_type`, `frame_end_offset`, `frame_end_preceding`.
- **InterpolateElement**: `column`, `expr`.
- **Set** (the `SETTINGS` clause / `ASTSetQuery`): `changes` (name → value
  object), `default_settings`.
- **SampleRatio**: `numerator`, `denominator` (exact rationals, stringified
  since they may exceed `UInt64`).

### DDL / DML

- **CreateQuery**: the `attach` / `temporary` / `if_not_exists` /
  `is_*_view` / `is_dictionary` / `replace_*` / `create_or_replace` flags
  (when set), `database`, `table`, `columns_list`, `aliases`, `storage`,
  `as_table_function`, `as_database` / `as_table`, `select`, `targets`,
  `comment`, `dictionary_attributes`, `dictionary`.
- **Columns** (`ASTColumns`): `columns`, `indices`, `constraints`,
  `projections` (inlined), `primary_key`, `primary_key_from_columns`.
- **ColumnDeclaration**: `name`, `data_type`, `default_specifier`,
  `default_expression`, `null_modifier`, `ephemeral_default`,
  `primary_key_specifier`, `comment`, `codec`, `statistics`, `ttl`,
  `collation`, `settings`.
- **DataType**: `name`, `arguments` (inlined, e.g. `Decimal(10, 2)`).
- **Storage**: `engine`, `partition_by`, `primary_key`, `order_by`,
  `sample_by`, `ttl_table`, `settings`.
- **InsertQuery**: `database`, `table`, `table_function`, `columns`,
  `format`, `partition_by`, `settings`, `select`, `infile`, `compression`.
- **Index**: `name`, `expression`, `index_type`, `granularity`.
- **Constraint**: `name`, `constraint_type` (`CHECK` / `ASSUME`),
  `expression`.
- **Projection**: `name`, `query`, `index`.
- **ProjectionSelectQuery**: `with`, `select`, `group_by`, `order_by`
  (inlined).
- **TTLElement**: `mode` (`DELETE` / `MOVE` / `GROUP_BY` / `RECOMPRESS`),
  `ttl`, and for `MOVE` the `destination_type` / `destination_name` /
  `if_exists`; `where`, `recompression_codec`.
- **Partition**: `all`, `value`, `id`.
- **DeleteQuery**: `database`, `table`, `cluster`, `partition`, `predicate`.
- **UpdateQuery**: `database`, `table`, `cluster`, `assignments`,
  `predicate`, `partition`.
- **DropQuery** (also `DetachQuery` / `TruncateQuery` by `type`): `kind`,
  `database`, `table`, `cluster`, `if_exists` / `if_empty` / `is_dictionary`
  / `is_view` / `sync` / `permanently`, `database_and_tables`.
- **OptimizeQuery**: `database`, `table`, `cluster`, `partition`, `final`,
  `deduplicate`, `deduplicate_by_columns`, `cleanup`.
- **Assignment**: `column`, `expression`.
- **AlterQuery**: `alter_object` (`TABLE` / `DATABASE`), `database`, `table`,
  `cluster`, `commands` (inlined).
- **AlterCommand**: `command_type` (the full `ALTER` verb, e.g. `ADD_COLUMN`,
  `MOVE_PARTITION`, `MODIFY_TTL`), the relevant flags, and whichever sub-node
  slots apply — `column_declaration`, `column`, `order_by`, `index_declaration`,
  `partition`, `predicate`, `assignments`, `comment`, `ttl`, `settings_changes`,
  `select`, `rename_to`, ... plus the `from*` / `to*` / `move_destination_name`
  strings.
- **CreateFunctionQuery**: `or_replace`, `if_not_exists`, `function_name`,
  `function_core`.
- **Dictionary** (`ASTDictionary`): `primary_key` (inlined), `source`,
  `lifetime`, `layout`, `range`, `settings`.
- **DictionaryAttributeDeclaration**: `name`, `data_type`, `default_value`,
  `expression`, `hierarchical`, `bidirectional`, `injective`, `is_object_id`.
- **FunctionWithKeyValueArguments**: `name`, `elements` (inlined `pair`s).
- **pair** (`ASTPair`): `key`, `value`.
- the dictionary sub-elements — layout (`layout_type`, `parameters`),
  lifetime (`min_sec`, `max_sec`), range (`min_attr_name`, `max_attr_name`),
  settings (`changes`).
- **ViewTargets**: `targets` (array of `{kind, database, table, inner_engine,
  table_ast}`).

The dictionary sub-elements get explicit `type` ids — `DictionaryLayout`,
`DictionaryLifetime`, `DictionaryRange`, `DictionarySettings` — set in
`astTypeName`. Their `getID`s (`"Dictionary lifetime"`, ...) would otherwise
all trim at the first space to the same `"Dictionary"` as the container; see
the note under "Output contract".

### Other statements and elements

- **Explain**: `kind`, `query`, `settings`, `table_function`, `table_override`.
- **DescribeQuery**: `table_expression`.
- **ShowTables**: the flags (`databases` / `dictionaries` / `temporary` /
  `full` / ...), `from`, `like`, `not_like`.
- **CreateIndexQuery** / **DropIndexQuery**: table target, `if_not_exists` /
  `unique` / `if_exists`, `index_name`, `index_declaration`.
- **CheckQuery**: table target, `partition`, `part_name`.
- **UseQuery**: `database`.
- **KillQueryQuery**: `kill_type`, `sync`, `test`, `cluster`, `where`.
- **Rename**: `exchange` / `database` / `dictionary` flags, `cluster`,
  `elements` (array of `{from_database, from_table, to_database, to_table,
  if_exists}`).
- **SYSTEM** (`ASTSystemQuery`): `system_type`, `database`, `table`,
  `cluster`, `replica`, `shard`, `target_model`, `target_function`.
- **Stat** (`ASTStatisticsDeclaration`): `columns`, `types`.
- **StorageOrderByElement**: `direction` (sort expression stays in `children`).
- **NameTypePair**: `name`, `data_type`.
- **QualifiedColumnsRegexpMatcher** / **QualifiedColumnsListMatcher**:
  `pattern` / `columns`, `qualifier`, `transformers`.

A generic fallback over `ASTQueryWithTableAndOutput` gives `database` /
`table` / `temporary` to every other simple table-scoped statement that
carries only a target — `EXISTS *`, `SHOW CREATE *`, `UNDROP TABLE`, etc.

### Not yet enriched

The remaining tail still exposes positional `children`: the
access-management statements (`CreateUserQuery`, `AuthenticationData`,
`ShowGrantsQuery`, workloads/resources, ...) and `BACKUP` / `RESTORE`.
`ParallelWithQuery` keeps `children` deliberately — it is a homogeneous list
of parallel sub-queries. The JSON stays valid; only those subtrees are
positional.

## Adding a new node class

1. In `enrichNode`, add an `else if (const auto * x = dynamic_cast<const ASTYourNode *>(&ast))`
   branch (before its base class if it derives from a handled one).
2. Emit scalars with `node.add`, list slots with `inlineExpressionList`,
   single sub-nodes with `addNodeSlot` / `formatASTAsJSON`.
3. `return true` to suppress `children` — unless the node legitimately
   carries a homogeneous list you want to keep there, in which case emit
   only the scalars and fall through to `return false`.
4. Add the `#include` for the node header.
5. Extend the doc list in `DumpASTNode.h` and add a case to the relevant
   test below.

This is a deliberate schema break versus the original opaque `children`
dump; acceptable because `EXPLAIN AST json = 1` is new and undocumented.
Inlining `ExpressionList` wrappers loses the (query-unobservable)
distinction between an empty list and an absent clause.

## Tests

`tests/queries/0_stateless/`:

- `03917_explain_ast_json_function` — Function slots.
- `03918_explain_ast_json_order_by` — OrderByElement slots.
- `03919_explain_ast_json_select_query` — SelectQuery clauses.
- `03920_explain_ast_json_expressions` — literals, lambda, operators, CAST,
  NULLS action, compound identifiers.
- `03921_explain_ast_json_union` — UNION modes, INTERSECT / EXCEPT.
- `03922_explain_ast_json_showcase` — one maximal query dumped in full (no
  node filtering); doubles as a readable example of the format.
- `03923_explain_ast_json_wrappers` — joins, array join, table expressions,
  subquery collapse, window frames, interpolate.
- `03924_explain_ast_json_leaf_state` — SETTINGS, SAMPLE, asterisks and
  COLUMNS matchers / transformers.

The tests pipe through `jq` (`--format TSVRaw`) to keep references readable
and focused.
