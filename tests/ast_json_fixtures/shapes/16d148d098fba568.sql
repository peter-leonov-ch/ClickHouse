SELECT *, toTypeName(*) FROM (SELECT * FROM system.numbers LIMIT 100) FORMAT Pretty SETTINGS output_format_pretty_display_footer_column_names=0
