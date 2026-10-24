#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "model/Game.h"
#include "model/Platform.h"

namespace nascade {

// 按 Imgs/<平台>/ 查找封面：先精确匹配 ROM 文件名，再按去掉标签后的标题匹配。
class CoverResolver {
public:
    explicit CoverResolver(std::string imgPath);

    // 批量扫描使用索引查找，避免每个游戏都去 stat 整组图片后缀。
    std::string find(const std::string& romBase);
    // 单个游戏（如 NAS 详情）直接检查常见后缀，未命中再退回索引。
    std::string findDirect(const std::string& romBase);

private:
    void buildIndex();

    std::string imgPath_;
    bool indexBuilt_ = false;
    std::unordered_map<std::string, std::string> exact_;
    std::unordered_map<std::string, std::string> index_;
};

class GameScanner {
public:
    static std::vector<Game> scan(const Platform& platform);
};

}  // namespace nascade
