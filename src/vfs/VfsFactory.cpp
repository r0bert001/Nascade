#include "vfs/VfsFactory.h"

#include "core/Util.h"
#include "vfs/LocalVfs.h"

#ifdef NASCADE_HAVE_LIBSMB2
#include "vfs/SmbVfs.h"
#endif

namespace nascade {

std::string localNasRootFromEnv() {
    const std::string root = util::getEnv("NASCADE_LOCAL_NAS");
    return root.empty() ? "" : util::normalizePath(root);
}

std::unique_ptr<Vfs> makeVfs(const SmbProfile& profile, const std::string& localNasRoot,
                             std::string& error) {
    std::unique_ptr<Vfs> vfs;
    if (!localNasRoot.empty()) {
        vfs = std::make_unique<LocalVfs>(localNasRoot);
    } else {
#ifdef NASCADE_HAVE_LIBSMB2
        vfs = std::make_unique<SmbVfs>(profile);
#else
        error = "当前构建未启用 libsmb2 后端";
        return nullptr;
#endif
    }

    if (!vfs->connect(error)) {
        return nullptr;
    }
    return vfs;
}

}  // namespace nascade
