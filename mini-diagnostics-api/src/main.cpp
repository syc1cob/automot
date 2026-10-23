#include "oatpp/core/base/Environment.hpp"
#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/network/Server.hpp"
#include "oatpp/network/tcp/server/ConnectionProvider.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/web/server/HttpConnectionHandler.hpp"
#include "oatpp/web/server/HttpRouter.hpp"
#include "oatpp/web/server/api/ApiController.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include OATPP_CODEGEN_BEGIN(DTO)

class EcuDto : public oatpp::DTO {
    DTO_INIT(EcuDto, DTO)

    DTO_FIELD(String, id);
    DTO_FIELD(String, name);
    DTO_FIELD(String, status);
    DTO_FIELD(String, firmwareVersion);
};

class ErrorDto : public oatpp::DTO {
    DTO_INIT(ErrorDto, DTO)

    DTO_FIELD(String, error);
};

#include OATPP_CODEGEN_END(DTO)

#include OATPP_CODEGEN_BEGIN(ApiController)

class DiagnosticsController : public oatpp::web::server::api::ApiController {
public:
    explicit DiagnosticsController(
        const std::shared_ptr<oatpp::data::mapping::ObjectMapper>& objectMapper)
        : ApiController(objectMapper)
    {
        m_ecus.emplace("engine", makeEcu("engine", "Engine Controller", "1.0.0"));
        m_ecus.emplace("battery", makeEcu("battery", "Battery Controller", "1.0.0"));
    }

    ENDPOINT("GET", "/api/v1/ecus", listEcus)
    {
        auto ecus = oatpp::Vector<oatpp::Object<EcuDto>>::createShared();
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            for (const auto& entry : m_ecus) {
                ecus->push_back(entry.second);
            }
        }
        return createDtoResponse(Status::CODE_200, ecus);
    }

    ENDPOINT("GET", "/api/v1/ecus/{id}", getEcu, PATH(String, id))
    {
        oatpp::Object<EcuDto> ecu;
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            const auto found = m_ecus.find(*id);
            if (found != m_ecus.end()) {
                ecu = found->second;
            }
        }
        if (!ecu) {
            return createErrorResponse(Status::CODE_404, "ECU not found");
        }
        return createDtoResponse(Status::CODE_200, ecu);
    }

    ENDPOINT("POST", "/api/v1/ecus", createEcu, BODY_DTO(oatpp::Object<EcuDto>, requestEcu))
    {
        if (!requestEcu || !requestEcu->id || requestEcu->id->empty() || !requestEcu->name ||
            requestEcu->name->empty() || !requestEcu->status || requestEcu->status->empty()) {
            return createErrorResponse(Status::CODE_400,
                                       "id, name, and status must be non-empty strings");
        }
        if (!hasValidIdCharacters(*requestEcu->id)) {
            return createErrorResponse(Status::CODE_400,
                                       "id may contain only letters, digits, '-' and '_'");
        }
        if (*requestEcu->status != "online" && *requestEcu->status != "offline" &&
            *requestEcu->status != "unknown") {
            return createErrorResponse(Status::CODE_400,
                                       "status must be online, offline, or unknown");
        }

        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            if (m_ecus.find(*requestEcu->id) != m_ecus.end()) {
                return createErrorResponse(Status::CODE_409, "ECU ID already exists");
            }
            m_ecus.emplace(*requestEcu->id, requestEcu);
        }

        auto response = createDtoResponse(Status::CODE_201, requestEcu);
        response->putHeader("Location", "/api/v1/ecus/" + *requestEcu->id);
        return response;
    }

private:
    static oatpp::Object<EcuDto> makeEcu(const std::string& id, const std::string& name,
                                         const std::string& firmwareVersion)
    {
        auto ecu = EcuDto::createShared();
        ecu->id = id;
        ecu->name = name;
        ecu->status = "online";
        ecu->firmwareVersion = firmwareVersion;
        return ecu;
    }

    static bool hasValidIdCharacters(const std::string& id)
    {
        return std::all_of(id.begin(), id.end(), [](unsigned char character) {
            return std::isalnum(character) != 0 || character == '-' || character == '_';
        });
    }

    std::shared_ptr<OutgoingResponse> createErrorResponse(Status status,
                                                          const oatpp::String& message)
    {
        auto error = ErrorDto::createShared();
        error->error = message;
        return createDtoResponse(status, error);
    }

    std::map<std::string, oatpp::Object<EcuDto>> m_ecus;
    std::mutex m_mutex;
};

#include OATPP_CODEGEN_END(ApiController)

void runServer(v_uint16 port)
{
    auto objectMapper = oatpp::parser::json::mapping::ObjectMapper::createShared();
    auto router = oatpp::web::server::HttpRouter::createShared();
    auto controller = std::make_shared<DiagnosticsController>(objectMapper);
    router->addController(controller);

    auto connectionHandler = oatpp::web::server::HttpConnectionHandler::createShared(router);
    auto connectionProvider = oatpp::network::tcp::server::ConnectionProvider::createShared(
        {"127.0.0.1", port, oatpp::network::Address::IP_4});

    oatpp::network::Server server(connectionProvider, connectionHandler);
    std::cout << "Mini Diagnostics API: http://127.0.0.1:" << port << "/api/v1/ecus\n"
              << "Press Ctrl+C to stop." << std::endl;
    server.run();
}

int main(int argc, char* argv[])
{
    v_uint16 port = 8080;
    if (argc > 2) {
        std::cerr << "Usage: mini-diagnostics-api [port]" << std::endl;
        return 1;
    }
    if (argc == 2) {
        char* end = nullptr;
        const unsigned long parsedPort = std::strtoul(argv[1], &end, 10);
        if (end == argv[1] || *end != '\0' || parsedPort == 0 || parsedPort > 65535) {
            std::cerr << "Port must be an integer between 1 and 65535" << std::endl;
            return 1;
        }
        port = static_cast<v_uint16>(parsedPort);
    }

    oatpp::base::Environment::init();
    int result = 0;
    try {
        runServer(port);
    }
    catch (const std::exception& error) {
        std::cerr << "Server failed: " << error.what() << std::endl;
        result = 1;
    }
    oatpp::base::Environment::destroy();
    return result;
}