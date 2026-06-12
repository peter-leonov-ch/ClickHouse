CREATE TABLE t_merge AS t
ENGINE = Merge('02111_modify_table_comment', 't')
COMMENT 'this is a Merge table'
