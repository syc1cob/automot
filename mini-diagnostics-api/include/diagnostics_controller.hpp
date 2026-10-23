#pragma once

#include "ecu_repository.hpp"
#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/web/server/api/ApiController.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <iostream>
#include <memory>
#include <string>

#include OATPP_CODEGEN_BEGIN(ApiController)

class DiagnosticsController : public oatpp::web::server::api::ApiController {
public:
    DiagnosticsController(const std::shared_ptr<oatpp::data::mapping::ObjectMapper>& objectMapper,
                          const std::shared_ptr<EcuRepository>& repository)
        : ApiController(objectMapper), m_repository(repository)
    {
    }

    // Collection route: return all ECU resources stored in SQLite.
    ENDPOINT("GET", "/api/v1/ecus", listEcus)
    {
        auto ecus = oatpp::Vector<oatpp::Object<EcuDto>>::createShared();
        try {
            for (const auto& ecu : m_repository->list()) {
                ecus->push_back(ecu);
            }
        }
        catch (const std::exception& error) {
            return createDatabaseErrorResponse(error);
        }
        return createDtoResponse(Status::CODE_200, ecus);
    }

    // Item route: PATH extracts {id} from the URL so the handler can find one ECU.
    ENDPOINT("GET", "/api/v1/ecus/{id}", getEcu, PATH(String, id))
    {
        oatpp::Object<EcuDto> ecu;
        try {
            ecu = m_repository->findById(*id);
        }
        catch (const std::exception& error) {
            return createDatabaseErrorResponse(error);
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

        bool created = false;
        try {
            created = m_repository->create(requestEcu);
        }
        catch (const std::exception& error) {
            return createDatabaseErrorResponse(error);
        }
        if (!created) {
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

        bool replaced = false;
        try {
            replaced = m_repository->replace(requestEcu);
        }
        catch (const std::exception& error) {
            return createDatabaseErrorResponse(error);
        }
        if (!replaced) {
            return createErrorResponse(Status::CODE_404, "ECU not found");
        }
        return createDtoResponse(Status::CODE_200, requestEcu);
    }

    // Remove an ECU. Successful deletion returns 204 with no response body.
    ENDPOINT("DELETE", "/api/v1/ecus/{id}", deleteEcu, PATH(String, id))
    {
        bool deleted = false;
        try {
            deleted = m_repository->remove(*id);
        }
        catch (const std::exception& error) {
            return createDatabaseErrorResponse(error);
        }
        if (!deleted) {
            return createErrorResponse(Status::CODE_404, "ECU not found");
        }
        return createResponse(Status::CODE_204, "");
    }

private:
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
        if (*ecu->status != "online" && *ecu->status != "offline" && *ecu->status != "unknown") {
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

    std::shared_ptr<OutgoingResponse> createErrorResponse(Status status, const std::string& message)
    {
        auto error = ErrorDto::createShared();
        error->error = message;
        return createDtoResponse(status, error);
    }

    std::shared_ptr<OutgoingResponse> createDatabaseErrorResponse(const std::exception& error)
    {
        std::cerr << "Database operation failed: " << error.what() << std::endl;
        return createErrorResponse(Status::CODE_500, "Database operation failed");
    }

    std::shared_ptr<EcuRepository> m_repository;
};

#include OATPP_CODEGEN_END(ApiController)