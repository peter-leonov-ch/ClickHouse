# EXPLAIN AST `json=1`: named slots instead of positional `children`

Follow-up to the initial JSON dump (see `formatASTAsJSON` in
`src/Parsers/DumpASTNode.cpp`). Several AST classes used to pack their
named sub-nodes into a positional `children` array, so consumers had to
know the in-class convention to tell `arguments` from `parameters`, or
`WHERE` from `HAVING`. This work replaces those positional slots with
named JSON fields per class, while keeping `children` for truly
homogeneous lists (`ExpressionList`, `TablesInSelectQuery`, ...).

**Status: done.** Landed as three self-contained commits — `ASTFunction`,
then `ASTOrderByElement`, then `ASTSelectQuery` — each with its own
stateless test (`03917` / `03918` / `03919`).

## Mechanism

`enrichNode` returns `bool handled_children`. When it returns `true`,
`formatASTAsJSON` skips the generic `children` walk for that node — every
interesting sub-node is already accessible by name, so emitting `children`
would duplicate them and reintroduce positionality. The default is
`return false`, so untouched classes keep their `children` array.

Two helpers in the anonymous namespace of `DumpASTNode.cpp` carry the work:

- `inlineExpressionList(list)` — emits an `ASTExpressionList`'s children as
  a JSON array, inlining the wrapper node (a null list yields `[]`). The
  wrapper is a parser-internal detail and was the source of half the
  positional confusion.
- `addNodeSlot(node, key, child)` — emits a single sub-node under `key`,
  but only when it is present.

Consumer contract after the change: `type` tells you the shape. If `type`
is `"Function"` you read `arguments` / `parameters` / `window_definition`.
If it's `"ExpressionList"` you read `children`.

---

## ASTFunction

Named slots, replacing the `[arguments, parameters, window_definition]`
positional `children`:

- `arguments` — **always** an array (possibly empty), even for ASTs
  without parsed arguments, so consumers never branch on presence. The
  inner `ASTExpressionList` is inlined.
- `parameters` — present only for parametric aggregates
  (`quantile(0.9)(x)`); inlined the same way.
- `window_definition` — the node as-is (it is **not** an
  `ASTExpressionList`); present only for window functions defined inline.
- `window_name` — present only for window functions referencing a named
  window.

Lambdas follow the same shape: their two arguments (parameter-tuple +
body) come through `arguments`. Operators (`a + b` → `name: "plus"`,
`is_operator: true`) are unchanged in spirit — just `arguments` of length 2.

```json
{
  "type": "Function",
  "name": "quantile",
  "arguments":  [ ... ],
  "parameters": [ ... ],
  "window_definition": { ... },
  "window_name": "w1"
}
```

---

## ASTOrderByElement

The `Child` enum is private and there is no `getExpression` accessor, so
the slots are emitted via the public getters (and `children.front()` for
the mandatory expression) rather than an enum table:

- `expression` — the mandatory sort key (first child).
- `collation` — `getCollation`.
- `fill_from` / `fill_to` / `fill_step` / `fill_staleness` — the
  `WITH FILL` bounds, via the matching getters.

`direction`, `nulls_first`, and `with_fill` remain as scalar fields.

---

## ASTSelectQuery

Driven by a `Slot` table over the public `Expression` enum, in declaration
order. Each present clause becomes a named slot:

| slot key | shape | | slot key | shape |
|---|---|---|---|---|
| `with` | list | | `order_by` | list |
| `select` | list | | `limit_by_offset` | node |
| `tables` | node | | `limit_by_length` | node |
| `prewhere` | node | | `limit_by` | list |
| `where` | node | | `limit_offset` | node |
| `group_by` | list | | `limit_length` | node |
| `having` | node | | `settings` | node |
| `window` | list | | `interpolate` | list |
| `qualify` | node | | | |

- **List-shaped** clauses inline their inner `ASTExpressionList`.
- **Single-expression** clauses are emitted as nodes.
- **`tables`** is a single `TablesInSelectQuery` object, which carries its
  own list of `TablesInSelectQueryElement`s.
- **Absent clauses are omitted** (no nulls).

`ALIASES` / `CTE_ALIASES` exist in the enum but are analyzer state — they
never appear on the parsed-but-not-analyzed ASTs returned by `EXPLAIN AST`,
so they are intentionally left out of the table. A future parser change
adding a new `Expression` value would silently drop out of the JSON until
the table is updated.

The existing boolean flags (`distinct`, `group_by_with_rollup`,
`order_by_all`, `recursive_with`, ...) remain as scalar fields.

---

## Notes / risks

- **Schema break.** Consumers written against the old `children` shape on
  these classes need to update. `EXPLAIN AST json=1` is new and not yet
  documented, so this was the right window.
- **Wrapper inlining.** Inlining `ExpressionList` children loses the
  ability to distinguish an empty `ExpressionList` from "no such clause",
  but that distinction is not observable from queries.
- **Tests** print the relevant subtree through `jq` (`--format TSVRaw` to
  get unescaped multi-line JSON from the explain result), keeping
  references readable and focused.
