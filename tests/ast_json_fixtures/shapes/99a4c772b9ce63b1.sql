UPDATE lightweight_test
SET value = 'UPDATED-1'
WHERE key IN (SELECT key FROM keys)
