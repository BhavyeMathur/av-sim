#include "Database.h"

#include <vector>

auto flatten_config(const SimulationConfigs &cfg) {
    std::vector<Database::KV> out;

    out.emplace_back("sim.length_s", std::to_string(cfg.sim.length_s));
    out.emplace_back("sim.h3_resolution", std::to_string(cfg.sim.h3_resolution));

    out.emplace_back("policy.matching", cfg.policy.matching);
    out.emplace_back("policy.charging", cfg.policy.charging);
    out.emplace_back("policy.charging_hubs", cfg.policy.charging_hubs);

    out.emplace_back("fleet.frac_2_seater", std::to_string(cfg.fleet.frac_2_seater));
    out.emplace_back("fleet.frac_4_seater", std::to_string(cfg.fleet.frac_4_seater));
    out.emplace_back("fleet.frac_6_seater", std::to_string(cfg.fleet.frac_6_seater));
    out.emplace_back("fleet.fleet_size", std::to_string(cfg.fleet.fleet_size));

    return out;
}

Database::Database(const std::string &db_path) {
    int rc = sqlite3_open(db_path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string msg = db_ ? sqlite3_errmsg(db_) : "unknown sqlite open error";
        if (db_) sqlite3_close(db_);
        throw std::runtime_error("Failed to open database: " + msg);
    }

    exec("PRAGMA journal_mode = WAL;");
    exec("PRAGMA foreign_keys = ON;");
    exec("PRAGMA synchronous = NORMAL;");
}

Database::~Database() {
    if (db_)
        sqlite3_close(db_);
}

void Database::init_schema() {
    exec(R"sql(
            CREATE TABLE IF NOT EXISTS runs (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                experiment TEXT NOT NULL,
                config_path TEXT NOT NULL,
                requests_path TEXT NOT NULL,
                riders_path TEXT NOT NULL,
                output_dir TEXT NOT NULL,
                started_at TEXT NOT NULL,
                duration_ms INTEGER NOT NULL
            );
        )sql");

    exec(R"sql(
            CREATE TABLE IF NOT EXISTS run_configs (
                run_id INTEGER NOT NULL,
                key TEXT NOT NULL,
                value TEXT NOT NULL,
                FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE
            );
        )sql");

    exec("CREATE INDEX IF NOT EXISTS idx_runs_experiment ON runs(experiment);");
    exec("CREATE INDEX IF NOT EXISTS idx_run_configs_run_id ON run_configs(run_id);");
    exec("CREATE INDEX IF NOT EXISTS idx_run_configs_key ON run_configs(key);");
}

void Database::create_run(const SimulationConfigs &configs, const std::string &config_path,
                          const std::string &started_at, int64_t duration_ms) {
    begin();
    try {
        const auto run_id = insert_run(configs, config_path, started_at, duration_ms);
        insert_run_configs(run_id, configs);
        commit();
    } catch (...) {
        rollback();
        throw;
    }
}

int64_t Database::insert_run(const SimulationConfigs &configs, const std::string &config_path,
                             const std::string &started_at, int64_t duration_ms) {
    static constexpr const char *sql = R"sql(
            INSERT INTO runs (
                experiment,
                config_path,
                requests_path,
                riders_path,
                output_dir,
                started_at,
                duration_ms
            ) VALUES (?, ?, ?, ?, ?, ?, ?);
        )sql";

    sqlite3_stmt *stmt = prepare(sql);
    bind_text(stmt, 1, configs.name);
    bind_text(stmt, 2, config_path);
    bind_text(stmt, 3, configs.sim.requests_file);
    bind_text(stmt, 4, configs.sim.riders_file);
    bind_text(stmt, 5, configs.sim.output);
    bind_text(stmt, 6, started_at);
    bind_int64(stmt, 7, duration_ms);

    step_done(stmt);
    finalize(stmt);

    return static_cast<int64_t>(sqlite3_last_insert_rowid(db_));
}

void Database::insert_run_configs(int64_t run_id, const SimulationConfigs &configs) {
    static constexpr const char *sql = R"sql(
            INSERT INTO run_configs (run_id, key, value)
            VALUES (?, ?, ?);
        )sql";

    sqlite3_stmt *stmt = prepare(sql);

    for (const auto &[key, value]: flatten_config(configs)) {
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);

        bind_int64(stmt, 1, run_id);
        bind_text(stmt, 2, key);
        bind_text(stmt, 3, value);

        step_done(stmt);
    }

    finalize(stmt);
}

void Database::exec(const char *sql) {
    char *errmsg = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &errmsg) != SQLITE_OK) {
        std::string msg = errmsg ? errmsg : "unknown sqlite exec error";
        sqlite3_free(errmsg);
        throw std::runtime_error("SQLite exec failed: " + msg);
    }
}

sqlite3_stmt *Database::prepare(const char *sql) {
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
        throw std::runtime_error("SQLite prepare failed: " + std::string(sqlite3_errmsg(db_)));

    return stmt;
}

void Database::step_done(sqlite3_stmt *stmt) {
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
        throw std::runtime_error("SQLite step failed: " + std::string(sqlite3_errmsg(db_)));
}

void Database::bind_text(sqlite3_stmt *stmt, int idx, const std::string &value) {
    int rc = sqlite3_bind_text(stmt, idx, value.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK)
        throw std::runtime_error("SQLite bind_text failed");
}

void Database::bind_int64(sqlite3_stmt *stmt, int idx, int64_t value) {
    int rc = sqlite3_bind_int64(stmt, idx, static_cast<sqlite3_int64>(value));
    if (rc != SQLITE_OK)
        throw std::runtime_error("SQLite bind_int64 failed");
}
