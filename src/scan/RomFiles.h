#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace nascade {

struct RomFileEntry {
    std::string relativePath;
    std::string name;
    uint64_t size = 0;
    int64_t mtime = 0;
};

// 补充原厂 extlist 中常见但缺失的游戏扩展名（如 ATARI2600 的 7z、NDS 的 nds）。
std::vector<std::string> augmentPlatformExtensions(
    const std::string& platformId, const std::vector<std::string>& configured);

// FC/SFC 等平台存在无后缀 ROM；只对这些平台启用，并过滤 COPYING/LICENSE 等文档。
bool platformAllowsExtensionless(const std::string& platformId);

// 递归扫描平台 ROM 目录，返回符合扩展名的文件；跳过隐藏目录、.db/.part 等。
std::vector<RomFileEntry> listRomFiles(const std::string& root,
                                       const std::set<std::string>& extensions,
                                       bool includeExtensionless,
                                       int maxDepth = 5);

// 同目录已有 .cue/.m3u/.gdi 时，去掉对应的 .bin/.iso 副文件，避免重复计数/列表。
std::vector<RomFileEntry> filterDiscCompanions(const std::vector<RomFileEntry>& files);

}  // namespace nascade
