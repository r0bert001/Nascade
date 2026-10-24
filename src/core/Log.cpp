#include "core/Log.h"

#include <cstdio>
#include <sys/stat.h>

#include "core/Util.h"

namespace nascade {

Log& Log::instance() {
    static Log log;
    return log;
}

bool Log::init(const std::string& file) {
    std::lock_guard<std::mutex> lock(mutex_);
    path_ = file;

    struct stat st {};
    if (stat(path_.c_str(), &st) == 0 && st.st_size > 2 * 1024 * 1024) {
        const std::string old = path_ + ".1";
        std::remove(old.c_str());
        std::rename(path_.c_str(), old.c_str());
    }

    file_.open(path_, std::ios::app);
    if (!file_) {
        std::fprintf(stderr, "[Nascade] cannot open log file: %s\n", path_.c_str());
        return false;
    }
    return true;
}

void Log::debug(const std::string& message) { write("DEBUG", message); }
void Log::info(const std::string& message) { write("INFO", message); }
void Log::warn(const std::string& message) { write("WARN", message); }
void Log::error(const std::string& message) { write("ERROR", message); }

void Log::write(const char* level, const std::string& message) {
    const std::string safe = util::redactSecrets(message);
    const std::string line = "[" + util::nowStamp() + "] [" + level + "] " + safe;
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_) {
        file_ << line << '\n';
        file_.flush();
    }
    std::fprintf(stderr, "%s\n", line.c_str());
}

}  // namespace nascade
