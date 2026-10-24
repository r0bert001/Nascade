#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace nascade {

struct VfsEntry {
    std::string name;
    bool isDir = false;
    uint64_t size = 0;
    int64_t mtime = 0;
};

struct VfsStat {
    bool isDir = false;
    uint64_t size = 0;
    int64_t mtime = 0;
};

class VfsReader {
public:
    virtual ~VfsReader() = default;
    virtual int64_t read(void* buffer, size_t length) = 0;
    virtual bool seek(uint64_t offset) = 0;
    virtual uint64_t tell() const = 0;
};

class Vfs {
public:
    virtual ~Vfs() = default;
    virtual bool connect(std::string& error) = 0;
    virtual void disconnect() = 0;
    virtual bool connected() const = 0;
    virtual bool alive() const = 0;

    virtual bool list(const std::string& path, std::vector<VfsEntry>& out, std::string& error) = 0;
    virtual bool stat(const std::string& path, VfsStat& out, std::string& error) = 0;
    virtual std::unique_ptr<VfsReader> open(const std::string& path, std::string& error) = 0;
    virtual bool ensureDir(const std::string& path, std::string& error) = 0;
};

}  // namespace nascade
