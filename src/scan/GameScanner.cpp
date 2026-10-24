#include "scan/GameScanner.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "core/Log.h"
#include "core/Util.h"
#include "scan/RomFiles.h"

namespace nascade {

namespace {

bool isCoverExtension(const std::string& ext) {
    return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" || ext == "bmp";
}

std::string findDirectCover(const std::string& imgPath, const std::string& base) {
    static const char* kExtensions[] = {"png", "jpg", "jpeg", "webp", "bmp",
                                        "PNG", "JPG", "JPEG", "WEBP", "BMP"};
    for (const char* ext : kExtensions) {
        const std::string path = util::joinPath(imgPath, base + "." + ext);
        if (util::fileExists(path)) return path;
    }
    return {};
}

// 去掉 (USA)、[简]、(Rev 1) 这类标签，并忽略空格和常见标点，
// 让带区域/汉化/版本后缀的 ROM 仍能匹配到原厂 Imgs 里的干净标题图。
std::string stripBracketTags(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (size_t i = 0; i < value.size();) {
        bool open = false;
        size_t openLength = 0;
        if (value[i] == '(' || value[i] == '[') {
            open = true;
            openLength = 1;
        } else if (value.compare(i, 3, "（") == 0 || value.compare(i, 3, "【") == 0) {
            open = true;
            openLength = 3;
        }
        if (!open) {
            out.push_back(value[i]);
            ++i;
            continue;
        }

        i += openLength;
        while (i < value.size()) {
            if (value[i] == ')' || value[i] == ']') {
                ++i;
                break;
            }
            if (value.compare(i, 3, "）") == 0 || value.compare(i, 3, "】") == 0) {
                i += 3;
                break;
            }
            ++i;
        }
    }
    return out;
}

std::string normalizeCoverName(const std::string& value) {
    const std::string stripped = stripBracketTags(value);
    std::string out;
    out.reserve(stripped.size());
    for (size_t i = 0; i < stripped.size();) {
        const unsigned char c = static_cast<unsigned char>(stripped[i]);
        if (c < 0x80) {
            if (std::isalnum(c)) {
                out.push_back(static_cast<char>(std::tolower(c)));
            } else if (c == '&' || c == '+' || c == '#') {
                out.push_back(static_cast<char>(c));
            }
            ++i;
            continue;
        }

        // 常见全角标点统一忽略或转换为 ASCII，避免同一标题因全/半角不同而匹配失败。
        if (stripped.compare(i, 3, "＆") == 0) {
            out.push_back('&');
            i += 3;
        } else if (stripped.compare(i, 3, "＋") == 0) {
            out.push_back('+');
            i += 3;
        } else if (stripped.compare(i, 3, "＃") == 0) {
            out.push_back('#');
            i += 3;
        } else if (stripped.compare(i, 3, "：") == 0 || stripped.compare(i, 3, "　") == 0) {
            i += 3;
        } else {
            out.push_back(stripped[i]);
            ++i;
        }
    }
    return out;
}

}  // namespace

CoverResolver::CoverResolver(std::string imgPath) : imgPath_(std::move(imgPath)) {}

std::string CoverResolver::find(const std::string& romBase) {
    if (imgPath_.empty()) return {};

    buildIndex();
    const auto exact = exact_.find(util::toLower(romBase));
    if (exact != exact_.end()) return exact->second;

    const auto it = index_.find(normalizeCoverName(romBase));
    return it != index_.end() ? it->second : std::string();
}

std::string CoverResolver::findDirect(const std::string& romBase) {
    return findDirectCover(imgPath_, romBase);
}

void CoverResolver::buildIndex() {
    if (indexBuilt_ || imgPath_.empty()) return;
    indexBuilt_ = true;

    const auto extRank = [](const std::string& path) {
        const std::string ext = util::extensionLower(path);
        if (ext == "png") return 5;
        if (ext == "jpg") return 4;
        if (ext == "jpeg") return 3;
        if (ext == "webp") return 2;
        return 1;  // bmp
    };

    std::unordered_set<std::string> duplicateKeys;
    for (const auto& cover : util::listDir(imgPath_)) {
        if (cover.isDir || cover.name.empty() || cover.name.front() == '.') continue;
        if (!isCoverExtension(util::extensionLower(cover.name))) continue;

        const std::string base = util::baseNameNoExt(cover.name);
        const std::string path = util::joinPath(imgPath_, cover.name);
        const std::string lowerBase = util::toLower(base);
        const auto existing = exact_.find(lowerBase);
        if (existing == exact_.end() || extRank(path) > extRank(existing->second)) {
            exact_[lowerBase] = path;
        }

        const std::string key = normalizeCoverName(base);
        if (key.size() < 3) continue;
        if (index_.count(key) > 0) {
            duplicateKeys.insert(key);
        } else {
            index_.emplace(key, path);
        }
    }
    // 同名归一化结果有歧义时不猜，避免把《龙珠 I&II》匹配到《龙珠 III》。
    for (const auto& key : duplicateKeys) index_.erase(key);
}

std::vector<Game> GameScanner::scan(const Platform& platform) {
    std::vector<Game> games;

    const std::set<std::string> extensions(platform.extensions.begin(), platform.extensions.end());
    const auto files = filterDiscCompanions(
        listRomFiles(platform.romPath, extensions,
                     platformAllowsExtensionless(platform.id)));

    CoverResolver covers(platform.imgPath);

    for (const auto& file : files) {
        const std::string ext = util::extensionLower(file.name);
        const std::string base = util::baseNameNoExt(file.name);

        Game game;
        game.name = base;
        game.romName = base;
        game.platformId = platform.id;
        game.path = util::joinPath(platform.romPath, file.relativePath);
        game.localPath = game.path;
        game.format = ext;
        game.size = file.size;
        game.mtime = file.mtime;
        game.installed = true;
        game.remote = false;
        game.coverPath = covers.find(base);

        games.push_back(std::move(game));
    }

    std::sort(games.begin(), games.end(), [](const Game& a, const Game& b) {
        return util::toLower(a.name) < util::toLower(b.name);
    });

    Log::instance().info("game scan " + platform.id + ": " + std::to_string(games.size()) + " games");
    return games;
}

}  // namespace nascade
