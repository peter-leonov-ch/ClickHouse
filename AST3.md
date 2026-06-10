# EXPLAIN AST `json=1`: named slots for the structural wrapper nodes

Follow-up to `AST2.md`. That work converted `ASTFunction`,
`ASTOrderByElement`, and `ASTSelectQuery` to named slots. What remained
were the *structural wrapper* nodes — the ones that produce the long
single-child `children` chains in the JSON (in the showcase dump, 34 of 37
`children` arrays held exactly one element). This plan gives those wrappers
named slots too, reusing the `handled_children` mechanism and the
`inlineExpressionList` / `addNodeSlot` helpers from `AST2.md`.

After this change, `children` survives only on the genuinely homogeneous
list nodes: `ExpressionList` and `TablesInSelectQuery`.

## Part 1 — table expressions and joins

- `ASTTableExpression`: `database_and_table_name`, `table_function`,
  `subquery`, `final` (bool), `sample_size`, `sample_offset`,
  `column_aliases` — exactly one of the first three is set.
- `ASTTableJoin`: `kind` (always — `INNER` / `LEFT` / `CROSS` / `COMMA` /
  ...), `strictness` and `locality` (only when not `UNSPECIFIED`), `using`
  (inlined list) and `on` (node).
- `ASTArrayJoin`: `kind` (`INNER` / `LEFT`), `expressions` (inlined list).
- `ASTTablesInSelectQueryElement`: `table_join`, `table_expression`,
  `array_join`.

`ASTTablesInSelectQuery` stays on `children` — it is a real list of
elements (one per JOIN).

## Part 2 — subquery, union query, WITH element

- `ASTSelectWithUnionQuery`: `union_mode` (already emitted), plus `selects`
  inlining `list_of_selects` — this drops the intermediate `ExpressionList`
  wrapper.
- `ASTSubquery`: `cte_name` (when set) and `query` (the single child).
  Combined with the union-query change this collapses the
  `Subquery → SelectWithUnionQuery → ExpressionList → SelectQuery` chain to
  `Subquery.query.selects[0]`.
- `ASTWithElement`: `name`, `subquery`, `aliases`.

## Part 3 — window definitions and interpolate

- `ASTWindowListElement`: `name`, `definition`.
- `ASTWindowDefinition`: `parent_window_name` (when set), `partition_by`
  (inlined list), `order_by` (inlined list), and — only when the frame is
  non-default — `frame_type`, `frame_begin_type`/`frame_begin_offset`/
  `frame_begin_preceding` and the `frame_end_*` counterparts.
- `ASTInterpolateElement`: `column`, `expr`.

This commit also refreshes the references of the earlier tests whose output
contains window definitions (`03917`) or window/order-by clauses (`03919`),
and the cumulative showcase (`03922`).

## Part 4 — leaf nodes that hid state

After the wrappers, the only nodes still rendering as a bare `{"type": ...}`
were leaf-ish ones holding non-AST state in private members rather than in
`children`:

- `ASTSetQuery` (the SETTINGS clause, `type: "Set"`): `changes` as a
  name → value object, plus `default_settings`.
- `ASTSampleRatio`: `numerator` / `denominator` (kept as exact rationals
  that can exceed `UInt64`, so emitted as strings).
- `ASTAsterisk` / `ASTQualifiedAsterisk`: `expression` / `qualifier` and
  `transformers`.
- COLUMNS matchers (`ASTColumnsRegexpMatcher` → `pattern`,
  `ASTColumnsListMatcher` → `columns`) and the transformers
  (`ASTColumnsApplyTransformer` → `func_name` / `parameters` / `lambda`,
  `ASTColumnsExceptTransformer` / `ASTColumnsReplaceTransformer` → `is_strict`,
  `Replacement` → `name`).

A plain `SELECT *` still serializes as `{"type": "Asterisk"}` — that node
genuinely has no attached state. The EXCEPT / REPLACE column lists stay in
`children` as homogeneous lists.

## Risks

- **Schema break**, same as `AST2.md`: consumers reading the old wrapper
  `children` must switch to the named slots. Still the right window since
  `EXPLAIN AST json=1` is new and undocumented.
- **Inlining loses the wrapper node identity** for `selects` /
  `partition_by` / `order_by` / `using` / array-join `expressions`, the
  same deliberate trade-off made in `AST2.md`.
