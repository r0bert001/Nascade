#pragma once

#include <fstream>
#include <mutex>
#include <string>

namespace nascade {

class Log {
public:
    static Log& instance();

    bool init(const std::string& file);
    void debug(const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);

private:
    Log() = default;
    void write(const char* level, const std::string& message);

    std::mutex mutex_;
    std::ofstream file_;
    std::string path_;
};

}  // namespace nascade
