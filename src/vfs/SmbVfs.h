#pragma once

#include <memory>
#include <string>

#include "core/Config.h"
#include "vfs/Vfs.h"

struct smb2_context;

namespace nascade {

class SmbVfs : public Vfs {
public:
    explicit SmbVfs(SmbProfile profile);
    ~SmbVfs() override;

    bool connect(std::string& error) override;
    void disconnect() override;
    bool connected() const override { return connected_; }
    bool alive() const override;

    bool list(const std::string& path, std::vector<VfsEntry>& out, std::string& error) override;
    bool stat(const std::string& path, VfsStat& out, std::string& error) override;
    std::unique_ptr<VfsReader> open(const std::string& path, std::string& error) override;
    bool ensureDir(const std::string& path, std::string& error) override;

private:
    std::string smbPath(const std::string& path) const;
    static bool safeRelative(const std::string& path);

    SmbProfile profile_;
    smb2_context* context_ = nullptr;
    bool connected_ = false;
};

}  // namespace nascade
