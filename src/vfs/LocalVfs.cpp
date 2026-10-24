#include "vfs/LocalVfs.h"

#include <fstream>
#include <sys/stat.h>

#include "core/Util.h"

namespace nascade {

namespace {

class LocalReader : public VfsReader {
public:
    explicit LocalReader(const std::string& path) : stream_(path, std::ios::binary) {}

    bool ok() const { return stream_.good(); }

    int64_t read(void* buffer, size_t length) override {
        stream_.read(static_cast<char*>(buffer), static_cast<std::streamsize>(length));
        const std::streamsize got = stream_.gcount();
        if (got > 0) return static_cast<int64_t>(got);
        return stream_.eof() ? 0 : -1;
    }

    bool seek(uint64_t offset) override {
        stream_.clear();
        stream_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        return stream_.good();
    }

    uint64_t tell() const override {
        return static_cast<uint64_t>(stream_.tellg());
    }

private:
    mutable std::ifstream stream_;
};

}  // namespace

LocalVfs::LocalVfs(std::string root)
    : root_(std::move(root)) {
    if (!root_.empty()) root_ = util::normalizePath(root_);
}

std::string LocalVfs::pathFor(const std::string& path) const {
    if (root_.empty()) return path;
    if (path.empty()) return root_;
    std::string relative = path;
    while (!relative.empty() && relative.front() == '/') relative.erase(relative.begin());
    if (relative.empty()) return root_;
    return util::joinPath(root_, relative);
}

bool LocalVfs::safeRelative(const std::string& path) const {
    if (root_.empty()) return true;
    for (const auto& part : util::split(path, '/')) {
        if (part == "..") return false;
    }
    return true;
}

bool LocalVfs::connect(std::string& error) {
    (void)error;
    connected_ = true;
    return true;
}

void LocalVfs::disconnect() {
    connected_ = false;
}

bool LocalVfs::list(const std::string& path, std::vector<VfsEntry>& out, std::string& error) {
    out.clear();
    if (!safeRelative(path)) {
        error = "非法路径";
        return false;
    }
    const std::string fullPath = pathFor(path);
    for (const auto& entry : util::listDir(fullPath)) {
        VfsEntry e;
        e.name = entry.name;
        e.isDir = entry.isDir;
        e.size = entry.size;
        e.mtime = entry.mtime;
        out.push_back(std::move(e));
    }
    if (out.empty() && !util::isDirectory(fullPath)) {
        error = "目录不存在: " + fullPath;
        return false;
    }
    return true;
}

bool LocalVfs::stat(const std::string& path, VfsStat& out, std::string& error) {
    if (!safeRelative(path)) {
        error = "非法路径";
        return false;
    }
    const std::string fullPath = pathFor(path);
    struct stat st {};
    if (::stat(fullPath.c_str(), &st) != 0) {
        error = "文件不存在: " + fullPath;
        return false;
    }
    out.isDir = S_ISDIR(st.st_mode);
    out.size = static_cast<uint64_t>(st.st_size);
    out.mtime = static_cast<int64_t>(st.st_mtime);
    return true;
}

std::unique_ptr<VfsReader> LocalVfs::open(const std::string& path, std::string& error) {
    if (!safeRelative(path)) {
        error = "非法路径";
        return nullptr;
    }
    const std::string fullPath = pathFor(path);
    auto reader = std::make_unique<LocalReader>(fullPath);
    if (!reader->ok()) {
        error = "无法打开文件: " + fullPath;
        return nullptr;
    }
    return reader;
}

bool LocalVfs::ensureDir(const std::string& path, std::string& error) {
    if (!safeRelative(path)) {
        error = "非法路径";
        return false;
    }
    const std::string fullPath = pathFor(path);
    if (!util::ensureDir(fullPath)) {
        error = "无法创建目录: " + fullPath;
        return false;
    }
    return true;
}

}  // namespace nascade
