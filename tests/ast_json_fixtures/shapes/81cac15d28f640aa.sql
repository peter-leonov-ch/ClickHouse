SELECT *, toTypeName(*) FROM (SELECT * FROM system.numbers LIMIT 100) FORMAT PrettySpaceNoEscapesMonoBlock
