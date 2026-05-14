#pragma once

#include <string>
#include <sqlite3.h>

struct SimulationConfigs;

class Database {
public:
    using KV = std::pair<std::string, std::string>;

    explicit Database(const std::string &db_path);

    ~Database();

    Database(const Database &) = delete;

    Database &operator=(const Database &) = delete;

    void init_schema();

    void begin() { exec("BEGIN IMMEDIATE;"); }

    void commit() { exec("COMMIT;"); }

    void rollback() noexcept { if (db_) sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr); }

    void create_run(const SimulationConfigs &configs, const std::string &config_path,
                    const std::string &started_at, int64_t duration_ms);

    int64_t insert_run(const SimulationConfigs &configs, const std::string &config_path,
                       const std::string &started_at, int64_t duration_ms);

    void insert_run_configs(int64_t run_id, const SimulationConfigs &configs);

private:
    sqlite3 *db_ = nullptr;

    void exec(const char *sql);

    sqlite3_stmt *prepare(const char *sql);

    void step_done(sqlite3_stmt *stmt);

    static void finalize(sqlite3_stmt *stmt) noexcept { if (stmt) sqlite3_finalize(stmt); }

    static void bind_text(sqlite3_stmt *stmt, int idx, const std::string &value);

    static void bind_int64(sqlite3_stmt *stmt, int idx, int64_t value);
};
