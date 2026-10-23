#include "ecu_repository.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

void throwDatabaseError(sqlite3* database, const std::string& operation)
{
    throw std::runtime_error(operation + ": " + sqlite3_errmsg(database));
}

void execute(sqlite3* database, const char* sql)
{
    char* errorMessage = nullptr;
    const int result = sqlite3_exec(database, sql, nullptr, nullptr, &errorMessage);
    if (result != SQLITE_OK) {
        const std::string message = errorMessage ? errorMessage : sqlite3_errmsg(database);
        sqlite3_free(errorMessage);
        throw std::runtime_error(message);
    }
}

class Statement {
public:
    Statement(sqlite3* database, const char* sql) : m_database(database)
    {
        const int result = sqlite3_prepare_v2(database, sql, -1, &m_statement, nullptr);
        if (result != SQLITE_OK) {
            throwDatabaseError(database, "Preparing SQL statement failed");
        }
    }

    ~Statement() { sqlite3_finalize(m_statement); }

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    sqlite3_stmt* get() const { return m_statement; }

    void bindText(int index, const oatpp::String& value)
    {
        const int result =
            value ? sqlite3_bind_text(m_statement, index, value->c_str(),
                                      static_cast<int>(value->size()), SQLITE_TRANSIENT)
                  : sqlite3_bind_null(m_statement, index);
        if (result != SQLITE_OK) {
            throwDatabaseError(m_database, "Binding SQL parameter failed");
        }
    }

    void bindText(int index, const std::string& value)
    {
        const int result = sqlite3_bind_text(m_statement, index, value.c_str(),
                                             static_cast<int>(value.size()), SQLITE_TRANSIENT);
        if (result != SQLITE_OK) {
            throwDatabaseError(m_database, "Binding SQL parameter failed");
        }
    }

private:
    sqlite3* m_database;
    sqlite3_stmt* m_statement{nullptr};
};

std::string columnString(sqlite3_stmt* statement, int column)
{
    const auto* text = sqlite3_column_text(statement, column);
    return text ? reinterpret_cast<const char*>(text) : std::string{};
}

oatpp::Object<EcuDto> readEcu(sqlite3_stmt* statement)
{
    auto ecu = EcuDto::createShared();
    ecu->id = columnString(statement, 0);
    ecu->name = columnString(statement, 1);
    ecu->status = columnString(statement, 2);
    if (sqlite3_column_type(statement, 3) != SQLITE_NULL) {
        ecu->firmwareVersion = columnString(statement, 3);
    }
    return ecu;
}

void bindEcu(Statement& statement, const oatpp::Object<EcuDto>& ecu)
{
    statement.bindText(1, ecu->id);
    statement.bindText(2, ecu->name);
    statement.bindText(3, ecu->status);
    statement.bindText(4, ecu->firmwareVersion);
}

} // namespace

EcuRepository::EcuRepository(const std::string& databasePath)
{
    const int result = sqlite3_open_v2(
        databasePath.c_str(), &m_database,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
    if (result != SQLITE_OK) {
        const std::string message =
            m_database ? sqlite3_errmsg(m_database) : "could not allocate database handle";
        if (m_database) {
            sqlite3_close(m_database);
            m_database = nullptr;
        }
        throw std::runtime_error("Opening SQLite database failed: " + message);
    }
    const int timeoutResult = sqlite3_busy_timeout(m_database, 5000);
    if (timeoutResult != SQLITE_OK) {
        const std::string message = sqlite3_errmsg(m_database);
        sqlite3_close(m_database);
        m_database = nullptr;
        throw std::runtime_error("Setting SQLite busy timeout failed: " + message);
    }
    try {
        initialize();
    }
    catch (...) {
        sqlite3_close(m_database);
        m_database = nullptr;
        throw;
    }
}

EcuRepository::~EcuRepository()
{
    if (m_database) {
        sqlite3_close(m_database);
    }
}

void EcuRepository::initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    execute(m_database, "CREATE TABLE IF NOT EXISTS ecus ("
                        "id TEXT PRIMARY KEY NOT NULL,"
                        "name TEXT NOT NULL,"
                        "status TEXT NOT NULL,"
                        "firmware_version TEXT"
                        ");");

    execute(m_database, "BEGIN IMMEDIATE TRANSACTION;");
    try {
        std::int64_t recordCount = 0;
        {
            Statement countStatement(m_database, "SELECT COUNT(*) FROM ecus;");
            if (sqlite3_step(countStatement.get()) != SQLITE_ROW) {
                throwDatabaseError(m_database, "Counting ECU records failed");
            }
            recordCount = sqlite3_column_int64(countStatement.get(), 0);
        }

        if (recordCount == 0) {
            Statement insertStatement(
                m_database,
                "INSERT INTO ecus (id, name, status, firmware_version) VALUES (?, ?, ?, ?);");

            auto engine = EcuDto::createShared();
            engine->id = "engine";
            engine->name = "Engine Controller";
            engine->status = "online";
            engine->firmwareVersion = "1.0.0";
            bindEcu(insertStatement, engine);
            if (sqlite3_step(insertStatement.get()) != SQLITE_DONE) {
                throwDatabaseError(m_database, "Seeding engine ECU failed");
            }
            sqlite3_reset(insertStatement.get());
            sqlite3_clear_bindings(insertStatement.get());

            auto battery = EcuDto::createShared();
            battery->id = "battery";
            battery->name = "Battery Controller";
            battery->status = "online";
            battery->firmwareVersion = "1.0.0";
            bindEcu(insertStatement, battery);
            if (sqlite3_step(insertStatement.get()) != SQLITE_DONE) {
                throwDatabaseError(m_database, "Seeding battery ECU failed");
            }
        }
        execute(m_database, "COMMIT;");
    }
    catch (...) {
        sqlite3_exec(m_database, "ROLLBACK;", nullptr, nullptr, nullptr);
        throw;
    }
}

std::vector<oatpp::Object<EcuDto>> EcuRepository::list() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    Statement statement(m_database,
                        "SELECT id, name, status, firmware_version FROM ecus ORDER BY id;");
    std::vector<oatpp::Object<EcuDto>> ecus;
    int result = SQLITE_OK;
    while ((result = sqlite3_step(statement.get())) == SQLITE_ROW) {
        ecus.push_back(readEcu(statement.get()));
    }
    if (result != SQLITE_DONE) {
        throwDatabaseError(m_database, "Listing ECU records failed");
    }
    return ecus;
}

oatpp::Object<EcuDto> EcuRepository::findById(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    Statement statement(m_database,
                        "SELECT id, name, status, firmware_version FROM ecus WHERE id = ?;");
    statement.bindText(1, id);
    const int result = sqlite3_step(statement.get());
    if (result == SQLITE_DONE) {
        return nullptr;
    }
    if (result != SQLITE_ROW) {
        throwDatabaseError(m_database, "Looking up ECU record failed");
    }
    return readEcu(statement.get());
}

bool EcuRepository::create(const oatpp::Object<EcuDto>& ecu)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    Statement statement(
        m_database,
        "INSERT OR IGNORE INTO ecus (id, name, status, firmware_version) VALUES (?, ?, ?, ?);");
    bindEcu(statement, ecu);
    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
        throwDatabaseError(m_database, "Creating ECU record failed");
    }
    return sqlite3_changes(m_database) == 1;
}

bool EcuRepository::replace(const oatpp::Object<EcuDto>& ecu)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    Statement statement(m_database,
                        "UPDATE ecus SET name = ?, status = ?, firmware_version = ? WHERE id = ?;");
    statement.bindText(1, ecu->name);
    statement.bindText(2, ecu->status);
    statement.bindText(3, ecu->firmwareVersion);
    statement.bindText(4, ecu->id);
    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
        throwDatabaseError(m_database, "Replacing ECU record failed");
    }
    return sqlite3_changes(m_database) == 1;
}

bool EcuRepository::remove(const std::string& id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    Statement statement(m_database, "DELETE FROM ecus WHERE id = ?;");
    statement.bindText(1, id);
    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
        throwDatabaseError(m_database, "Deleting ECU record failed");
    }
    return sqlite3_changes(m_database) == 1;
}