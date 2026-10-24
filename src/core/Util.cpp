#include "core/Util.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

#ifdef _WIN32
#include <direct.h>
#endif

namespace nascade::util {

namespace {

int makeDirectory(const char* path) {
#ifdef _WIN32
    return _mkdir(path);
#else
    return mkdir(path, 0755);
#endif
}

}  // namespace

std::string joinPath(const std::string& a, const std::string& b) {
    if (b.empty()) return a;
    std::string bb = b;
    std::replace(bb.begin(), bb.end(), '\\', '/');
    if (!bb.empty() && (bb.front() == '/' || (bb.size() > 1 && bb[1] == ':'))) {
        return bb;
    }
    if (a.empty()) return bb;
    if (a.back() == '/') return a + bb;
    return a + "/" + bb;
}

std::string normalizePath(const std::string& path) {
    if (path.empty()) return path;

    std::string p = path;
    std::replace(p.begin(), p.end(), '\\', '/');

    bool absolute = !p.empty() && p.front() == '/';
    std::string drive;
    size_t start = 0;
    if (!absolute && p.size() >= 2 && std::isalpha(static_cast<unsigned char>(p[0])) && p[1] == ':') {
        drive = p.substr(0, 2);
        start = 2;
        if (start < p.size() && p[start] == '/') ++start;
    }

    std::vector<std::string> parts;
    size_t i = start;
    while (i <= p.size()) {
        size_t j = p.find('/', i);
        if (j == std::string::npos) j = p.size();
        const std::string seg = p.substr(i, j - i);
        if (!seg.empty() && seg != ".") {
            if (seg == "..") {
                if (!parts.empty() && parts.back() != "..") {
                    parts.pop_back();
                } else if (!absolute && drive.empty()) {
                    parts.push_back("..");
                }
            } else {
                parts.push_back(seg);
            }
        }
        if (j == p.size()) break;
        i = j + 1;
    }

    std::string out = drive;
    if (absolute) out += '/';
    else if (!drive.empty()) out += '/';
    for (size_t k = 0; k < parts.size(); ++k) {
        if (k > 0) out += '/';
        out += parts[k];
    }
    if (out.empty()) out = absolute ? "/" : ".";
    if (!drive.empty() && out.size() == 2) out += '/';
    return out;
}

std::string parentPath(const std::string& path) {
    const std::string p = normalizePath(path);
    const size_t pos = p.find_last_of('/');
    if (pos == std::string::npos) return ".";
    if (pos == 0) return "/";
    return p.substr(0, pos);
}

std::string baseName(const std::string& path) {
    const std::string p = normalizePath(path);
    const size_t pos = p.find_last_of('/');
    return (pos == std::string::npos) ? p : p.substr(pos + 1);
}

std::string baseNameNoExt(const std::string& path) {
    std::string name = baseName(path);
    const size_t pos = name.find_last_of('.');
    if (pos != std::string::npos && pos > 0) name = name.substr(0, pos);
    return name;
}

std::string extensionLower(const std::string& path) {
    const std::string name = baseName(path);
    const size_t pos = name.find_last_of('.');
    if (pos == std::string::npos || pos + 1 >= name.size()) return "";
    return toLower(name.substr(pos + 1));
}

std::string trim(const std::string& s) {
    size_t begin = 0;
    size_t end = s.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(s[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(begin, end - begin);
}

std::string toLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::string current;
    for (char c : s) {
        if (c == delim) {
            out.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    out.push_back(current);
    return out;
}

std::vector<std::string> splitAny(const std::string& s, const std::string& delims) {
    std::vector<std::string> out;
    std::string current;
    for (char c : s) {
        if (delims.find(c) != std::string::npos) {
            if (!current.empty()) {
                out.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) out.push_back(current);
    return out;
}

std::string formatSize(uint64_t bytes) {
    static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    char buf[64];
    if (unit == 0) {
        std::snprintf(buf, sizeof(buf), "%llu %s", static_cast<unsigned long long>(bytes), units[unit]);
    } else {
        std::snprintf(buf, sizeof(buf), "%.1f %s", value, units[unit]);
    }
    return buf;
}

std::string formatTime(int64_t t) {
    if (t <= 0) return "-";
    std::time_t tt = static_cast<std::time_t>(t);
    std::tm* tm = std::localtime(&tt);
    if (!tm) return "-";
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", tm);
    return buf;
}

std::string nowStamp() {
    std::time_t tt = std::time(nullptr);
    std::tm* tm = std::localtime(&tt);
    if (!tm) return "";
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
    return buf;
}

std::string getEnv(const std::string& name, const std::string& fallback) {
    const char* value = std::getenv(name.c_str());
    return value ? std::string(value) : fallback;
}

std::string currentWorkingDir() {
    char buf[4096];
    if (getcwd(buf, sizeof(buf)) != nullptr) return buf;
    return ".";
}

bool fileExists(const std::string& path) {
    struct stat st {};
    return stat(path.c_str(), &st) == 0;
}

bool fileSize(const std::string& path, uint64_t& size) {
    struct stat st {};
    if (stat(path.c_str(), &st) != 0 || S_ISDIR(st.st_mode)) return false;
    size = static_cast<uint64_t>(st.st_size);
    return true;
}

bool isDirectory(const std::string& path) {
    struct stat st {};
    if (stat(path.c_str(), &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

bool ensureDir(const std::string& path) {
    const std::string p = normalizePath(path);
    if (p.empty()) return false;

    std::string cur;
    size_t i = 0;
    if (p.front() == '/') {
        cur = "/";
        i = 1;
    } else if (p.size() >= 2 && p[1] == ':') {
        cur = p.substr(0, 2);
        i = 2;
        if (i < p.size() && p[i] == '/') {
            cur += "/";
            ++i;
        }
    }

    while (i <= p.size()) {
        size_t j = p.find('/', i);
        if (j == std::string::npos) j = p.size();
        const std::string seg = p.substr(i, j - i);
        if (!seg.empty()) {
            if (!cur.empty() && cur.back() != '/') cur += "/";
            cur += seg;
            if (makeDirectory(cur.c_str()) != 0 && errno != EEXIST) {
                if (!isDirectory(cur)) return false;
            }
        }
        if (j == p.size()) break;
        i = j + 1;
    }
    return isDirectory(p);
}

bool readFile(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

bool writeFile(const std::string& path, const std::string& data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    return out.good();
}

std::vector<DirEntry> listDir(const std::string& path) {
    std::vector<DirEntry> out;
    DIR* dir = opendir(path.c_str());
    if (!dir) return out;

    while (dirent* ent = readdir(dir)) {
        const std::string name = ent->d_name;
        if (name == "." || name == "..") continue;
        const std::string full = joinPath(path, name);
        struct stat st {};
        if (stat(full.c_str(), &st) != 0) continue;
        DirEntry e;
        e.name = name;
        e.isDir = S_ISDIR(st.st_mode);
        e.size = static_cast<uint64_t>(st.st_size);
        e.mtime = static_cast<int64_t>(st.st_mtime);
        out.push_back(e);
    }
    closedir(dir);

    std::sort(out.begin(), out.end(), [](const DirEntry& a, const DirEntry& b) {
        if (a.isDir != b.isDir) return a.isDir > b.isDir;
        return toLower(a.name) < toLower(b.name);
    });
    return out;
}

std::string htmlUnescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        if (s[i] != '&') {
            out.push_back(s[i++]);
            continue;
        }
        const size_t semi = s.find(';', i + 1);
        if (semi == std::string::npos || semi - i > 12) {
            out.push_back(s[i++]);
            continue;
        }
        const std::string entity = s.substr(i + 1, semi - i - 1);
        if (entity == "amp") out.push_back('&');
        else if (entity == "lt") out.push_back('<');
        else if (entity == "gt") out.push_back('>');
        else if (entity == "quot") out.push_back('"');
        else if (entity == "apos" || entity == "#39") out.push_back('\'');
        else if (!entity.empty() && entity[0] == '#') {
            unsigned long code = 0;
            bool ok = true;
            if (entity.size() > 1 && (entity[1] == 'x' || entity[1] == 'X')) {
                for (size_t k = 2; k < entity.size(); ++k) {
                    if (!std::isxdigit(static_cast<unsigned char>(entity[k]))) { ok = false; break; }
                    code = code * 16 + static_cast<unsigned long>(std::stoul(std::string(1, entity[k]), nullptr, 16));
                }
            } else {
                for (size_t k = 1; k < entity.size(); ++k) {
                    if (!std::isdigit(static_cast<unsigned char>(entity[k]))) { ok = false; break; }
                    code = code * 10 + static_cast<unsigned long>(entity[k] - '0');
                }
            }
            if (ok && code <= 0x10FFFF) {
                if (code <= 0x7F) {
                    out.push_back(static_cast<char>(code));
                } else if (code <= 0x7FF) {
                    out.push_back(static_cast<char>(0xC0 | (code >> 6)));
                    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                } else if (code <= 0xFFFF) {
                    out.push_back(static_cast<char>(0xE0 | (code >> 12)));
                    out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                } else {
                    out.push_back(static_cast<char>(0xF0 | (code >> 18)));
                    out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                }
            } else {
                out.append(s, i, semi - i + 1);
            }
        } else {
            out.append(s, i, semi - i + 1);
        }
        i = semi + 1;
    }
    return out;
}

std::string redactSecrets(const std::string& text) {
    std::string out = text;
    const char* keys[] = {
        "devpassword=", "sspassword=", "password=", "passwd=",
        "token=", "apikey=", "api_key=", "authorization:"
    };
    for (const char* key : keys) {
        std::string lower = toLower(out);
        size_t pos = 0;
        while ((pos = lower.find(key, pos)) != std::string::npos) {
            const size_t valueStart = pos + std::strlen(key);
            size_t valueEnd = out.find_first_of("& \r\n\"", valueStart);
            if (valueEnd == std::string::npos) valueEnd = out.size();
            out.replace(valueStart, valueEnd - valueStart, "***");
            lower = toLower(out);
            pos = valueStart + 3;
        }
    }
    return out;
}

}  // namespace nascade::util
