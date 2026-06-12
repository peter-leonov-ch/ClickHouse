alter table  mt update n = n + (n not in m) in partition id '1' where 1 settings mutations_sync=1
