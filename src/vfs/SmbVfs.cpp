#include "vfs/SmbVfs.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <utility>

#if defined(_WIN32)
#ifndef _WINDOWS
#define _WINDOWS
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <poll.h>
#include <sys/socket.h>
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <smb2.h>
#include <libsmb2.h>
#pragma GCC diagnostic pop

#include "core/Log.h"
#include "core/Util.h"

namespace nascade {

namespace {

bool ensureWinsock(std::string& error) {
#if defined(_WIN32)
    static bool initialized = false;
    if (!initialized) {
        WSADATA data {};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            error = "WSAStartup 失败";
            return false;
        }
        initialized = true;
    }
#else
    (void)error;
#endif
    return true;
}

class SmbReader : public VfsReader {
public:
    SmbReader(smb2_context* context, smb2fh* handle)
        : context_(context), handle_(handle) {}

    ~SmbReader() override {
        if (handle_) {
            smb2_close(context_, handle_);
            handle_ = nullptr;
        }
    }

    int64_t read(void* buffer, size_t length) override {
        if (!handle_) return -1;
        const uint32_t request = static_cast<uint32_t>(std::min<size_t>(length, 4 * 1024 * 1024));
        const int result = smb2_pread(context_, handle_, static_cast<uint8_t*>(buffer), request, offset_);
        if (result < 0) {
            Log::instance().warn(std::string("smb2_pread failed: ") + smb2_get_error(context_));
            return -1;
        }
        offset_ += static_cast<uint64_t>(result);
        return result;
    }

    bool seek(uint64_t offset) override {
        offset_ = offset;
        return true;
    }

    uint64_t tell() const override { return offset_; }

private:
    smb2_context* context_ = nullptr;
    smb2fh* handle_ = nullptr;
    uint64_t offset_ = 0;
};

}  // namespace

SmbVfs::SmbVfs(SmbProfile profile)
    : profile_(std::move(profile)) {}

SmbVfs::~SmbVfs() {
    disconnect();
}

bool SmbVfs::safeRelative(const std::string& path) {
    for (const auto& part : util::split(path, '/')) {
        if (part == "..") return false;
    }
    return true;
}

std::string SmbVfs::smbPath(const std::string& path) const {
    // libsmb2 的 CREATE/OPEN 使用共享内相对路径，不能带前导 '/'。
    // 根目录对应空字符串；例如 root=/Roms、path=GBA/x.gba -> Roms/GBA/x.gba。
    const auto trim = [](std::string value) {
        std::replace(value.begin(), value.end(), '\\', '/');
        while (!value.empty() && value.front() == '/') value.erase(value.begin());
        while (!value.empty() && value.back() == '/') value.pop_back();
        return value;
    };

    const std::string root = trim(profile_.root.empty() ? "/" : profile_.root);
    const std::string relative = trim(path);
    if (relative.empty()) return root;
    if (root.empty()) return relative;
    return root + "/" + relative;
}

bool SmbVfs::connect(std::string& error) {
    disconnect();

    if (profile_.host.empty() || profile_.share.empty()) {
        error = "SMB 主机或共享名为空";
        return false;
    }
    if (!ensureWinsock(error)) {
        return false;
    }

    context_ = smb2_init_context();
    if (!context_) {
        error = "smb2_init_context 失败";
        return false;
    }

    smb2_set_timeout(context_, 30);
    smb2_set_security_mode(context_, SMB2_NEGOTIATE_SIGNING_ENABLED);
    smb2_set_authentication(context_, SMB2_SEC_NTLMSSP);

    if (!profile_.username.empty()) {
        smb2_set_user(context_, profile_.username.c_str());
    }
    if (!profile_.password.empty()) {
        smb2_set_password(context_, profile_.password.c_str());
    }
    if (!profile_.domain.empty()) {
        smb2_set_domain(context_, profile_.domain.c_str());
    }

    std::string server = profile_.host;
    if (profile_.port > 0 && profile_.port != 445) {
        server += ":" + std::to_string(profile_.port);
    }

    const char* user = profile_.username.empty() ? nullptr : profile_.username.c_str();
    if (smb2_connect_share(context_, server.c_str(), profile_.share.c_str(), user) != 0) {
        error = std::string("SMB 连接失败：") + smb2_get_error(context_);
        Log::instance().warn("SMB connect failed: host=" + profile_.host + " share=" + profile_.share +
                             " error=" + smb2_get_error(context_));
        smb2_destroy_context(context_);
        context_ = nullptr;
        return false;
    }

    connected_ = true;
    Log::instance().info("SMB connected: " + profile_.host + "/" + profile_.share);
    return true;
}

void SmbVfs::disconnect() {
    if (context_) {
        // 不等待 smb2_disconnect_share 的 TREE_DISCONNECT 往返：
        // 服务器离线或网络卡顿时它可能长时间阻塞，下载收尾和退出都会卡住。
        // 直接关闭 socket，SMB 服务端会按传输断开清理会话。
        smb2_destroy_context(context_);
        context_ = nullptr;
    }
    connected_ = false;
}

bool SmbVfs::alive() const {
    if (!connected_ || !context_) return false;
    const auto fd = smb2_get_fd(context_);

#if defined(_WIN32)
    if (fd == INVALID_SOCKET) return false;
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(fd, &readSet);
    timeval timeout{0, 0};
    const int ready = ::select(0, &readSet, nullptr, nullptr, &timeout);
    if (ready < 0) return WSAGetLastError() == WSAEINTR;
    if (ready == 0) return true;
    char byte = 0;
    const int result = ::recv(fd, &byte, 1, MSG_PEEK);
    if (result == 0) return false;
    if (result < 0) return WSAGetLastError() == WSAEWOULDBLOCK;
    return true;
#else
    if (fd < 0) return false;
    short events = POLLIN;
#ifdef POLLRDHUP
    events |= POLLRDHUP;
#endif
    struct pollfd pfd {fd, events, 0};
    const int ready = ::poll(&pfd, 1, 0);
    if (ready < 0) return errno == EINTR;
    if (ready == 0) return true;
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) return false;
#ifdef POLLRDHUP
    if (pfd.revents & POLLRDHUP) return false;
#endif
    if (pfd.revents & POLLIN) {
        char byte = 0;
        const ssize_t result = ::recv(fd, &byte, 1, MSG_PEEK | MSG_DONTWAIT);
        if (result == 0) return false;
        if (result < 0 && errno != EAGAIN && errno != EWOULDBLOCK) return false;
    }
    return true;
#endif
}

bool SmbVfs::list(const std::string& path, std::vector<VfsEntry>& out, std::string& error) {
    out.clear();
    if (!connected_ || !context_) {
        error = "SMB 未连接";
        return false;
    }
    if (!safeRelative(path)) {
        error = "非法路径";
        return false;
    }

    const std::string remote = smbPath(path);
    smb2dir* dir = smb2_opendir(context_, remote.c_str());
    if (!dir) {
        error = std::string("打开目录失败：") + smb2_get_error(context_);
        return false;
    }

    while (smb2dirent* entry = smb2_readdir(context_, dir)) {
        const std::string name = entry->name ? entry->name : "";
        if (name == "." || name == "..") continue;
        VfsEntry item;
        item.name = name;
        item.isDir = entry->st.smb2_type == SMB2_TYPE_DIRECTORY;
        item.size = entry->st.smb2_size;
        item.mtime = static_cast<int64_t>(entry->st.smb2_mtime);
        out.push_back(std::move(item));
    }
    smb2_closedir(context_, dir);

    std::sort(out.begin(), out.end(), [](const VfsEntry& a, const VfsEntry& b) {
        if (a.isDir != b.isDir) return a.isDir > b.isDir;
        return util::toLower(a.name) < util::toLower(b.name);
    });
    return true;
}

bool SmbVfs::stat(const std::string& path, VfsStat& out, std::string& error) {
    if (!connected_ || !context_) {
        error = "SMB 未连接";
        return false;
    }
    if (!safeRelative(path)) {
        error = "非法路径";
        return false;
    }

    const std::string remote = smbPath(path);
    smb2_stat_64 st {};
    if (smb2_stat(context_, remote.c_str(), &st) != 0) {
        error = std::string("获取文件信息失败：") + smb2_get_error(context_);
        return false;
    }
    out.isDir = st.smb2_type == SMB2_TYPE_DIRECTORY;
    out.size = st.smb2_size;
    out.mtime = static_cast<int64_t>(st.smb2_mtime);
    return true;
}

std::unique_ptr<VfsReader> SmbVfs::open(const std::string& path, std::string& error) {
    if (!connected_ || !context_) {
        error = "SMB 未连接";
        return nullptr;
    }
    if (!safeRelative(path)) {
        error = "非法路径";
        return nullptr;
    }

    const std::string remote = smbPath(path);
    smb2fh* handle = smb2_open(context_, remote.c_str(), O_RDONLY);
    if (!handle) {
        error = std::string("打开文件失败：") + smb2_get_error(context_);
        return nullptr;
    }
    return std::make_unique<SmbReader>(context_, handle);
}

bool SmbVfs::ensureDir(const std::string& path, std::string& error) {
    if (!connected_ || !context_) {
        error = "SMB 未连接";
        return false;
    }
    if (!safeRelative(path)) {
        error = "非法路径";
        return false;
    }

    const std::string remote = smbPath(path);
    const int result = smb2_mkdir(context_, remote.c_str());
    if (result != 0 && result != -EEXIST) {
        error = std::string("创建目录失败：") + smb2_get_error(context_);
        return false;
    }
    return true;
}

}  // namespace nascade
