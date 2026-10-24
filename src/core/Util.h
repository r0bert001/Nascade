#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nascade::util {

struct DirEntry {
    std::string name;
    bool isDir = false;
    uint64_t size = 0;
    int64_t mtime = 0;
};

std::string joinPath(const std::string& a, const std::string& b);
std::string normalizePath(const std::string& path);
std::string parentPath(const std::string& path);
std::string baseName(const std::string& path);
std::string baseNameNoExt(const std::string& path);
std::string extensionLower(const std::string& path);
std::string trim(const std::string& s);
std::string toLower(const std::string& s);
bool startsWith(const std::string& s, const std::string& prefix);
bool endsWith(const std::string& s, const std::string& suffix);

std::vector<std::string> split(const std::string& s, char delim);
std::vector<std::string> splitAny(const std::string& s, const std::string& delims);

std::string formatSize(uint64_t bytes);
std::string formatTime(int64_t t);
std::string nowStamp();

std::string getEnv(const std::string& name, const std::string& fallback = "");
std::string currentWorkingDir();

bool fileExists(const std::string& path);
bool fileSize(const std::string& path, uint64_t& size);
bool isDirectory(const std::string& path);
bool ensureDir(const std::string& path);
bool readFile(const std::string& path, std::string& out);
bool writeFile(const std::string& path, const std::string& data);
std::vector<DirEntry> listDir(const std::string& path);

std::string htmlUnescape(const std::string& s);
std::string redactSecrets(const std::string& text);

}  // namespace nascade::util
