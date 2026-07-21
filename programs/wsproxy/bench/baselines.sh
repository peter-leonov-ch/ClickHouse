#!/usr/bin/env bash
# Baseline fetch timings for a JSONCompactEachRow result, matched to the proxy's
# output (unquoted 64-bit ints) so byte counts are identical across paths.
set -uo pipefail
ROWS="${1:-1000000}"
ITERS="${2:-5}"
CH_HOST="$CLICKHOUSE_CLOUD_HOST"; CH_PW="$CLICKHOUSE_CLOUD_PASSWORD"
Q="SELECT number AS n, number*2 AS d, toString(number) AS s FROM numbers(${ROWS})"
S="SETTINGS output_format_json_quote_64bit_integers=0"
FQ="$Q $S FORMAT JSONCompactEachRow"

median() { printf '%s\n' "$@" | sort -n | awk '{a[NR]=$1} END{print a[int((NR+1)/2)]}'; }

# Run "$@" with up to 3 retries on transient failure; echoes stdout byte count.
retry_bytes() { local n=0; while :; do b=$("$@" | wc -c) && [ "$b" -gt 100 ] && { echo "$b"; return 0; }; n=$((n+1)); [ "$n" -ge 3 ] && { echo 0; return 1; }; sleep 2; done; }
nat() { clickhouse-client --host "$CH_HOST" --secure --user default --password "$CH_PW" --query "$FQ"; }
httpp() { curl -sS "https://${CH_HOST}:8443/" -H "X-ClickHouse-User: default" -H "X-ClickHouse-Key: ${CH_PW}" --data-binary "$FQ"; }
httpz() { curl -sS --compressed "https://${CH_HOST}:8443/?enable_http_compression=1" -H "X-ClickHouse-User: default" -H "X-ClickHouse-Key: ${CH_PW}" --data-binary "$FQ"; }

echo "### rows=$ROWS iters=$ITERS"

clickhouse-client --host "$CH_HOST" --secure --user default --password "$CH_PW" --query "SELECT 1" >/dev/null 2>&1 || true

run_path() { # $1=label $2=fn
  local label="$1" fn="$2" bytes=0 t=() s e b
  for _ in $(seq "$ITERS"); do
    s=$(date +%s.%N); b=$(retry_bytes "$fn"); e=$(date +%s.%N)
    t+=("$(echo "$e - $s" | bc)"); bytes=$b
  done
  printf '%-40s median=%ss bytes=%s\n' "$label" "$(median "${t[@]}")" "$bytes"
}

run_path "NATIVE  (native lz4 -> client format)" nat
run_path "HTTP    (server format, plain JSON)" httpp
run_path "HTTP-GZ (server format, gzip WAN)" httpz
