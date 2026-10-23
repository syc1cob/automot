#pragma once

#include "ecu_dto.hpp"

#include <sqlite3.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

class EcuRepository {
public:
    explicit EcuRepository(const std::string& databasePath);
    ~EcuRepository();

    EcuRepository(const EcuRepository&) = delete;
    EcuRepository& operator=(const EcuRepository&) = delete;

    std::vector<oatpp::Object<EcuDto>> list() const;
    oatpp::Object<EcuDto> findById(const std::string& id) const;
    bool create(const oatpp::Object<EcuDto>& ecu);
    bool replace(const oatpp::Object<EcuDto>& ecu);
    bool remove(const std::string& id);

private:
    void initialize();

    sqlite3* m_database{nullptr};
    mutable std::mutex m_mutex;
};