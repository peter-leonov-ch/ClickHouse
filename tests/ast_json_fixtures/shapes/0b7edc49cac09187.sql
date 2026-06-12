WITH
  (SELECT count()
     FROM t
    WHERE accurateCastOrNull(d,'IPv4') IS NOT NULL
      AND toIPv4(accurateCastOrNull(d,'IPv4')) NOT IN (toIPv4('0.0.0.0'), toIPv4('192.168.0.1'))
  ) AS bad_v4,
  (SELECT count()
     FROM t
    WHERE accurateCastOrNull(d,'IPv6') IS NOT NULL
      AND toIPv6(accurateCastOrNull(d,'IPv6')) NOT IN (toIPv6('::'), toIPv6('::1'), toIPv6('::ffff:192.168.0.1'))
  ) AS bad_v6,
  bad_v4 + bad_v6 AS bad_cnt
SELECT
  'ch_dbg_summary' AS tag,
  (SELECT count() FROM t)                                                AS total,
  (SELECT count() FROM t WHERE accurateCastOrNull(d,'IPv4') IS NOT NULL) AS typed_v4,
  (SELECT count() FROM t WHERE accurateCastOrNull(d,'IPv6') IS NOT NULL) AS typed_v6,
  bad_v4, bad_v6,
  version() AS ver,
  getSetting('session_timezone') AS tz
WHERE bad_cnt > 0
