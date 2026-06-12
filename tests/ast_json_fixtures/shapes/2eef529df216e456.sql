CREATE DICTIONARY null_ip_dict (
    network String,
    val     UInt8 DEFAULT 77
)
PRIMARY KEY network
SOURCE(NULL())
LAYOUT(IP_TRIE())
LIFETIME(0)
