set (CLICKHOUSE_WSPROXY_SOURCES
    WsProxy.cpp
    WsProxyHandler.cpp
    WebSocketFrames.cpp
    ProxySession.cpp
)

set (CLICKHOUSE_WSPROXY_LINK
    PRIVATE
        clickhouse_common_config
        clickhouse_common_io
        clickhouse_functions
        dbms
)

# A standalone binary, deliberately NOT wired into the multi-call `clickhouse`
# dispatch: there is no `mainEntryClickHouseWsProxy` in programs/main.cpp and no
# `clickhouse_program_install` line for it. Omitting both is what keeps it a
# separate executable with its own `main`, which in turn keeps the diff to
# shared dispatch code at zero. This mirrors the standalone-keeper pattern
# (`BUILD_STANDALONE_KEEPER`), except we do not also produce a multi-call lib.
#
# Note: linking `dbms` is unavoidable (it is where `Connection`, `FormatFactory`
# and the HTTP server infrastructure live), so this binary is roughly the size
# of the full `clickhouse` binary. The separate target buys an independently
# deployable artifact, not a smaller one.
clickhouse_add_executable (clickhouse-wsproxy ${CLICKHOUSE_WSPROXY_SOURCES})
target_link_libraries (clickhouse-wsproxy ${CLICKHOUSE_WSPROXY_LINK})
# So the program's own headers resolve as angled includes (matches the keeper
# standalone pattern).
target_include_directories (clickhouse-wsproxy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
