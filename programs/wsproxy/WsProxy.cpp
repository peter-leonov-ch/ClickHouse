#include <csignal>
#include <pthread.h>

#include <vector>

#include <Poco/Net/HTTPServerParams.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/ThreadPool.h>
#include <Poco/Timespan.h>
#include <Poco/Util/ServerApplication.h>

#include <AggregateFunctions/registerAggregateFunctions.h>
#include <Formats/registerFormats.h>
#include <Functions/registerFunctions.h>
#include <Interpreters/Context.h>

#include <Server/HTTP/HTTPContext.h>
#include <Server/HTTP/HTTPRequestHandler.h>
#include <Server/HTTP/HTTPRequestHandlerFactory.h>
#include <Server/HTTP/HTTPServer.h>

#include <Common/logger_useful.h>

#include <WsProxyHandler.h>
#include <ProxySession.h>

#include <base/types.h>

#include <cstdlib>
#include <string>


/// Standalone WebSocket proxy.
///
/// Deliberately a separate binary, kept out of the multi-call `clickhouse`
/// dispatch (no `mainEntryClickHouseWsProxy`, no `clickhouse_program_install`).
///
/// Step 2 scope: bring up the global `Context` and the format machinery, then
/// serve a real WebSocket endpoint (`WsProxyHandler`) that completes the RFC
/// 6455 handshake and echoes messages. Step 3 will replace the echo loop with a
/// native-protocol `Connection` to a backend server.
///
/// Signal handling: we block SIGINT/SIGTERM in the main thread *before* the
/// server spawns any worker threads, so the workers inherit the blocked mask.
/// A process-directed signal then stays pending until `waitForTerminationRequest`
/// consumes it via `sigwait`, giving a clean shutdown instead of the default
/// "terminate" disposition firing on a worker thread. (The heavier alternative
/// is to base this on `BaseDaemon`, which also provides config/logging/crash
/// handling; deferred until the proxy needs those.)

namespace DB
{

namespace
{

class WsProxyHandlerFactory : public HTTPRequestHandlerFactory
{
public:
    WsProxyHandlerFactory(ContextPtr context_, BackendParams backend_)
        : context(std::move(context_)), backend(std::move(backend_))
    {
    }

    std::unique_ptr<HTTPRequestHandler> createRequestHandler(const HTTPServerRequest &) override
    {
        return std::make_unique<WsProxyHandler>(context, backend);
    }

private:
    ContextPtr context;
    BackendParams backend;
};

/// Read a string environment variable, falling back to `def` when unset/empty.
std::string envOr(const char * name, const std::string & def)
{
    const char * value = std::getenv(name);
    return (value && *value) ? std::string(value) : def;
}

BackendParams backendParamsFromEnv()
{
    BackendParams params;
    params.host = envOr("WSPROXY_BACKEND_HOST", params.host);
    params.port = static_cast<UInt16>(std::stoul(envOr("WSPROXY_BACKEND_PORT", "9000")));
    params.user = envOr("WSPROXY_BACKEND_USER", params.user);
    params.password = envOr("WSPROXY_BACKEND_PASSWORD", params.password);
    params.database = envOr("WSPROXY_BACKEND_DATABASE", params.database);
    return params;
}

/// Conservative HTTP limits/timeouts for the skeleton; a later revision should
/// source these from configuration.
class WsProxyHTTPContext : public IHTTPContext
{
public:
    uint64_t getMaxHstsAge() const override { return 0; }
    uint64_t getMaxUriSize() const override { return 1024 * 1024; }
    uint64_t getMaxFields() const override { return 1'000'000; }
    uint64_t getMaxFieldNameSize() const override { return 128 * 1024; }
    uint64_t getMaxFieldValueSize() const override { return 128 * 1024; }
    uint64_t getMaxRequestHeaderSize() const override { return 8 * 1024 * 1024; }
    Poco::Timespan getHeadersReadTimeout() const override { return {30, 0}; }
    Poco::Timespan getReceiveTimeout() const override { return {30, 0}; }
    Poco::Timespan getSendTimeout() const override { return {30, 0}; }
};

}

class WsProxyServer : public Poco::Util::ServerApplication
{
protected:
    int main(const std::vector<std::string> &) override
    {
        LoggerPtr log = getLogger("WsProxy");

        /// Block termination signals before any worker threads are created, so
        /// they inherit the mask and `waitForTerminationRequest` can consume the
        /// signal cleanly. See the file header for the rationale.
        sigset_t sigset{};
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGINT);
        sigaddset(&sigset, SIGTERM);
        pthread_sigmask(SIG_BLOCK, &sigset, nullptr);

        /// Minimal global state the format machinery needs.
        shared_context = Context::createShared();
        global_context = Context::createGlobal(shared_context.get());
        global_context->makeGlobalContext();
        global_context->setApplicationType(Context::ApplicationType::SERVER);

        /// The same registrations clickhouse-client performs: aggregate functions
        /// are required to (de)serialize `AggregateFunction`-typed columns off the
        /// native wire, and functions back defaults/codecs. `registerFormats` alone
        /// is not enough for full type coverage.
        registerFunctions();
        registerAggregateFunctions();
        registerFormats();

        const BackendParams backend = backendParamsFromEnv();

        const UInt16 port = static_cast<UInt16>(std::stoul(envOr("WSPROXY_PORT", "9010")));
        Poco::Net::ServerSocket socket(port);
        Poco::ThreadPool server_pool(/* minCapacity= */ 1, /* maxCapacity= */ 16);
        Poco::Net::HTTPServerParams::Ptr params(new Poco::Net::HTTPServerParams);

        HTTPServer server(
            std::make_shared<WsProxyHTTPContext>(),
            std::make_shared<WsProxyHandlerFactory>(global_context, backend),
            server_pool,
            socket,
            params);

        server.start();
        LOG_INFO(log, "clickhouse-wsproxy listening on port {}; backend {}:{}", port, backend.host, backend.port);

        waitForTerminationRequest();

        LOG_INFO(log, "Shutting down");
        server.stop();
        return Application::EXIT_OK;
    }

private:
    SharedContextHolder shared_context;
    ContextMutablePtr global_context;
};

}

int main(int argc, char ** argv)
{
    DB::WsProxyServer app;
    return app.run(argc, argv);
}
