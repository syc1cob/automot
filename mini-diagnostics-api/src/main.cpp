#include "diagnostics_controller.hpp"
#include "oatpp/core/base/Environment.hpp"
#include "oatpp/network/Server.hpp"
#include "oatpp/network/tcp/server/ConnectionProvider.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/web/server/HttpConnectionHandler.hpp"
#include "oatpp/web/server/HttpRouter.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>

namespace {

// Connect the JSON mapper, route table, HTTP handler, and local TCP listener.
void runServer(v_uint16 port, const std::string& databasePath)
{
    auto repository = std::make_shared<EcuRepository>(databasePath);
    auto objectMapper = oatpp::parser::json::mapping::ObjectMapper::createShared();
    auto router = oatpp::web::server::HttpRouter::createShared();
    auto controller = std::make_shared<DiagnosticsController>(objectMapper, repository);
    router->addController(controller);

    auto connectionHandler = oatpp::web::server::HttpConnectionHandler::createShared(router);
    auto connectionProvider = oatpp::network::tcp::server::ConnectionProvider::createShared(
        {"127.0.0.1", port, oatpp::network::Address::IP_4});

    oatpp::network::Server server(connectionProvider, connectionHandler);
    std::cout << "Mini Diagnostics API: http://127.0.0.1:" << port << "/api/v1/ecus\n"
              << "SQLite database: " << databasePath << "\n"
              << "Press Ctrl+C to stop." << std::endl;
    server.run();
}

} // namespace

int main(int argc, char* argv[])
{
    v_uint16 port = 8080;
    if (argc > 3) {
        std::cerr << "Usage: mini-diagnostics-api [port] [database-file]" << std::endl;
        return 1;
    }
    // Allow an alternate port and database file for local development and testing.
    if (argc >= 2) {
        char* end = nullptr;
        const unsigned long parsedPort = std::strtoul(argv[1], &end, 10);
        if (end == argv[1] || *end != '\0' || parsedPort == 0 || parsedPort > 65535) {
            std::cerr << "Port must be an integer between 1 and 65535" << std::endl;
            return 1;
        }
        port = static_cast<v_uint16>(parsedPort);
    }
    const std::string databasePath = argc == 3 ? argv[2] : "ecus.db";

    // Initialize oat++ before creating framework objects; destroy it after the server exits.
    oatpp::base::Environment::init();
    int result = 0;
    try {
        runServer(port, databasePath);
    }
    catch (const std::exception& error) {
        std::cerr << "Server failed: " << error.what() << std::endl;
        result = 1;
    }
    oatpp::base::Environment::destroy();
    return result;
}