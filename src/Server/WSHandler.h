#pragma once

#include <Server/HTTP/HTTPRequestHandler.h>

namespace DB
{

class IServer;

/// In-server WebSocket query endpoint (the `ws_port`).
///
/// A plain HTTP request serves a short info page. A WebSocket upgrade completes
/// the RFC 6455 handshake after validating any browser `Origin`, authenticates
/// via the server's own session auth (Basic / X-ClickHouse headers /
/// ?user=&password= / default), and then bridges
/// the socket to an in-process `LocalConnection` using the shared
/// `WebSocketSession`. The wire protocol is identical to `clickhouse-wsproxy`:
/// query as a text frame, results as binary frames in the `?format=` output
/// format, JSON control frames, and the `?logs`/`?flow`/`?parse`/`?parallel` knobs.
class WSHandler : public HTTPRequestHandler
{
public:
    explicit WSHandler(IServer & server_);

    void handleRequest(HTTPServerRequest & request, HTTPServerResponse & response, const ProfileEvents::Event & write_event) override;

private:
    void handleWebSocket(HTTPServerRequest & request, HTTPServerResponse & response);
    void serveInfo(HTTPServerRequest & request, HTTPServerResponse & response);

    IServer & server;
};

}
