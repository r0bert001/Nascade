#pragma once

#include <string>

struct sqlite3;
struct sqlite3_stmt;

namespace nascade {

class Database {
public:
    Database() = default;
    ~Database();

    bool open(const std::string& path, std::string& error);
    void close();
    bool initialized() const { return db_ != nullptr; }

    bool exec(const std::string& sql, std::string& error);
    bool prepare(const std::string& sql, sqlite3_stmt** stmt, std::string& error);

    sqlite3* raw() const { return db_; }

private:
    bool createSchema(std::string& error);

    sqlite3* db_ = nullptr;
};

}  // namespace nascade
