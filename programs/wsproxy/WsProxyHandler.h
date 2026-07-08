#pragma once

#include <Server/HTTP/HTTPRequestHandler.h>

namespace DB
{

/// HTTP entry point for the proxy.
///
/// Step 2 scope: a plain HTTP request serves a short info page; a WebSocket
/// upgrade request completes the RFC 6455 handshake and then runs an echo loop
/// (control frames handled, data frames reflected back). The echo loop is the
/// placeholder for step 3, where each session will instead open a native-protocol
/// `Connection` to a backend server and bridge packets <-> frames.
class WsProxyHandler : public HTTPRequestHandler
{
public:
    void handleRequest(HTTPServerRequest & request, HTTPServerResponse & response, const ProfileEvents::Event & write_event) override;

private:
    void handleWebSocket(HTTPServerRequest & request, HTTPServerResponse & response);
    void serveInfo(HTTPServerRequest & request, HTTPServerResponse & response);
};

}
