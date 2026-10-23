#pragma once

#include "oatpp/core/Types.hpp"
#include "oatpp/core/macro/codegen.hpp"

#include OATPP_CODEGEN_BEGIN(DTO)

// Defines the JSON shape shared by ECU responses and request bodies.
class EcuDto : public oatpp::DTO {
    DTO_INIT(EcuDto, DTO)

    DTO_FIELD(String, id);
    DTO_FIELD(String, name);
    DTO_FIELD(String, status);
    DTO_FIELD(String, firmwareVersion);
};

// Keeps API error responses in a consistent JSON format.
class ErrorDto : public oatpp::DTO {
    DTO_INIT(ErrorDto, DTO)

    DTO_FIELD(String, error);
};

#include OATPP_CODEGEN_END(DTO)