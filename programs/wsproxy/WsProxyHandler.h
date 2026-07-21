#pragma once

#include <Interpreters/Context_fwd.h>
#include <Server/HTTP/HTTPRequestHandler.h>
#include <base/types.h>

namespace DB
{

/// Where the edge proxy forwards native-protocol queries (a remote backend).
struct BackendParams
{
    String host = "localhost";
    UInt16 port = 9000;
    String user = "default";
    String password;
    String database;
    bool secure = false; /// Connect to the backend over TLS (native secure protocol).
    /// Native-protocol compression codec for backend->proxy result blocks: "lz4" (fast, default),
    /// "zstd" (higher ratio — fewer bytes on a bandwidth-limited WAN, at more backend CPU), or
    /// "none". The proxy decompresses whatever the server sends regardless (codec is self-describing).
    String compression_method = "lz4";
};

/// HTTP entry point for the proxy.
///
/// A plain HTTP request serves a short info page. A WebSocket upgrade completes
/// the RFC 6455 handshake after validating any browser `Origin`, then hands the
/// socket to a `WebSocketSession`, which bridges the WebSocket to a
/// native-protocol `Connection` against the backend.
/// The desired output format is taken from the `format` query parameter of the
/// WebSocket URL (default `JSONEachRow`).
class WsProxyHandler : public HTTPRequestHandler
{
public:
    WsProxyHandler(ContextPtr context_, BackendParams backend_);

    void handleRequest(HTTPServerRequest & request, HTTPServerResponse & response, const ProfileEvents::Event & write_event) override;

private:
    void handleWebSocket(HTTPServerRequest & request, HTTPServerResponse & response);
    void serveInfo(HTTPServerRequest & request, HTTPServerResponse & response);

    ContextPtr context;
    BackendParams backend;
};

}
