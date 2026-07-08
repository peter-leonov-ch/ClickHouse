#pragma once

#include <ProxySession.h>

#include <Interpreters/Context_fwd.h>
#include <Server/HTTP/HTTPRequestHandler.h>

namespace DB
{

/// HTTP entry point for the proxy.
///
/// A plain HTTP request serves a short info page. A WebSocket upgrade completes
/// the RFC 6455 handshake and then hands the socket to a `ProxySession`, which
/// bridges the WebSocket to a native-protocol `Connection` against the backend.
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
