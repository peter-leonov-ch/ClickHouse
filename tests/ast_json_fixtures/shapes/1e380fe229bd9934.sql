WITH 'UTC' as timezone SELECT timezone, timeZoneOf(now64(3, timezone)) == timezone
