#pragma once

#include "vfs/Vfs.h"

namespace nascade {

class LocalVfs : public Vfs {
public:
    explicit LocalVfs(std::string root = {});

    bool connect(std::string& error) override;
    void disconnect() override;
    bool connected() const override { return connected_; }
    bool alive() const override { return connected_; }

    bool list(const std::string& path, std::vector<VfsEntry>& out, std::string& error) override;
    bool stat(const std::string& path, VfsStat& out, std::string& error) override;
    std::unique_ptr<VfsReader> open(const std::string& path, std::string& error) override;
    bool ensureDir(const std::string& path, std::string& error) override;

private:
    std::string pathFor(const std::string& path) const;
    bool safeRelative(const std::string& path) const;

    std::string root_;
    bool connected_ = false;
};

}  // namespace nascade
