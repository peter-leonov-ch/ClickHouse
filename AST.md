# `EXPLAIN AST json = 1` — JSON dump of the parsed AST

`EXPLAIN AST json = 1 <query>` serializes the parsed (not analyzed) AST of
a query to JSON. It is implemented by `formatASTAsJSON` in
`src/Parsers/DumpASTNode.cpp`; `InterpreterExplainQuery` calls
`formatASTAsJSONDocument`, which wraps the result in the versioned document.

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
  | jq '.ast.selects[0].where'
```

The query only has to **parse** — referenced tables and columns need not
exist. `json` is mutually exclusive with the `graph` option of
`EXPLAIN AST`.

## Document wrapper and version

The top-level value is a versioned document, not the root node directly:

```json
{ "version": 2, "ast": { "type": "SelectWithUnionQuery", ... } }
```

`version` (`AST_JSON_FORMAT_VERSION` in `DumpASTNode.h`) is bumped on any
backwards-incompatible change to the JSON shape, so external consumers that
pin reference fixtures can detect breaks. The AST itself is under `ast`.

## Downstream consumer — clickhouse-js-parser

This format is consumed by the TypeScript parser at
<https://github.com/ClickHouse/clickhouse-js-parser>, which pins the
fixtures in `tests/ast_json_fixtures` as a reference suite (vendored
alongside its existing `clickhouse-reference-{ast,format,explain,round-trip}`
suites). Several format decisions exist specifically to line up with it, and
should not be "simplified" away without coordinating there:

- **Versioned document.** The js parser pins fixtures, so any break must be
  detectable — hence `{ version, ast }` and the `AST_JSON_FORMAT_VERSION`
  bump-on-break rule above.
- **64-bit integers as strings.** Its AST stores `Literal.value` as `string`
  because JS `JSON.parse` corrupts integers above 2^53; see the contract
  below.
- **Named slots mirror its AST types.** The slot names and shapes track the
  node types in the parser's `src/ast.ts` — e.g. `QueryParameter` nodes
  (including in identifier/table position, `Identifier = string | QueryParam`),
  `LimitByClause { count, by, offset }` ↔ our `limit_by { length, offset?,
  by }`, the `WindowFrameBound` discriminated union ↔ our `frame_begin` /
  `frame_end` `{ type, offset?, preceding? }`, and the except/replace/apply
  `ColumnTransformer` types ↔ our inlined `transformers` array.
- **Operator-name normalization** matches its text-explain serializer's
  operator map (`src/explain.ts`); see "Known divergences" below.

When changing the format: bump `AST_JSON_FORMAT_VERSION`, update the `039*`
stateless tests and regenerate `tests/ast_json_fixtures`, and flag the change
so the js parser can re-vendor and pin the new version.

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

So: if `type` is `"Function"` you read `arguments` / `parameters`; if it is
`"ExpressionList"` you read `children`.

Absent optional slots are omitted entirely (no `null`s). List-shaped
clauses **inline** their inner `ExpressionList` wrapper rather than emit it,
because the wrapper is a parser-internal detail.

64-bit integer literal values (`UInt64` / `Int64`) — and settings values in
the `Settings` node's `changes` map — are emitted as JSON **strings**, not
numbers: values above 2^53 would lose precision when a JavaScript consumer
runs them through `JSON.parse`. `value_type` still says how to interpret the
string.

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

`formatASTAsJSONDocument` wraps the result of `formatASTAsJSON` in the
`{ version, ast }` document; it is the entry point used by the interpreter.

### Dispatch ordering caveat

`enrichNode` is an `if / else if` chain of `dynamic_cast`s. Derived classes
must be matched **before** their base. In particular
`ASTSelectIntersectExceptQuery` derives from `ASTSelectQuery` but keeps its
operand selects in `children` (not in the `Expression` slots), so it is
matched first; it re-exposes those operands under `selects` and returns
`true`. Adding a new subclass of an already-handled class without ordering
it first will silently route it through the base branch.

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
- **QueryParameter**: `name`, `param_type` (the `{name:type}` substitution;
  appears in value position and in identifier/table position). Note these
  survive into the AST only inside a parameterized view — otherwise the
  parameter is substituted (or errors) before `EXPLAIN` runs.
- **Literal**: `value_type` (the `Field` type), `value`. `Null` → JSON null,
  `Bool` → JSON bool, `Float64` → JSON number, `String` → JSON string;
  `UInt64` / `Int64` → JSON **string** (see contract); `Array` / `Tuple` →
  JSON array of typed `{value_type, value}` elements, `Map` / `Object` → JSON
  object of typed values; everything else → string via `FieldVisitorToString`
  (see "Container and fallback value serialization").
- **Asterisk**: `expression`, `transformers` (an array; plain `*` is bare).
- **QualifiedAsterisk**: `qualifier`, `transformers` (array).
- **ColumnsRegexpMatcher**: `pattern`, `expression`, `transformers` (array).
- **ColumnsListMatcher**: `expression`, `columns` (inlined), `transformers`
  (array).
- **ColumnsApplyTransformer**: `func_name`, `parameters`, `lambda`,
  `lambda_arg`, `column_name_prefix`.
- **ColumnsExceptTransformer**: `is_strict`, `columns` (inlined) for the
  explicit column-list form, or `pattern` for the regexp form
  (`EXCEPT 'a.*'`).
- **ColumnsReplaceTransformer**: `is_strict`, `replacements` (inlined).
- **ColumnsReplaceTransformer::Replacement**: `name`, `expression`.

`transformers` is a JSON array of the transformer nodes (the
`ASTColumnsTransformerList` wrapper is inlined, like every other list slot).

### Clauses and queries

- **SelectQuery**: scalar flags `distinct`, `group_by_all`,
  `group_by_with_totals` / `_rollup` / `_cube` / `_grouping_sets`,
  `order_by_all`, `recursive_with`, `limit_with_ties` (`LIMIT ... WITH TIES`);
  then one slot per clause —
  `with`, `select`, `from`, `prewhere`, `where`, `group_by`, `having`,
  `window`, `qualify`, `order_by`, `offset`, `limit`, `settings`,
  `interpolate`, and `limit_by`. List-shaped clauses are inlined; the rest
  are nodes. `LIMIT ... BY ...` is grouped into one object slot
  `limit_by: { length, offset?, by }` (`by` inlined). `ALIASES` /
  `CTE_ALIASES` are analyzer state and never present on parsed ASTs, so
  they are omitted.
- **SelectWithUnionQuery**: `union_mode` (non-default only), `selects`
  (inlines `list_of_selects`), plus the shared output clause (see below).
- **SelectIntersectExceptQuery**: `operator`, `selects` (the operand selects,
  mirroring `SelectWithUnionQuery`).
- **Subquery**: `cte_name`, `query` (its single child).
- **WithElement**: `name`, `subquery`, `aliases`.
- **Output clause** (shared by all `ASTQueryWithOutput` subclasses — e.g.
  `SelectWithUnionQuery` and the table-scoped DDL/DML): the trailing
  `[INTO OUTFILE <file> [APPEND | TRUNCATE] [AND STDOUT]
  [COMPRESSION <c> [LEVEL <n>]]] [FORMAT <name>] [SETTINGS ...]` suffix.
  Serialized as `out_file` (the filename literal node; present only for
  `INTO OUTFILE`), the `outfile_append` / `outfile_truncate` /
  `outfile_with_stdout` boolean flags (each only when set), `compression` and
  `compression_level` (literal nodes), `format` (a plain string, mirroring the
  InsertQuery `format` field), and `settings` (the output-level `SETTINGS`
  placed *after* `FORMAT`, e.g. `SELECT ... FORMAT TSV SETTINGS max_threads =
  1`). All slots are omitted when absent. Note the position of `SETTINGS`
  matters: a `SETTINGS` placed *before* `FORMAT` (`SELECT 1 SETTINGS x = 1
  FORMAT TSV`) is consumed into the operand `SelectQuery`'s own `settings`
  slot instead of the wrapper's output-clause `settings`.

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
  `order_by` (inlined), and — when the frame is non-default — `frame_type`
  plus `frame_begin` / `frame_end`, each a `{ type, offset?, preceding? }`
  object (`type` is the boundary type; `preceding` emitted only when true).
- **InterpolateElement**: `column`, `expr`.
- **Settings** (the `SETTINGS` clause / `ASTSetQuery`; `type` is `"Settings"`,
  not `"Set"` — see note below): `changes` (name → value object, values
  stringified like literals), `default_settings`.
- **SampleRatio**: `numerator`, `denominator` (exact rationals, stringified
  since they may exceed `UInt64`).

### DDL / DML

- **CreateQuery** (also `AttachQuery` by `type` when `attach`): the `attach` /
  `temporary` / `if_not_exists` / `is_*_view` / `is_dictionary` / `replace_*` /
  `create_or_replace` flags (when set), `attach_from_path` and
  `attach_as_replicated` (the `ATTACH TABLE t FROM '/path'` source and the
  `ATTACH TABLE t AS [NOT] REPLICATED` conversion marker), `uuid`, `cluster`
  (`ON CLUSTER`), `database`, `table`, `columns_list`, `aliases`, `storage`,
  `as_table_function`, `as_database` / `as_table`, `select`, `targets`,
  `comment`, `dictionary_attributes`, `dictionary`, `refresh` (the refreshable
  materialized-view `REFRESH ...` strategy).
- **RefreshStrategy** (`ASTRefreshStrategy`; the MV `REFRESH` clause, also the
  target of `ALTER ... MODIFY REFRESH`): `schedule_kind` (`AFTER` / `EVERY` /
  `UNKNOWN`), `append`, `period`, `offset`, `spread` (each a `TimeInterval`),
  `dependencies` (inlined), `settings`. `schedule_kind` / `append` are scalar
  members, not `children`.
- **TimeInterval** (`ASTTimeInterval`, e.g. `1 YEAR 3 DAY`): `interval`, an
  array of `{kind, value}` units (`kind` from `IntervalKind::toString()`). The
  whole value is held in a member with no AST children.
- **Columns** (`ASTColumns`): `columns`, `indices`, `constraints`,
  `projections` (inlined), `primary_key`, `primary_key_from_columns`.
- **ColumnDeclaration**: `name`, `data_type`, `default_specifier`,
  `default_expression`, `null_modifier`, `ephemeral_default`,
  `primary_key_specifier`, `comment`, `codec`, `statistics`, `ttl`,
  `collation`, `settings`.
- **DataType**: `name`, `arguments` (inlined, e.g. `Decimal(10, 2)`).
- **EnumDataType** (`Enum` / `Enum8` / `Enum16` with fully explicit values):
  `name`, `values` (array of `{ name, value }`; `value` is a JSON number — enum
  values fit in `Int16`). Auto-assigned enums (`Enum8('a', 'b')`) instead parse
  to a generic `DataType` carrying the elements in `arguments`.
- **TupleDataType** (`Tuple`): `name`, `arguments` (the element types, inlined),
  and `element_names` (array of strings) for a *named* tuple. Unnamed tuples omit
  `element_names`.
- **ObjectTypeArgument** (`ASTObjectTypeArgument`; its `type` is overridden
  from the raw `"ASTObjectTypeArgument"` getID): one argument of a JSON/Object
  type — exactly one of `path_with_type` (an `ObjectTypedPath`), `skip_path`
  (`SKIP x`), `skip_path_regexp` (`SKIP REGEXP '...'`), or `parameter`
  (a `setting = N` pair) is present.
- **ObjectTypedPath** (`ASTObjectTypedPathArgument`, a JSON/Object typed path
  `a.b.c Type`): `name` (the path) and `data_type` (not `type`, which is
  reserved for the node-class discriminator). The path is otherwise only
  echoed into `getID`, which `astTypeName` trims away.
- **Storage**: `engine`, `partition_by`, `primary_key`, `order_by`,
  `sample_by`, `ttl_table`, `settings`.
- **InsertQuery**: `database`, `table`, `table_function`, `columns`,
  `format`, `partition_by`, `settings`, `select`, `infile`, `compression`.
- **Index**: `name`, `expression`, `index_type`, `granularity`.
- **Constraint**: `name`, `constraint_type` (`CHECK` / `ASSUME`),
  `expression`.
- **Projection**: `name`, `query`, `index`, `index_type` (the `INDEX expr TYPE
  name` projection index type, an `ASTFunction`), `settings` (projection-level
  `SETTINGS`). `index_type` and `settings` live in dedicated members, not
  `children`.
- **ProjectionSelectQuery**: `with`, `select`, `group_by`, `order_by` (all
  inlined). Unlike a normal SELECT, a projection stores its `ORDER BY` as a
  *single* node — multiple keys packed into a `tuple(...)` and a lone key kept
  bare — so `order_by` is reconstructed as a flat list of keys (the tuple's
  arguments, or the single key), matching the SQL formatter. A projection's
  `GROUP BY` is a plain expression list (no GROUPING SETS / ROLLUP / CUBE in the
  projection grammar).
- **TTLElement**: `mode` (`DELETE` / `MOVE` / `GROUP_BY` / `RECOMPRESS`),
  `ttl`, and for `MOVE` the `destination_type` / `destination_name` /
  `if_exists`; for `GROUP_BY` the `group_by_key` and `group_by_assignments`
  (both arrays of nodes, kept in dedicated members the native AST otherwise
  drops); `where`, `recompression_codec`.
- **Collation** (`ASTCollation`, the `COLLATE` clause of a column or ORDER BY
  element): `name` (the collation identifier; falls back to the parsed node
  when it is not a plain identifier). The name lives in the `collation` member,
  not `children`.
- **Partition**: `all`, `value`, `id`.
- **DeleteQuery**: `database`, `table`, `cluster`, `partition`, `predicate`.
- **UpdateQuery**: `database`, `table`, `cluster`, `assignments`,
  `predicate`, `partition`.
- **DropQuery** (also `DetachQuery` / `TruncateQuery` by `type`): `kind`,
  `database`, `table`, `cluster`, `if_exists` / `if_empty` / `is_dictionary`
  / `is_view` / `sync` / `permanently`, `database_and_tables`. For
  `TRUNCATE ALL TABLES FROM db`: `has_all` / `has_tables`, plus the optional
  table-name pattern `like` with `not_like` / `case_insensitive_like` flags.
- **OptimizeQuery**: `database`, `table`, `cluster`, `partition`, `final`,
  `deduplicate`, `deduplicate_by_columns`, `cleanup`.
- **Assignment**: `column`, `expression`.
- **AlterQuery**: `alter_object` (`TABLE` / `DATABASE`), `database`, `table`,
  `cluster`, `commands` (inlined).
- **AlterCommand**: `command_type` (the full `ALTER` verb, e.g. `ADD_COLUMN`,
  `MOVE_PARTITION`, `MODIFY_TTL`), the relevant flags, and whichever sub-node
  slots apply — `column_declaration`, `column`, `order_by`, `index_declaration`,
  `partition`, `predicate`, `assignments`, `comment`, `ttl`, `settings_changes`,
  `select`, `rename_to`, `snapshot_desc`, ... plus the `from*` / `to*` /
  `move_destination_name` / `with_name` / `snapshot_name` strings,
  `move_destination_type` (`DISK` / `VOLUME` / `TABLE` / `SHARD`, on
  `MOVE_PARTITION`), and `replace` (on `REPLACE_PARTITION`, distinguishing
  `REPLACE` from `ATTACH ... FROM`).
- **CreateFunctionQuery**: `or_replace`, `if_not_exists`, `cluster`,
  `function_name`, `function_core`.
- **DropFunctionQuery**: `function_name`, `if_exists`, `cluster`.
- **CreateNamedCollectionQuery**: `collection_name`, `if_not_exists`,
  `cluster`, `changes` (the `key = value` body as a name -> value object), and
  `overridability` (a name -> bool object, present only for keys carrying an
  explicit `OVERRIDABLE` / `NOT OVERRIDABLE` flag). The node has no `children`.
  Unlike the `Settings` node's `changes` (untyped strings), each value here is a
  typed `{value_type, value}` pair, exactly like a `Literal`. A named collection
  carries no schema, so the tag is required to keep the value forms from
  colliding: `b = 5` (`UInt64`) vs `b = '5'` (`String`) both stringify to `"5"`;
  likewise `a = -5` (`Int64`) vs `'-5'`, `a = disk(...)` (a function, stored as a
  `CustomType`) vs the same text as a `String`, and `a = 1.0` (`Float64`, value
  `1`) vs the integer `1`.
- **CreateWorkloadQuery**: `or_replace`, `if_not_exists`, `cluster`,
  `workload_name`, `workload_parent` (the `IN parent` clause), and `changes`
  (array of `{name, value, resource?}` — each SETTINGS entry with its optional
  `FOR resource`). `value` is a typed `{value_type, value}` pair, as for a named
  collection above (same collision reasoning).
- **CreateResourceQuery**: `or_replace`, `if_not_exists`, `cluster`,
  `resource_name`, `unit` (`IOByte` / `CPUNanosecond` / `QuerySlot`), and
  `operations` (array of `{mode, disk?}`; `mode` is `READ` / `WRITE` /
  `MASTER_THREAD` / `WORKER_THREAD` / `QUERY`, and an absent `disk` means ANY
  DISK).
- **DropNamedCollectionQuery** / **DropWorkloadQuery** /
  **DropResourceQuery**: `collection_name` / `workload_name` / `resource_name`
  respectively, plus `if_exists` and `cluster`. All are plain string members
  (no `children`), so without the explicit branches these nodes would expose
  nothing but their `type`.
- **BackupQuery** (also `RestoreQuery` by `type`): `kind`, `cluster`,
  `elements`, `backup_name` (the `TO` / `FROM` destination), `base_backup_name`
  (incremental base), `settings`, `cluster_host_ids` (internal, normally
  absent). Each `elements` entry has `element_type`, `database` / `table`,
  `new_database` / `new_table` (only when an `AS` clause renamed the object),
  `partitions`, `except_tables` (array of `{database, table}`),
  `except_databases`. Note the parser folds the `DICTIONARY` / `VIEW` keywords
  into `element_type` `TABLE`.
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
all trim at the first space to the same `"Dictionary"` as the container.
`ASTSetQuery` is likewise overridden to `"Settings"` (its `getID` is `"Set"`,
confusable with `SET` statements and the `Set` data structure).

### Other statements and elements

- **Explain**: `kind`, `query`, `settings`, `table_function`, `table_override`,
  `output_settings`. `settings` is the EXPLAIN-level settings clause
  (`EXPLAIN SETTINGS <k>=<v> ...`, `ASTExplainQuery::ast_settings`), while
  `output_settings` is the trailing output-clause settings
  (`EXPLAIN ... FORMAT <fmt> SETTINGS <k>=<v>`, the `ASTQueryWithOutput` base
  `settings_ast`). Both may be present at once and are kept on separate keys.
- **DescribeQuery**: `table_expression`.
- **ShowTables**: covers SHOW TABLES / DATABASES / CLUSTERS / CLUSTER /
  DICTIONARIES / SETTINGS / MERGES / FILESYSTEM CACHES on one class. The
  variant-selector flags `databases`, `clusters`, `cluster` (singular, SHOW
  CLUSTER `<name>`), `dictionaries`, `show_settings` (the internal
  `m_settings` member — renamed so the `m_` prefix does not leak and it does
  not collide with the `settings` slot), `changed` (SHOW CHANGED SETTINGS),
  `merges`, `caches`, `temporary`, `full`; `cluster_name` (the SHOW CLUSTER
  operand, the internal `cluster_str` member — `cluster` elsewhere is the ON
  CLUSTER string, so that key is not reused); `from`, `like`, `not_like`,
  `case_insensitive_like` (LIKE vs ILIKE), `where`, `limit`.
- **ShowColumns**: `extended` / `full` flags, `table` (always present — the
  grammar requires `FROM <table>`), `database`, `like` / `not_like` /
  `case_insensitive_like`, `where`, `limit`. The native AST keeps all of these
  in plain members (its `children` is empty), so they are surfaced explicitly
  here; otherwise `SHOW COLUMNS`, `SHOW FIELDS`, `SHOW EXTENDED FULL COLUMNS`,
  ... would collapse to a bare `{"type": "ShowColumns"}` and lose the table.
- **ShowSetting**: `setting_name` (the sole operand; lives in a private member,
  so without this it would collapse to a bare `{"type": "ShowSetting"}`).
- **ShowFunctions**: `like` / `case_insensitive_like` (the sole operand; a plain
  member, so otherwise `SHOW FUNCTIONS LIKE '...'` loses its filter).
- **ShowIndexes** (`ASTShowIndexesQuery`, `SHOW INDEX/INDEXES/INDICES/KEYS`):
  `extended`, `table` (mandatory), `database`, `where`. Its `getID` is a mis-set
  `"ShowColumns"` (shared verbatim with `ASTShowColumnsQuery`), so `astTypeName`
  overrides the `type` to `"ShowIndexes"` to keep the two distinct statements
  distinguishable.
- **CreateIndexQuery** / **DropIndexQuery**: table target, `if_not_exists` /
  `unique` / `if_exists`, `index_name`, `index_declaration`.
- **CheckQuery**: table target, `partition`, `part_name`.
- **UseQuery**: `database`.
- **KillQueryQuery**: `kill_type`, `sync`, `test`, `cluster`, `where`. The
  `SYNC` / `ASYNC` / `TEST` mode is the `sync` and `test` bool pair — `ASYNC` is
  neither set.
- **TransactionControl** (`ASTTransactionControl`; `type` is overridden from the
  raw `"ASTTransactionControl"` getID): `action` (`BEGIN` / `COMMIT` /
  `ROLLBACK` / `SET_SNAPSHOT`) and, for `SET_SNAPSHOT`, `snapshot` (stringified
  `UInt64`). Both collapse onto one class otherwise.
- **Rename**: `exchange` / `database` / `dictionary` flags, `cluster`,
  `elements` (array of `{from_database, from_table, to_database, to_table,
  if_exists}`).
- **SYSTEM** (`ASTSystemQuery`): `system_type`, then the operand fields —
  each only meaningful for a subset of the ~130 sub-commands, surfaced
  whenever populated: `database`, `table`, `if_exists`, `cluster`, `replica`,
  `shard`, `replica_zk_path` and `is_drop_whole_replica` (DROP REPLICA),
  `with_tables`, `target_model`, `target_function`, `storage_policy` /
  `volume` / `disk` (STOP MOVES / MERGES scopes and cache drops), `seconds`
  (SUSPEND FOR; stringified `UInt64`), `sync_replica_mode` (`STRICT` /
  `LIGHTWEIGHT` / `PULL`; `DEFAULT` omitted) with `src_replicas`
  (LIGHTWEIGHT FROM list), `tables` (FLUSH LOGS / FLUSH ASYNC INSERT QUEUE
  targets, an array of `{database?, table}`), `settings`,
  `schema_cache_storage` / `schema_cache_format` (DROP SCHEMA CACHE),
  `filesystem_cache_name` / `key_to_drop` / `offset_to_drop` (stringified;
  filesystem-cache drops), `distributed_cache_drop_connections` /
  `distributed_cache_server_id`, `query_result_cache_tag`, `backup_name` /
  `backup_source` (UNFREEZE / RESTORE-related), `fail_point_name` /
  `fail_point_action` (`PAUSE` / `RESUME`), `fake_time_for_view`
  (TEST VIEW SET FAKE TIME; stringified — absent means UNSET), and for
  START/STOP LISTEN the `server_type` object `{type, custom_name?,
  exclude_types?, exclude_custom_names?}` (values from
  `ServerType::serverTypeToString`). The `SYSTEM INSTRUMENT` operands are
  compiled only under `USE_XRAY` and are not serialized.
- **Stat** (`ASTStatisticsDeclaration`): `columns`, `types`.
- **StorageOrderByElement**: `expression`, `direction`.
- **NameTypePair**: `name`, `data_type`.
- **QualifiedColumnsRegexpMatcher** / **QualifiedColumnsListMatcher**:
  `pattern` / `columns`, `qualifier`, `transformers`.

A generic fallback over `ASTQueryWithTableAndOutput` gives `database` /
`table` / `temporary` / `uuid` (when set, e.g. `UNDROP TABLE t UUID '...'`) to
every other simple table-scoped statement that carries only a target —
`EXISTS *`, `SHOW CREATE *`, etc. `UndropQuery` additionally exposes `cluster`
(`ON CLUSTER`).

### Access management

The access-control statements store their content in typed member fields and
plain value objects rather than the positional `children` list (`GrantQuery`
has no children at all; `CreateUserQuery` keeps only `AuthenticationData`
markers), so each is enriched with named slots. Scalar flags are emitted only
when set.

- **GrantQuery** / **RevokeQuery** (one class `ASTGrantQuery`, split by
  `is_revoke`): `attach_mode`, `admin_option`, `current_grants`,
  `replace_access`, `replace_granted_roles`, `cluster`; then either
  `access_rights` (privilege grant) **or** `roles` (role grant, a
  `RolesOrUsersSet`), and `grantees` (`RolesOrUsersSet`). `access_rights` is an
  array of privilege elements — `{access_types, database?, default_database?,
  table?, columns?, parameter?, wildcard?, grant_option?, is_partial_revoke?}`;
  `access_types` is the decoded keyword list (`["SELECT", "UPDATE"]`).
- **CheckGrantQuery**: `access_rights` (same element shape).
- **CreateUserQuery** (also ALTER USER via `alter`): `alter`, `attach`,
  `if_exists`, `if_not_exists`, `or_replace`, `cluster`, `names`
  (`UserNamesWithHost`), `new_name`, `storage_name`, `authentication_methods`
  (array of `AuthenticationData`), `reset_authentication_methods_to_new`,
  `add_identified_with`, `replace_authentication_methods`, `hosts` / `add_hosts`
  / `remove_hosts` (the HOST clause, see below), `default_roles`
  (`RolesOrUsersSet`), `default_database` (`DatabaseOrNone`), `settings` /
  `alter_settings`, `grantees` (`RolesOrUsersSet`), `global_valid_until`.
- **CreateRoleQuery** (also ALTER ROLE): `alter`, `attach`, the `if_*` flags,
  `cluster`, `names` (string array), `new_name`, `storage_name`, `settings` /
  `alter_settings`.
- **CreateQuotaQuery** (also ALTER QUOTA): `alter`, `attach`, the `if_*` flags,
  `cluster`, `names`, `new_name`, `key_type`, `storage_name`, `limits`, `roles`
  (`RolesOrUsersSet`). Each `limits` entry is `{duration_sec,
  randomize_interval?, drop?, max?}` where `max` maps quota-type name →
  stringified value (e.g. `{ "QUERIES": "100", "RESULT_ROWS": "1000" }`; the
  keys are the uppercase `toString(QuotaType)` set below).
- **SetRoleQuery** (`SET ROLE` / `SET DEFAULT ROLE`): `kind`, `roles`
  (`RolesOrUsersSet`), `to_users` (`RolesOrUsersSet`, for `SET DEFAULT ROLE`).
- **CreateRowPolicyQuery** (also ALTER ROW POLICY): `alter`, `attach`, the
  `if_*` flags, `cluster`, `storage_name`, `names` (`RowPolicyNames`),
  `new_short_name`, `is_restrictive` (bool), `filters`, `roles`. Each `filters`
  entry is `{filter_type, condition?}` — `condition` is omitted when the filter
  was set to `NONE`.
- **CreateSettingsProfileQuery** (also ALTER SETTINGS PROFILE): like
  CreateRole, plus `to_roles` (`RolesOrUsersSet`).
- **CreateMaskingPolicyQuery** (also ALTER MASKING POLICY): the create/alter
  flags, `cluster`, `storage_name`, `name`, `database`, `table`, `new_name`,
  `update_assignments` (inlined `Assignment` list), `where_condition`, `roles`,
  `priority` (stringified, only when non-zero).
- **DropAccessEntityQuery**: `entity_type`, `if_exists`, `cluster`,
  `storage_name`, `names`, `row_policy_names` (`RowPolicyNames`, for policies).
- **MoveAccessEntityQuery**: `entity_type`, `cluster`, `storage_name`, `names`,
  `row_policy_names`.
- **ExecuteAsQuery**: `target_user` (`UserNameWithHost`), `subquery`.
- **ShowGrantsQuery**: `for_roles` (`RolesOrUsersSet`), `with_implicit`,
  `final`.
- **ShowCreateAccessEntityQuery**: `entity_type`, `names`, `row_policy_names`,
  `current_quota`, `current_user`, `all`, `short_name`, `database`, `table`.
- **ShowAccessEntitiesQuery**: `entity_type`, `all`, `current_quota`,
  `current_roles`, `enabled_roles`, `short_name`, `database`, `table`.

Shared helper nodes:

- **RolesOrUsersSet**: `all`, `use_keyword_any`, `names`, `current_user`,
  `except_names`, `except_current_user`, `id_mode` (when true the `names` hold
  UUIDs rather than names).
- **UserNamesWithHost**: `users` (array of `UserNameWithHost`).
- **UserNameWithHost**: `name`, `host_pattern` (only when present).
- **AuthenticationData**: `auth_type`, `contains_password`, `contains_hash`,
  `ssl_cert_subject_type`, `valid_until`, and `arguments` — the password / hash
  / salt / server / realm literal nodes, emitted verbatim (the JSON reflects
  the parsed literals regardless of secret-masking display settings).
- **SettingsProfileElements**: `elements` (array of `SettingsProfileElement`).
- **SettingsProfileElement**: `parent_profile`, `setting_name`, `value`,
  `min_value`, `max_value`, `disallowed_values`, `writability`, `id_mode`.
  Values are stringified like literals.
- **AlterSettingsProfileElements**: `add_settings`, `modify_settings`,
  `drop_settings` (each a `SettingsProfileElements`), `drop_all_settings`,
  `drop_all_profiles`.
- **RowPolicyNames**: `policies` (array of `{short_name, database?, table?}`).
- **RowPolicyName**: `short_name`, `database`, `table`.
- **DatabaseOrNone**: `none: true` **or** `database`.
- **PublicSSHKey**: `key_type`, `key_base64`.

The privilege list (`access_rights`) and the HOST clause (`hosts` /
`add_hosts` / `remove_hosts`) are plain value objects, not AST nodes, so they
are serialized inline. The HOST object is `{any_host?, local_host?, names?,
name_regexps?, like_patterns?, addresses?, subnets?}` (addresses and subnets in
canonical text form).

### Not yet enriched

`ParallelWithQuery` keeps `children` deliberately — it is a homogeneous list
of parallel sub-queries. Any class not special-cased in `enrichNode` likewise
falls back to the positional `children` array. The JSON stays valid; only
those subtrees are positional.

## Container and fallback value serialization

`Literal.value` is native JSON for the scalar `Field` types (with the 64-bit
integer caveat). The container `Field` types recurse, preserving each
element's own type:

- **Array** / **Tuple**: a JSON array of typed elements, each a
  `{ "value_type": <type>, "value": <value> }` pair mirroring how an
  `ASTLiteral` node is serialized — so `[1, 2]` becomes
  `[{"value_type": "UInt64", "value": "1"}, {"value_type": "UInt64",
  "value": "2"}]`.
- **Map** / **Object**: a JSON object keyed by the (stringified) key with
  typed values, e.g. `{"tbl": {"value_type": "String", "value": "x = 1"}}`.
  JSON keys are always strings; a non-`String` map key falls back to its
  `FieldVisitorToString` form (an integer key becomes its digits). A `Map`
  whose elements are not two-element (key, value) tuples — not expected for a
  real `Map` field — falls back to the generic array-of-typed-elements form.

Every remaining `value_type` is stringified by `FieldVisitorToString`
(`src/Common/FieldVisitorToString.cpp`, the authority for the exact syntax).
Consumers re-parsing those strings should pin against that. The shape:

- **String** (and `String`-typed scalars): the content is delivered
  already-unescaped in the JSON string.
- **Decimal32/64/128/256**: the decimal text, e.g. `Decimal(10, 2)` casts
  serialize the type as a plain string.
- **UUID / IPv4 / IPv6**: their canonical text form, e.g.
  `00000000-0000-0000-0000-000000000000`.
- **Int128 / UInt128 / Int256 / UInt256**: decimal digits as a string.
- **Bool** is native JSON (`true` / `false`); `Null` is JSON `null`.

Note integer literals too large for `UInt256` are parsed as `Float64` and
emitted as JSON numbers (so they can lose precision — that is a property of
the parser, not the serializer).

## Enum value sets

These slots carry a fixed set of strings from internal `toString` helpers.
Pinned here so upstream renames are reviewable. Values omitted when the
node's default applies are marked *(default, omitted)*.

- **Function.kind**: `WINDOW_FUNCTION`, `LAMBDA_FUNCTION`, `TABLE_ENGINE`,
  `DATABASE_ENGINE`, `BACKUP_NAME`, `CODEC`, `STATISTICS`
  (`ORDINARY_FUNCTION` *(default, omitted)*).
- **Function.nulls_action**: `RESPECT NULLS`, `IGNORE NULLS`
  (`EMPTY` *(default, omitted)*).
- **TableJoin.kind**: `INNER`, `LEFT`, `RIGHT`, `FULL`, `CROSS`, `COMMA`,
  `PASTE`.
- **TableJoin.strictness**: `RIGHT_ANY`, `ANY`, `ALL`, `ASOF`, `SEMI`,
  `ANTI` (`UNSPECIFIED` *(default, omitted)*).
- **TableJoin.locality**: `LOCAL`, `GLOBAL` (`UNSPECIFIED` *(default,
  omitted)*).
- **ArrayJoin.kind**: `INNER`, `LEFT`.
- **SelectWithUnionQuery.union_mode** (only when non-default): `UNION_ALL`,
  `UNION_DISTINCT`, `EXCEPT_ALL`, `EXCEPT_DISTINCT`, `INTERSECT_ALL`,
  `INTERSECT_DISTINCT` (the `*_DEFAULT` variants are not emitted).
- **SelectIntersectExceptQuery.operator**: `INTERSECT ALL`,
  `INTERSECT DISTINCT`, `EXCEPT ALL`, `EXCEPT DISTINCT`.
- **OrderByElement.direction** / **StorageOrderByElement.direction**: `ASC`,
  `DESC`.
- **ColumnDeclaration.default_specifier**: `DEFAULT`, `MATERIALIZED`,
  `ALIAS`, `EPHEMERAL`, `AUTO_INCREMENT` (Empty *(default, omitted)*).
  `null_modifier` is a JSON bool, not a string.
- **WindowDefinition.frame_type**: `ROWS`, `GROUPS`, `RANGE`.
- **frame_begin.type / frame_end.type**: `Unbounded`, `Current`, `Offset`.
- **Constraint.constraint_type**: `CHECK`, `ASSUME`.
- **DropQuery.kind**: `DROP`, `DETACH`, `TRUNCATE` (also reflected in the
  node's `type`: `DropQuery` / `DetachQuery` / `TruncateQuery`).
- **BackupQuery.kind**: `BACKUP`, `RESTORE` (also reflected in the node's
  `type`: `BackupQuery` / `RestoreQuery`).
- **BackupQuery.elements[].element_type**: `TABLE`, `TEMPORARY_TABLE`,
  `DATABASE`, `ALL`.
- **KillQueryQuery.kill_type**: `QUERY`, `MUTATION`, `PART_MOVE_TO_SHARD`,
  `TRANSACTION`.
- **TTLElement.mode**: `DELETE`, `MOVE`, `GROUP_BY`, `RECOMPRESS`.
- **TTLElement.destination_type** (with `MOVE`): `DISK`, `VOLUME`, `TABLE`,
  `DELETE`, `SHARD`.
- **AlterQuery.alter_object**: `TABLE`, `DATABASE` (`UNKNOWN` *(default,
  omitted)*).
- **AlterCommand.command_type**: `ADD_COLUMN`, `DROP_COLUMN`,
  `MODIFY_COLUMN`, `COMMENT_COLUMN`, `RENAME_COLUMN`, `MATERIALIZE_COLUMN`,
  `MODIFY_ORDER_BY`, `MODIFY_SAMPLE_BY`, `MODIFY_TTL`, `REWRITE_PARTS`,
  `MATERIALIZE_TTL`, `MODIFY_SETTING`, `RESET_SETTING`, `MODIFY_QUERY`,
  `MODIFY_REFRESH`, `REMOVE_TTL`, `REMOVE_SAMPLE_BY`, `ADD_INDEX`,
  `DROP_INDEX`, `MATERIALIZE_INDEX`, `ADD_CONSTRAINT`, `DROP_CONSTRAINT`,
  `ADD_PROJECTION`, `DROP_PROJECTION`, `MATERIALIZE_PROJECTION`,
  `ADD_STATISTICS`, `DROP_STATISTICS`, `MODIFY_STATISTICS`,
  `MATERIALIZE_STATISTICS`, `DROP_PARTITION`, `DROP_DETACHED_PARTITION`,
  `FORGET_PARTITION`, `ATTACH_PARTITION`, `MOVE_PARTITION`,
  `REPLACE_PARTITION`, `FETCH_PARTITION`, `FREEZE_PARTITION`, `FREEZE_ALL`,
  `UNFREEZE_PARTITION`, `UNFREEZE_ALL`, `DELETE`, `UPDATE`,
  `APPLY_DELETED_MASK`, `APPLY_PATCHES`, `NO_TYPE`, `MODIFY_DATABASE_SETTING`,
  `MODIFY_DATABASE_COMMENT`, `MODIFY_COMMENT`, `MODIFY_SQL_SECURITY`,
  `UNLOCK_SNAPSHOT`.
- **TransactionControl.action**: `BEGIN`, `COMMIT`, `ROLLBACK`, `SET_SNAPSHOT`.
- **RefreshStrategy.schedule_kind**: `AFTER`, `EVERY`, `UNKNOWN`.
- **SetRoleQuery.kind**: `SET_ROLE`, `SET_ROLE_DEFAULT`, `SET_DEFAULT_ROLE`.
- **CreateQuotaQuery.key_type** (`toString(QuotaKeyType)`): `NONE`,
  `USER_NAME`, `IP_ADDRESS`, `FORWARDED_IP_ADDRESS`, `CLIENT_KEY`,
  `CLIENT_KEY_OR_USER_NAME`, `CLIENT_KEY_OR_IP_ADDRESS`.
- **CreateQuotaQuery.limits[].max** keys (`toString(QuotaType)`): `QUERIES`,
  `QUERY_SELECTS`, `QUERY_INSERTS`, `ERRORS`, `RESULT_ROWS`, `RESULT_BYTES`,
  `READ_ROWS`, `READ_BYTES`, `EXECUTION_TIME`, `WRITTEN_BYTES`,
  `FAILED_SEQUENTIAL_AUTHENTICATIONS`.
- **CreateRowPolicyQuery.filters[].filter_type**
  (`toString(RowPolicyFilterType)`): `SELECT_FILTER`.
- **SettingsProfileElement.writability**: `WRITABLE`, `CONST`,
  `CHANGEABLE_IN_READONLY`.
- **AuthenticationData.auth_type** (`toString(AuthenticationType)`, the SQL
  keyword): `NO_PASSWORD`, `PLAINTEXT_PASSWORD`, `SHA256_PASSWORD`,
  `DOUBLE_SHA1_PASSWORD`, `LDAP`, `KERBEROS`, `SSL_CERTIFICATE`,
  `BCRYPT_PASSWORD`, `SSH_KEY`, `HTTP`, `JWT`, `SCRAM_SHA256_PASSWORD`,
  `NO_AUTHENTICATION`.
- **entity_type** on the Drop/Move/ShowCreate/Show access statements
  (`toString(AccessEntityType)`, uppercased with spaces): `USER`, `ROLE`,
  `SETTINGS PROFILE`, `ROW POLICY`, `QUOTA`, `MASKING POLICY`.

`Explain.kind` (`ASTExplainQuery::toString`) and `SYSTEM.system_type`
(`ASTSystemQuery::typeToString`) draw from large sets — see those helpers.
The `03930_explain_ast_json_type_ids` test snapshots the complete set of
node `type` ids so a new descriptive `getID` cannot silently collide.

## Known divergences for source-fidelity consumers

The JSON reflects the parsed `IAST`, not the source text. Consumers needing
source fidelity should know (no code change planned):

- **Operator spelling is normalized**: `!=` and `<>` both become
  `notEquals`, `||` → `concat`, `a BETWEEN x AND y` → `and(greaterOrEquals,
  lessOrEquals)`, etc. The text-explain serializer's operator map is the
  inverse.
- **No comments, source locations, or parenthesization** are preserved by
  the parser, so they are absent from the JSON.
- **Empty vs absent `ExpressionList`** is indistinguishable after inlining —
  an empty inlined list and a missing clause both read as "no/empty array".
- **`LIMIT n, m` comma syntax is normalized** into separate `offset` /
  `limit` slots (there is no record that the comma form was used).
- **`INTERPOLATE` is attached at `SelectQuery` level** (the `interpolate`
  slot), not to the last `WITH FILL` `ORDER BY` element.

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
- `03925_explain_ast_json_ddl` — CREATE TABLE family + INSERT.
- `03926_explain_ast_json_ddl_elements` — index/constraint/projection/TTL/
  partition and DELETE/UPDATE/DROP/OPTIMIZE.
- `03927_explain_ast_json_alter_dict` — ALTER family and dictionaries.
- `03928_explain_ast_json_misc_queries` — EXPLAIN/DESCRIBE/SHOW/KILL/RENAME/
  SYSTEM and the table-target fallback.
- `03929_explain_ast_json_query_parameters` — the `{ version, ast }` wrapper
  and query parameters (via a parameterized view).
- `03930_explain_ast_json_type_ids` — snapshot of every emitted node `type`
  id (collision guard).
- `03931_explain_ast_json_datatype_elements` — the data-type element slots:
  `EnumDataType.values`, `TupleDataType.element_names` (named / unnamed /
  mixed), the `Nested` `NameTypePair` elements, the auto-enum fallback, and the
  `Dynamic(max_types = N)` argument.

The tests pipe through `jq` (`--format TSVRaw`) to keep references readable
and focused.
