#pragma once

#include "ecu_dto.hpp"
#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/web/server/api/ApiController.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include OATPP_CODEGEN_BEGIN(ApiController)

class DiagnosticsController : public oatpp::web::server::api::ApiController {
public:
    explicit DiagnosticsController(
        const std::shared_ptr<oatpp::data::mapping::ObjectMapper>& objectMapper)
        : ApiController(objectMapper)
    {
        // Seed the in-memory collection with sample resources for this demo.
        m_ecus.emplace("engine", makeEcu("engine", "Engine Controller", "1.0.0"));
        m_ecus.emplace("battery", makeEcu("battery", "Battery Controller", "1.0.0"));
    }

    // Collection route: return all ECU resources currently in memory.
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

    // Item route: PATH extracts {id} from the URL so the handler can find one ECU.
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

    // Create route: oat++ parses the JSON request body into an EcuDto.
    ENDPOINT("POST", "/api/v1/ecus", createEcu, BODY_DTO(oatpp::Object<EcuDto>, requestEcu))
    {
        const std::string validationError = validateEcu(requestEcu);
        if (!validationError.empty()) {
            return createErrorResponse(Status::CODE_400, validationError);
        }

        bool alreadyExists = false;
        {
            // Check and insert under one lock to prevent duplicate IDs in concurrent requests.
            const std::lock_guard<std::mutex> lock(m_mutex);
            alreadyExists = m_ecus.find(*requestEcu->id) != m_ecus.end();
            if (!alreadyExists) {
                m_ecus.emplace(*requestEcu->id, requestEcu);
            }
        }
        if (alreadyExists) {
            return createErrorResponse(Status::CODE_409, "ECU ID already exists");
        }

        // 201 means a resource was created; Location identifies its new URL.
        auto response = createDtoResponse(Status::CODE_201, requestEcu);
        response->putHeader("Location", "/api/v1/ecus/" + *requestEcu->id);
        return response;
    }

    // Replace an existing ECU. The URL ID is canonical and must match the body ID.
    ENDPOINT("PUT", "/api/v1/ecus/{id}", replaceEcu, PATH(String, id),
             BODY_DTO(oatpp::Object<EcuDto>, requestEcu))
    {
        const std::string validationError = validateEcu(requestEcu);
        if (!validationError.empty()) {
            return createErrorResponse(Status::CODE_400, validationError);
        }
        if (*requestEcu->id != *id) {
            return createErrorResponse(Status::CODE_400, "body id must match path id");
        }

        bool foundEcu = false;
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            const auto found = m_ecus.find(*id);
            if (found != m_ecus.end()) {
                found->second = requestEcu;
                foundEcu = true;
            }
        }
        if (!foundEcu) {
            return createErrorResponse(Status::CODE_404, "ECU not found");
        }
        return createDtoResponse(Status::CODE_200, requestEcu);
    }

    // Remove an ECU. Successful deletion returns 204 with no response body.
    ENDPOINT("DELETE", "/api/v1/ecus/{id}", deleteEcu, PATH(String, id))
    {
        bool deleted = false;
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            deleted = m_ecus.erase(*id) != 0;
        }
        if (!deleted) {
            return createErrorResponse(Status::CODE_404, "ECU not found");
        }
        return createResponse(Status::CODE_204);
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

    // Validate fields shared by both POST and PUT requests.
    static std::string validateEcu(const oatpp::Object<EcuDto>& ecu)
    {
        if (!ecu || !ecu->id || ecu->id->empty() || !ecu->name || ecu->name->empty() ||
            !ecu->status || ecu->status->empty()) {
            return "id, name, and status must be non-empty strings";
        }
        if (!hasValidIdCharacters(*ecu->id)) {
            return "id may contain only letters, digits, '-' and '_'";
        }
        if (*ecu->status != "online" && *ecu->status != "offline" &&
            *ecu->status != "unknown") {
            return "status must be online, offline, or unknown";
        }
        return {};
    }

    // Keep IDs URL-friendly by allowing only letters, digits, hyphens, and underscores.
    static bool hasValidIdCharacters(const std::string& id)
    {
        return std::all_of(id.begin(), id.end(), [](unsigned char character) {
            return std::isalnum(character) != 0 || character == '-' || character == '_';
        });
    }

    std::shared_ptr<OutgoingResponse> createErrorResponse(Status status,
                                                          const std::string& message)
    {
        auto error = ErrorDto::createShared();
        error->error = message;
        return createDtoResponse(status, error);
    }

    // Demo storage: data is shared by endpoints but lost when the server stops.
    std::map<std::string, oatpp::Object<EcuDto>> m_ecus;
    std::mutex m_mutex;
};

#include OATPP_CODEGEN_END(ApiController)