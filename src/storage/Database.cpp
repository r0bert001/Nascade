#include "storage/Database.h"

#include <sqlite3.h>

#include "core/Paths.h"
#include "core/Util.h"

namespace nascade {

Database::~Database() {
    close();
}

bool Database::open(const std::string& path, std::string& error) {
    close();
    util::ensureDir(util::parentPath(path));

    if (sqlite3_open_v2(path.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
        error = db_ ? sqlite3_errmsg(db_) : "sqlite3_open_v2 failed";
        close();
        return false;
    }
    sqlite3_busy_timeout(db_, 3000);

    if (!exec("PRAGMA journal_mode=DELETE;", error)) return false;
    if (!exec("PRAGMA synchronous=FULL;", error)) return false;
    if (!exec("PRAGMA foreign_keys=ON;", error)) return false;
    return createSchema(error);
}

void Database::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::exec(const std::string& sql, std::string& error) {
    if (!db_) {
        error = "database is not open";
        return false;
    }
    char* message = nullptr;
    const int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &message);
    if (rc != SQLITE_OK) {
        error = message ? message : sqlite3_errmsg(db_);
        if (message) sqlite3_free(message);
        return false;
    }
    return true;
}

bool Database::prepare(const std::string& sql, sqlite3_stmt** stmt, std::string& error) {
    if (!db_) {
        error = "database is not open";
        return false;
    }
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, stmt, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db_);
        return false;
    }
    return true;
}

bool Database::createSchema(std::string& error) {
    const char* schema = R"SQL(
CREATE TABLE IF NOT EXISTS scrape_cache (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    provider TEXT NOT NULL,
    game_key TEXT NOT NULL,
    platform_id TEXT,
    language TEXT NOT NULL DEFAULT 'zh',
    provider_game_id TEXT,
    title TEXT,
    description TEXT,
    genre TEXT,
    developer TEXT,
    publisher TEXT,
    release_date TEXT,
    players TEXT,
    rating TEXT,
    cover_url TEXT,
    screenshot_url TEXT,
    updated_at INTEGER NOT NULL DEFAULT 0,
    UNIQUE(provider, game_key, language)
);

CREATE TABLE IF NOT EXISTS media_cache (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    url TEXT NOT NULL UNIQUE,
    local_path TEXT NOT NULL,
    content_type TEXT,
    bytes INTEGER DEFAULT 0,
    updated_at INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS download_tasks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    remote_path TEXT NOT NULL,
    local_path TEXT NOT NULL,
    platform_id TEXT,
    total_bytes INTEGER NOT NULL DEFAULT 0,
    downloaded_bytes INTEGER NOT NULL DEFAULT 0,
    status TEXT NOT NULL DEFAULT 'pending',
    error TEXT,
    created_at INTEGER NOT NULL DEFAULT 0,
    updated_at INTEGER NOT NULL DEFAULT 0
);
)SQL";
    return exec(schema, error);
}

}  // namespace nascade
