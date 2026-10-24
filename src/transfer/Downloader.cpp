#include "transfer/Downloader.h"

#include <cstdio>
#include <fstream>
#include <vector>

#include "core/Log.h"
#include "core/Util.h"
#include "vfs/VfsFactory.h"

namespace nascade {

DownloadResult downloadFile(const SmbProfile& profile, const std::string& localNasRoot,
                            const std::string& remotePath, const std::string& localPath,
                            uint64_t expectedSize, std::atomic<uint64_t>* progress) {
    DownloadResult result;

    if (remotePath.empty() || localPath.empty()) {
        result.error = "远程或本地路径为空";
        return result;
    }
    if (util::fileExists(localPath)) {
        result.error = "目标文件已存在";
        return result;
    }

    util::ensureDir(util::parentPath(localPath));
    const std::string partPath = localPath + ".part";

    uint64_t offset = 0;
    if (util::fileExists(partPath) && !util::fileSize(partPath, offset)) {
        result.error = "无法读取临时文件";
        return result;
    }
    result.resumed = offset > 0;

    std::string error;
    std::unique_ptr<Vfs> vfs = makeVfs(profile, localNasRoot, error);
    if (!vfs) {
        result.error = error;
        return result;
    }

    if (expectedSize == 0) {
        VfsStat st {};
        if (vfs->stat(remotePath, st, error)) expectedSize = st.size;
    }

    if (expectedSize > 0 && offset > expectedSize) {
        result.error = "临时文件大于源文件";
        vfs->disconnect();
        return result;
    }

    if (expectedSize > 0 && offset == expectedSize) {
        std::remove(localPath.c_str());
        if (std::rename(partPath.c_str(), localPath.c_str()) != 0) {
            result.error = "无法重命名临时文件";
            vfs->disconnect();
            return result;
        }
        vfs->disconnect();
        result.ok = true;
        result.bytes = offset;
        if (progress) progress->store(offset);
        return result;
    }

    std::unique_ptr<VfsReader> reader = vfs->open(remotePath, error);
    if (!reader) {
        result.error = error;
        vfs->disconnect();
        return result;
    }
    if (offset > 0 && !reader->seek(offset)) {
        result.error = "无法定位断点";
        reader.reset();
        vfs->disconnect();
        return result;
    }

    std::ofstream out(partPath, std::ios::binary |
                                (offset > 0 ? std::ios::app : std::ios::trunc));
    if (!out) {
        result.error = "无法创建临时文件";
        vfs->disconnect();
        return result;
    }

    std::vector<char> buffer(256 * 1024);
    uint64_t done = offset;
    if (progress) progress->store(done);

    bool failed = false;
    while (true) {
        const int64_t read = reader->read(buffer.data(), buffer.size());
        if (read < 0) {
            result.error = "读取远程文件失败";
            failed = true;
            break;
        }
        if (read == 0) break;

        out.write(buffer.data(), static_cast<std::streamsize>(read));
        if (!out.good()) {
            result.error = "写入本地文件失败";
            failed = true;
            break;
        }
        done += static_cast<uint64_t>(read);
        if (progress) progress->store(done);
    }
    Log::instance().info("download finalize: flush " + localPath);
    out.close();

    if (!failed && expectedSize > 0 && done != expectedSize) {
        result.error = "传输不完整";
        failed = true;
    }

    if (failed) {
        reader.reset();
        vfs->disconnect();
        return result;
    }

    std::remove(localPath.c_str());
    Log::instance().info("download finalize: rename " + localPath);
    if (std::rename(partPath.c_str(), localPath.c_str()) != 0) {
        result.error = "无法重命名临时文件";
        reader.reset();
        vfs->disconnect();
        return result;
    }

    // 必须先释放 reader 关闭 SMB 文件句柄，再销毁 SMB context。
    // 否则 SmbReader 析构会拿已释放的 context 调用 smb2_close。
    Log::instance().info("download finalize: close " + localPath);
    reader.reset();
    Log::instance().info("download finalize: disconnect " + localPath);
    vfs->disconnect();
    result.ok = true;
    result.bytes = done;
    Log::instance().info("download complete: " + localPath + " (" + std::to_string(done) + " bytes)");
    return result;
}

}  // namespace nascade
