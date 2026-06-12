#!/usr/bin/env python3
"""Harvest SQL statements from tests/queries/0_stateless/*.sql and capture their
JSON AST via `EXPLAIN AST json = 1`, producing SQL -> JSON golden pairs.

This reuses the existing stateless test corpus as parser-conformance inputs.
Statements that the reference parser rejects (or that use query-parameter
placeholders like `{x:UInt8}`, which require values) are skipped and counted.

Usage:
    CLICKHOUSE_BINARY=/path/to/clickhouse \
        ./harvest_stateless.py --out harvested --limit 0 --jobs 8

    --limit N   stop after N statements (0 = all); useful for a quick sample
    --jobs N    parallel EXPLAIN AST workers
    --out DIR   output dir (pairs go to DIR/<hash>.sql / .json)
"""
import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

# Leaf "value" fields dropped from the structural signature: two queries that
# differ only in identifier names, literal values, aliases, settings contents,
# patterns, etc. collapse to the same AST shape. Enum/flag scalars (direction,
# kind, strictness, union_mode, frame_type, is_operator, ...) and literal
# `value_type` are kept, so each parser branch stays represented.
SHAPE_DROP_KEYS = {
    "name", "name_parts", "alias", "database", "value", "cte_name",
    "parent_window_name", "window_name", "column", "func_name", "lambda_arg",
    "column_name_prefix", "pattern", "numerator", "denominator", "changes",
    "default_settings",
}


def ast_shape(node, depth=0):
    """A canonical structural signature of a JSON AST: node types + slot keys +
    enum/flag scalars, recursing into nested nodes. Consecutive identical
    children collapse, so list arity does not multiply distinct shapes.

    `depth` (0 = unlimited) caps recursion: nodes below it collapse to their
    type, giving a coarser signature and fewer distinct shapes."""
    if isinstance(node, dict):
        if depth == 1:
            return node.get("type", "?") + "(...)"
        parts = []
        for k in sorted(node):
            if k == "type" or k in SHAPE_DROP_KEYS:
                continue
            v = node[k]
            if isinstance(v, (dict, list)):
                parts.append(k + "=" + ast_shape(v, depth - 1 if depth else 0))
            else:
                parts.append(k + "=" + str(v))
        return node.get("type", "?") + "(" + ",".join(parts) + ")"
    if isinstance(node, list):
        out, prev = [], None
        for e in node:
            s = ast_shape(e, depth)
            if s != prev:  # collapse runs of identical child shapes
                out.append(s)
                prev = s
        return "[" + ",".join(out) + "]"
    return str(node)

HERE = os.path.dirname(os.path.abspath(__file__))
STATELESS = os.path.normpath(os.path.join(HERE, "..", "queries", "0_stateless"))

PARAM_RE = re.compile(r"\{[A-Za-z_]\w*\s*:")  # query-parameter placeholder


def split_statements(text):
    """Split a multi-statement SQL script on top-level semicolons, respecting
    string literals (', "), quoted identifiers (`), and -- / /* */ comments."""
    out, buf, i, n = [], [], 0, len(text)
    while i < n:
        c = text[i]
        two = text[i:i + 2]
        if two == "--":
            j = text.find("\n", i)
            i = n if j < 0 else j
            continue
        if two == "/*":
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
            continue
        if c in "'\"`":
            buf.append(c)
            i += 1
            while i < n:
                buf.append(text[i])
                if text[i] == c:
                    # doubled quote is an escape inside the same literal
                    if i + 1 < n and text[i + 1] == c:
                        buf.append(text[i + 1])
                        i += 2
                        continue
                    i += 1
                    break
                if text[i] == "\\" and c == "'" and i + 1 < n:
                    buf.append(text[i + 1])
                    i += 2
                    continue
                i += 1
            continue
        if c == ";":
            stmt = "".join(buf).strip()
            if stmt:
                out.append(stmt)
            buf = []
            i += 1
            continue
        buf.append(c)
        i += 1
    tail = "".join(buf).strip()
    if tail:
        out.append(tail)
    return out


def collect_statements(limit):
    seen, stmts = set(), []
    for fname in sorted(os.listdir(STATELESS)):
        if not fname.endswith(".sql"):
            continue
        try:
            text = open(os.path.join(STATELESS, fname), encoding="utf-8", errors="replace").read()
        except OSError:
            continue
        for stmt in split_statements(text):
            if PARAM_RE.search(stmt):
                continue
            key = " ".join(stmt.split())
            if key in seen:
                continue
            seen.add(key)
            stmts.append(stmt)
            if limit and len(stmts) >= limit:
                return stmts
    return stmts


def explain(binary, stmt):
    try:
        r = subprocess.run(
            [binary, "local", "--format", "TSVRaw", "-q", "EXPLAIN AST json = 1 " + stmt],
            capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=30,
        )
    except subprocess.TimeoutExpired:
        return None
    if r.returncode != 0 or not r.stdout.strip():
        return None
    return r.stdout


MARKER = "@@CHFIX@@"
MARKER_RE = re.compile(r"^" + re.escape(MARKER) + r" (\d+) ([BE])$")


def explain_batch(binary, stmts):
    """EXPLAIN AST a whole batch in one process. EXPLAIN only parses (never
    executes), and --ignore-error skips rejects, so the batch is safe and
    self-resynchronising. Marker SELECTs delimit each result.

    Returns a list aligned with `stmts`; an entry is the JSON string or None."""
    script = []
    for i, s in enumerate(stmts):
        script.append(f"SELECT '{MARKER} {i} B';")
        script.append("EXPLAIN AST json = 1 " + s + ";")
        script.append(f"SELECT '{MARKER} {i} E';")
    try:
        r = subprocess.run(
            [binary, "local", "--format", "TSVRaw", "--multiquery", "--ignore-error"],
            input="\n".join(script), capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=600,
        )
    except subprocess.TimeoutExpired:
        return [None] * len(stmts)

    results = [None] * len(stmts)
    cur, buf = None, []
    for line in r.stdout.splitlines():
        m = MARKER_RE.match(line)
        if m:
            idx, kind = int(m.group(1)), m.group(2)
            if kind == "B":
                cur, buf = idx, []
            elif kind == "E" and cur == idx:
                text = "\n".join(buf).strip()
                if text:
                    results[idx] = text + "\n"
                cur, buf = None, []
            continue
        if cur is not None:
            buf.append(line)
    return results


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(HERE, "harvested"))
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--jobs", type=int, default=8, help="parallel batches")
    ap.add_argument("--batch", type=int, default=500, help="statements per process (1 = per-statement)")
    ap.add_argument("--binary", default=os.environ.get("CLICKHOUSE_BINARY", "clickhouse"))
    ap.add_argument("--dedupe", choices=["text", "shape"], default="text",
                    help="text = unique statement text (default); "
                         "shape = one representative per distinct AST shape")
    ap.add_argument("--shape-depth", type=int, default=0,
                    help="shape dedup: cap signature depth (0 = unlimited); "
                         "smaller = coarser shapes, fewer representatives")
    ap.add_argument("--write", action="store_true", help="write pairs (otherwise dry-run stats only)")
    args = ap.parse_args()

    stmts = collect_statements(args.limit)
    print(f"collected {len(stmts)} unique, param-free statements")

    if args.write:
        os.makedirs(args.out, exist_ok=True)

    ok = fail = 0

    def write_pair(stmt, js):
        h = hashlib.sha1(stmt.encode()).hexdigest()[:16]
        with open(os.path.join(args.out, h + ".sql"), "w") as f:
            f.write(stmt + "\n")
        with open(os.path.join(args.out, h + ".json"), "w") as f:
            f.write(js)

    def run_batch(batch):
        results = ([explain(args.binary, batch[0])] if args.batch == 1
                   else explain_batch(args.binary, batch))
        pairs = [(s, js) for s, js in zip(batch, results) if js is not None]
        return len(pairs), len(batch) - len(pairs), pairs

    # shape dedup keeps one representative per AST shape: the shortest
    # statement (ties broken lexicographically) so the choice is deterministic
    # regardless of batch completion order.
    reps = {}
    written = 0

    batches = [stmts[i:i + max(1, args.batch)] for i in range(0, len(stmts), max(1, args.batch))]
    with ThreadPoolExecutor(max_workers=args.jobs) as ex:
        for n_ok, n_fail, pairs in ex.map(run_batch, batches):
            ok += n_ok
            fail += n_fail
            for stmt, js in pairs:
                if args.dedupe == "shape":
                    try:
                        doc = json.loads(js)
                    except json.JSONDecodeError:
                        continue
                    # Shape the AST itself, not the { version, ast } wrapper, so
                    # depth granularity is measured from the real root.
                    root = doc["ast"] if isinstance(doc, dict) and "ast" in doc else doc
                    sig = ast_shape(root, args.shape_depth)
                    cand = (len(stmt), stmt)
                    if sig not in reps or cand < reps[sig][0]:
                        reps[sig] = (cand, js)
                    continue
                written += 1
                if args.write:
                    write_pair(stmt, js)

    if args.dedupe == "shape":
        for (_, stmt), js in reps.values():
            written += 1
            if args.write:
                write_pair(stmt, js)

    total = ok + fail
    rate = (100.0 * ok / total) if total else 0.0
    print(f"parsed OK: {ok}  failed: {fail}  yield: {rate:.1f}%")
    if args.dedupe == "shape":
        print(f"distinct AST shapes: {len(reps)}")
    print(f"{'wrote' if args.write else 'would write'} {written} pairs"
          + (f" to {args.out}" if args.write else ""))


if __name__ == "__main__":
    sys.exit(main())
