WITH
  (SELECT count()
     FROM t
    WHERE accurateCastOrNull(d,'IPv4') IS NOT NULL
      AND toIPv4(accurateCastOrNull(d,'IPv4')) NOT IN (toIPv4('0.0.0.0'), toIPv4('192.168.0.1'))
  ) +
  (SELECT count()
     FROM t
    WHERE accurateCastOrNull(d,'IPv6') IS NOT NULL
      AND toIPv6(accurateCastOrNull(d,'IPv6')) NOT IN (toIPv6('::'), toIPv6('::1'), toIPv6('::ffff:192.168.0.1'))
  ) AS bad_cnt
SELECT
  'ch_dbg_offenders' AS tag,
  id,
  toTypeName(d)                        AS src_type,
  toIPv4(accurateCastOrNull(d,'IPv4')) AS v4,
  toIPv6(accurateCastOrNull(d,'IPv6')) AS v6,
  toString(d)                          AS raw_d
FROM t
WHERE bad_cnt > 0
  AND (
        (accurateCastOrNull(d,'IPv4') IS NOT NULL
         AND toIPv4(accurateCastOrNull(d,'IPv4')) NOT IN (toIPv4('0.0.0.0'), toIPv4('192.168.0.1')))
     OR (accurateCastOrNull(d,'IPv6') IS NOT NULL
         AND toIPv6(accurateCastOrNull(d,'IPv6')) NOT IN (toIPv6('::'), toIPv6('::1'), toIPv6('::ffff:192.168.0.1')))
      )
ORDER BY id
LIMIT 20
