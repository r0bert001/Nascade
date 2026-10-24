#include "scrape/LibretroScraper.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <sys/stat.h>
#include <tuple>
#include <utility>
#include <vector>

#include <zlib.h>

#include "core/Log.h"
#include "core/Util.h"
#include "scrape/HttpClient.h"

namespace nascade {

namespace {

constexpr size_t kMaxRomPrefix = 128 * 1024;
constexpr time_t kCacheTtlSeconds = 7 * 24 * 60 * 60;

std::string lowerAscii(const std::string& value) {
    std::string out = value;
    for (char& c : out) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u < 0x80) c = static_cast<char>(std::tolower(u));
    }
    return out;
}

std::string normalizeMatch(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (const unsigned char c : value) {
        if (c < 0x80 && std::isalnum(c)) {
            out.push_back(static_cast<char>(std::tolower(c)));
        }
    }
    return out;
}

std::string platformKey(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (const unsigned char c : value) {
        if (c < 0x80 && std::isalnum(c)) {
            out.push_back(static_cast<char>(std::tolower(c)));
        }
    }
    return out;
}

std::string trimCopy(const std::string& value) {
    size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) ++begin;
    size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    return value.substr(begin, end - begin);
}

std::string coverStem(const std::string& name) {
    const size_t paren = name.find('(');
    std::string stem = paren == std::string::npos ? name : name.substr(0, paren);
    return trimCopy(stem);
}

std::string romStem(const std::string& name) {
    size_t cut = name.find('(');
    const size_t bracket = name.find('[');
    if (cut == std::string::npos) cut = bracket;
    else if (bracket != std::string::npos) cut = std::min(cut, bracket);
    return trimCopy(cut == std::string::npos ? name : name.substr(0, cut));
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string urlDecode(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            const int hi = hexValue(value[i + 1]);
            const int lo = hexValue(value[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        out.push_back(value[i]);
    }
    return out;
}

std::vector<std::string> parseCoverNames(const std::string& html) {
    std::vector<std::string> names;
    const std::string needle = "href=\"";
    size_t pos = 0;
    while ((pos = html.find(needle, pos)) != std::string::npos) {
        pos += needle.size();
        const size_t end = html.find('"', pos);
        if (end == std::string::npos) break;
        const std::string link = html.substr(pos, end - pos);
        pos = end + 1;

        if (link.find('?') != std::string::npos) continue;
        if (link.find('/') != std::string::npos) continue;
        if (!util::endsWith(lowerAscii(link), ".png")) continue;

        const std::string name = util::htmlUnescape(urlDecode(link));
        if (!name.empty()) names.push_back(name);
    }
    return names;
}

std::string libretroSystemDir(const std::string& platform) {
    static const std::map<std::string, std::string> dirs = {
        {"fc", "Nintendo - Nintendo Entertainment System"},
        {"nes", "Nintendo - Nintendo Entertainment System"},
        {"fds", "Nintendo - Family Computer Disk System"},
        {"sfc", "Nintendo - Super Nintendo Entertainment System"},
        {"snes", "Nintendo - Super Nintendo Entertainment System"},
        {"gb", "Nintendo - Game Boy"},
        {"gbc", "Nintendo - Game Boy Color"},
        {"gba", "Nintendo - Game Boy Advance"},
        {"nds", "Nintendo - Nintendo DS"},
        {"n64", "Nintendo - Nintendo 64"},
        {"md", "Sega - Mega Drive - Genesis"},
        {"genesis", "Sega - Mega Drive - Genesis"},
        {"sms", "Sega - Master System - Mark III"},
        {"ms", "Sega - Master System - Mark III"},
        {"gg", "Sega - Game Gear"},
        {"dc", "Sega - Dreamcast"},
        {"ss", "Sega - Saturn"},
        {"saturn", "Sega - Saturn"},
        {"ps", "Sony - PlayStation"},
        {"psx", "Sony - PlayStation"},
        {"playstation", "Sony - PlayStation"},
        {"psp", "Sony - PlayStation Portable"},
        {"ppsspp", "Sony - PlayStation Portable"},
        {"pce", "NEC - PC Engine - TurboGrafx 16"},
        {"pcengine", "NEC - PC Engine - TurboGrafx 16"},
        {"wsc", "Bandai - WonderSwan Color"},
        {"ws", "Bandai - WonderSwan"},
        {"ngp", "SNK - Neo Geo Pocket"},
        {"ngpc", "SNK - Neo Geo Pocket Color"},
        {"neogeo", "SNK - Neo Geo"},
        {"arcade", "MAME"},
        {"mame", "MAME"},
        {"mame2003plus", "MAME"},
        {"mame2010", "MAME"},
        {"fbneo", "FBNeo - Arcade Games"},
        {"cps1", "FBNeo - Arcade Games"},
        {"cps2", "FBNeo - Arcade Games"},
        {"cps3", "FBNeo - Arcade Games"},
        {"pgm", "FBNeo - Arcade Games"},
        {"atari2600", "Atari - 2600"},
        {"atari7800", "Atari - 7800"},
        {"lynx", "Atari - Lynx"},
        {"gw", "Handheld Electronic Game"},
    };
    const auto it = dirs.find(platformKey(platform));
    return it == dirs.end() ? "" : it->second;
}

std::string cleanAscii(const std::string& data, size_t offset, size_t length) {
    if (offset >= data.size()) return {};
    const size_t end = std::min(data.size(), offset + length);
    std::string out;
    out.reserve(end - offset);
    for (size_t i = offset; i < end; ++i) {
        const unsigned char c = static_cast<unsigned char>(data[i]);
        if (c == 0 || c < 0x20 || c > 0x7E) break;
        out.push_back(static_cast<char>(c));
    }
    return trimCopy(out);
}

std::string internalTitle(const std::string& platform, const std::string& data) {
    const std::string key = platformKey(platform);
    if (key == "gb" || key == "gbc" || key == "gg") {
        return cleanAscii(data, 0x134, 16);
    }
    if (key == "gba") {
        return cleanAscii(data, 0xA0, 12);
    }
    if (key == "sfc" || key == "snes") {
        const size_t offsets[] = {0x7FC0, 0xFFC0, 0x7FC0 + 512, 0xFFC0 + 512};
        for (const size_t offset : offsets) {
            const std::string title = cleanAscii(data, offset, 21);
            if (title.size() >= 3) return title;
        }
        return {};
    }
    if (key == "md" || key == "genesis") {
        std::string title = cleanAscii(data, 0x150, 48);
        if (title.empty()) title = cleanAscii(data, 0x120, 48);
        return title;
    }
    if (key == "n64") {
        return cleanAscii(data, 0x20, 20);
    }
    return {};
}

uint16_t read16(const unsigned char* data) {
    return static_cast<uint16_t>(data[0] | (data[1] << 8));
}

uint32_t read32(const unsigned char* data) {
    return static_cast<uint32_t>(data[0] | (data[1] << 8) | (data[2] << 16) |
                                 (static_cast<uint32_t>(data[3]) << 24));
}

bool hasRomExtension(const std::string& name) {
    static const std::set<std::string> extensions = {
        "gb", "gbc", "gba", "nes", "fds", "unf", "sfc", "smc", "fig",
        "md", "gen", "smd", "bin", "n64", "z64", "v64", "pce", "gg",
        "sms", "32x", "iso",
    };
    const std::string ext = lowerAscii(util::extensionLower(name));
    return extensions.count(ext) > 0;
}

bool readFilePrefix(const std::string& path, size_t maxBytes, std::string& out,
                    std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "无法读取 ROM：" + path;
        return false;
    }
    out.resize(maxBytes);
    file.read(&out[0], static_cast<std::streamsize>(maxBytes));
    out.resize(static_cast<size_t>(file.gcount()));
    return true;
}

// 从 zip 的第一个 ROM 条目中最多解压 maxBytes，足够读取卡带头部。
bool readZipPrefix(const std::string& path, size_t maxBytes, std::string& out,
                   std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "无法读取压缩包：" + path;
        return false;
    }
    file.seekg(0, std::ios::end);
    const std::streamoff fileSize = file.tellg();
    if (fileSize < 22) {
        error = "压缩包为空";
        return false;
    }

    const size_t tailSize = static_cast<size_t>(std::min<std::streamoff>(fileSize, 65557));
    std::vector<unsigned char> tail(tailSize);
    file.seekg(fileSize - static_cast<std::streamoff>(tailSize));
    file.read(reinterpret_cast<char*>(tail.data()), static_cast<std::streamsize>(tailSize));
    if (file.gcount() != static_cast<std::streamsize>(tailSize)) {
        error = "读取 zip 尾部失败";
        return false;
    }

    int eocd = -1;
    for (int i = static_cast<int>(tail.size()) - 22; i >= 0; --i) {
        if (read32(tail.data() + i) == 0x06054b50) {
            eocd = i;
            break;
        }
    }
    if (eocd < 0) {
        error = "zip 缺少中央目录";
        return false;
    }

    const uint32_t cdSize = read32(tail.data() + eocd + 12);
    const uint32_t cdOffset = read32(tail.data() + eocd + 16);
    if (cdOffset == 0xFFFFFFFF || cdSize == 0xFFFFFFFF || cdSize > 32 * 1024 * 1024) {
        error = "不支持 zip64 或压缩包过大";
        return false;
    }

    std::vector<unsigned char> central(cdSize);
    file.seekg(static_cast<std::streamoff>(cdOffset));
    file.read(reinterpret_cast<char*>(central.data()), static_cast<std::streamsize>(cdSize));
    if (file.gcount() != static_cast<std::streamsize>(cdSize)) {
        error = "读取 zip 中央目录失败";
        return false;
    }

    size_t pos = 0;
    bool found = false;
    uint16_t method = 0;
    uint32_t compressedSize = 0;
    uint32_t localOffset = 0;
    std::string entryName;

    while (pos + 46 <= central.size()) {
        const unsigned char* header = central.data() + pos;
        if (read32(header) != 0x02014b50) break;

        const uint16_t methodValue = read16(header + 10);
        const uint32_t compressed = read32(header + 20);
        const uint16_t nameLength = read16(header + 28);
        const uint16_t extraLength = read16(header + 30);
        const uint16_t commentLength = read16(header + 32);
        const uint32_t local = read32(header + 42);
        if (pos + 46 + nameLength > central.size()) break;

        const std::string name(reinterpret_cast<const char*>(header + 46), nameLength);
        if (!name.empty() && name.back() != '/' && hasRomExtension(name)) {
            found = true;
            method = methodValue;
            compressedSize = compressed;
            localOffset = local;
            entryName = name;
            break;
        }
        pos += 46 + nameLength + extraLength + commentLength;
    }

    if (!found) {
        error = "zip 内没有可识别的 ROM";
        return false;
    }

    if (static_cast<uint64_t>(localOffset) + 30 > static_cast<uint64_t>(fileSize)) {
        error = "zip 本地头偏移无效";
        return false;
    }

    unsigned char localHeader[30];
    file.seekg(static_cast<std::streamoff>(localOffset));
    file.read(reinterpret_cast<char*>(localHeader), sizeof(localHeader));
    if (file.gcount() != static_cast<std::streamsize>(sizeof(localHeader)) ||
        read32(localHeader) != 0x04034b50) {
        error = "zip 本地头无效";
        return false;
    }
    const uint16_t localNameLength = read16(localHeader + 26);
    const uint16_t localExtraLength = read16(localHeader + 28);
    const std::streamoff dataOffset = static_cast<std::streamoff>(localOffset) + 30 +
                                      localNameLength + localExtraLength;

    if (method == 0) {
        const size_t toRead = std::min<size_t>(maxBytes, compressedSize);
        out.resize(toRead);
        file.seekg(dataOffset);
        if (toRead > 0) {
            file.read(&out[0], static_cast<std::streamsize>(toRead));
            out.resize(static_cast<size_t>(file.gcount()));
        }
        return true;
    }
    if (method != 8) {
        error = "不支持的 zip 压缩方式";
        return false;
    }

    z_stream stream {};
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
        error = "zlib 初始化失败";
        return false;
    }

    file.seekg(dataOffset);
    const uint32_t compressedLimit = std::min<uint32_t>(
        compressedSize, static_cast<uint32_t>(16 * 1024 * 1024));
    uint64_t consumed = 0;
    bool done = false;
    std::vector<unsigned char> input(64 * 1024);
    out.clear();
    out.reserve(maxBytes);

    while (!done && consumed < compressedLimit) {
        const size_t want = static_cast<size_t>(
            std::min<uint64_t>(input.size(), compressedLimit - consumed));
        file.read(reinterpret_cast<char*>(input.data()), static_cast<std::streamsize>(want));
        const size_t got = static_cast<size_t>(file.gcount());
        if (got == 0) break;
        consumed += got;

        stream.next_in = input.data();
        stream.avail_in = static_cast<uInt>(got);
        while (stream.avail_in > 0 && out.size() < maxBytes) {
            unsigned char chunk[32 * 1024];
            stream.next_out = chunk;
            stream.avail_out = sizeof(chunk);
            const int result = inflate(&stream, Z_NO_FLUSH);
            const size_t produced = sizeof(chunk) - stream.avail_out;
            if (produced > 0) {
                const size_t remaining = maxBytes - out.size();
                out.append(reinterpret_cast<const char*>(chunk), std::min(produced, remaining));
            }
            if (result == Z_STREAM_END) {
                done = true;
                break;
            }
            if (result != Z_OK && result != Z_BUF_ERROR) {
                inflateEnd(&stream);
                error = "zip 解压失败";
                return false;
            }
            if (produced == 0) break;
        }
        if (out.size() >= maxBytes) done = true;
    }

    inflateEnd(&stream);
    if (out.empty()) {
        error = "zip 条目为空";
        return false;
    }
    Log::instance().info("libretro zip entry: " + entryName);
    return true;
}

bool readRomPrefix(const Game& game, size_t maxBytes, std::string& out, std::string& error) {
    std::string path;
    if (!game.localPath.empty() && util::fileExists(game.localPath)) {
        path = game.localPath;
    } else if (!game.remote && !game.path.empty() && util::fileExists(game.path)) {
        path = game.path;
    }
    if (path.empty()) {
        // NAS 游戏不在本地，内部标题不可读；由文件名回退。
        return true;
    }
    if (util::endsWith(lowerAscii(path), ".zip")) {
        return readZipPrefix(path, maxBytes, out, error);
    }
    return readFilePrefix(path, maxBytes, out, error);
}

int levenshteinDistance(const std::string& a, const std::string& b) {
    if (a.empty()) return static_cast<int>(b.size());
    if (b.empty()) return static_cast<int>(a.size());
    std::vector<int> previous(b.size() + 1);
    std::vector<int> current(b.size() + 1);
    for (size_t j = 0; j <= b.size(); ++j) previous[j] = static_cast<int>(j);
    for (size_t i = 1; i <= a.size(); ++i) {
        current[0] = static_cast<int>(i);
        for (size_t j = 1; j <= b.size(); ++j) {
            const int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            current[j] = std::min({previous[j] + 1, current[j - 1] + 1,
                                   previous[j - 1] + cost});
        }
        std::swap(previous, current);
    }
    return previous[b.size()];
}

double similarity(const std::string& a, const std::string& b) {
    if (a.empty() || b.empty()) return 0.0;
    const size_t maxLen = std::max(a.size(), b.size());
    return 1.0 - static_cast<double>(levenshteinDistance(a, b)) / static_cast<double>(maxLen);
}

const std::map<std::string, std::vector<std::string>>& aliases() {
    static const std::map<std::string, std::vector<std::string>> map = {
        {"pmcrystal", {"pokemoncrystal"}},
        {"pkcrystal", {"pokemoncrystal"}},
        {"lufiaretur", {"lufiathelegendreturns"}},
        {"zeldadin", {"oracleofages"}},
        {"zeldanayru", {"oracleofseasons"}},
        {"zeldalinks", {"linksawakening"}},
        {"topnarikiri", {"narikiridungeon"}},
        {"koro2", {"korokoro"}},
        {"kdldx", {"kirbysdreamlanddx"}},
        {"kdld", {"kirbysdreamland"}},
        {"bmaxhikari", {"hikarionoyuusha"}},
        {"bmax", {"bombermanmax"}},
        {"hmoon2", {"harvestmoon2"}},
        {"bokumonogb3", {"harvestmoon3"}},
        {"csummoner", {"cardsummoner"}},
        {"lastbible1", {"lastbible"}},
        {"lastbible2", {"lastbible2"}},
        {"debitiru", {"devilchildren"}},
        {"sirengb2", {"sirengb2"}},
        {"shiren2", {"sirengb2"}},
        {"metroid2", {"metroidiireturnofsamus"}},
        {"sirrengb2", {"sirengb2"}},
        {"heroesm", {"heroesofmightandmagic"}},
        {"ghostsn", {"ghostsn"}},
        {"boku", {"harvestmoon", "bokujou"}},
        {"gargoyle", {"gargoylesquest"}},
    };
    return map;
}

int regionRank(const std::string& cover, const std::string& configuredRegion) {
    const size_t begin = cover.find('(');
    const size_t end = cover.find(')', begin == std::string::npos ? 0 : begin);
    if (begin == std::string::npos || end == std::string::npos) return 99;
    const std::string region = lowerAscii(cover.substr(begin + 1, end - begin - 1));

    std::vector<std::string> order;
    const std::string preferred = lowerAscii(configuredRegion);
    if (preferred == "jp") {
        order = {"japan", "world", "usa", "europe"};
    } else if (preferred == "eu") {
        order = {"europe", "usa", "japan", "world"};
    } else if (preferred == "cn") {
        // libretro 没有中国大陆版本，默认按美版/欧版优先，避免优先选到日版。
        order = {"usa", "europe", "japan", "world"};
    } else {
        order = {"usa", "europe", "japan", "world"};
    }
    for (size_t i = 0; i < order.size(); ++i) {
        if (region.find(order[i]) != std::string::npos) return static_cast<int>(i);
    }
    return 50;
}

int nonRetailRank(const std::string& cover) {
    const std::string low = lowerAscii(cover);
    static const char* kMarkers[] = {
        "[hack", "[sample", "[beta", "[demo", "[proto", "(unl", "[t-en",
        "[t+", "[a]", "[h]", "[b]", "(promo", "(kiosk", "(virtual console",
        "(wii", "(3ds", "(nintendo switch", "(switch", "(classic mini",
    };
    for (const char* marker : kMarkers) {
        if (low.find(marker) != std::string::npos) return 1;
    }
    return 0;
}

int tagCount(const std::string& cover) {
    return static_cast<int>(std::count(cover.begin(), cover.end(), '('));
}

struct CoverCandidate {
    std::string name;
    double ratio = 0.0;
};

std::string chooseBest(const std::vector<CoverCandidate>& candidates,
                       const std::string& configuredRegion) {
    const CoverCandidate* best = nullptr;
    for (const auto& candidate : candidates) {
        if (best == nullptr) {
            best = &candidate;
            continue;
        }
        const auto bestKey = std::make_tuple(
            -best->ratio, regionRank(best->name, configuredRegion),
            nonRetailRank(best->name), tagCount(best->name));
        const auto candidateKey = std::make_tuple(
            -candidate.ratio, regionRank(candidate.name, configuredRegion),
            nonRetailRank(candidate.name), tagCount(candidate.name));
        if (candidateKey < bestKey) best = &candidate;
    }
    return best ? best->name : std::string();
}

std::vector<CoverCandidate> matchCovers(const std::string& title,
                                        const std::string& fileName,
                                        const std::vector<std::string>& covers) {
    const std::string titleNorm = normalizeMatch(title);
    const std::string fileNorm = normalizeMatch(romStem(fileName));
    std::vector<std::string> aliasKeys;
    if (!titleNorm.empty()) {
        for (const auto& item : aliases()) {
            if (titleNorm.find(item.first) != std::string::npos) aliasKeys.push_back(item.first);
        }
    }

    std::vector<CoverCandidate> hits;
    for (const auto& cover : covers) {
        const std::string stem = normalizeMatch(coverStem(cover));
        if (stem.empty()) continue;

        bool ok = false;
        double ratio = 0.0;
        if (!titleNorm.empty()) {
            ratio = similarity(titleNorm, stem);
            if (stem == titleNorm) {
                ok = true;
            } else if (titleNorm.size() >= 5 &&
                       (stem.find(titleNorm) != std::string::npos ||
                        (stem.size() >= 4 &&
                         titleNorm.find(stem) != std::string::npos))) {
                ok = true;
            } else if (titleNorm.size() >= 6 && stem.size() >= 6 &&
                       titleNorm.substr(0, 6) == stem.substr(0, 6) && ratio >= 0.6) {
                ok = true;
            } else if (titleNorm.size() >= 6 && ratio >= 0.85) {
                ok = true;
            }
            if (!ok && !aliasKeys.empty()) {
                for (const auto& key : aliasKeys) {
                    const auto& keywords = aliases().at(key);
                    bool all = true;
                    for (const auto& keyword : keywords) {
                        if (stem.find(keyword) == std::string::npos) {
                            all = false;
                            break;
                        }
                    }
                    if (all) {
                        ok = true;
                        break;
                    }
                }
            }
        }
        if (ok) hits.push_back(CoverCandidate{cover, ratio});
    }

    if (hits.empty() && !fileNorm.empty()) {
        for (const auto& cover : covers) {
            const std::string stem = normalizeMatch(coverStem(cover));
            if (stem.empty()) continue;
            if (stem == fileNorm ||
                (fileNorm.size() >= 5 &&
                 (stem.find(fileNorm) != std::string::npos ||
                  fileNorm.find(stem) != std::string::npos))) {
                hits.push_back(CoverCandidate{cover, similarity(fileNorm, stem)});
            }
        }
    }
    return hits;
}

bool cacheIsFresh(const std::string& path) {
    struct stat st {};
    if (::stat(path.c_str(), &st) != 0) return false;
    return std::time(nullptr) - st.st_mtime < kCacheTtlSeconds;
}

std::vector<std::string> readCachedNames(const std::string& path) {
    std::string text;
    if (!util::readFile(path, text) || text.empty()) return {};
    std::vector<std::string> names;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) names.push_back(line);
    }
    return names;
}

std::string cacheFileFor(const std::string& cacheDir, const std::string& platform) {
    const std::string key = platformKey(platform);
    return util::joinPath(util::joinPath(cacheDir, "libretro"),
                          (key.empty() ? std::string("unknown") : key) + "_boxarts.txt");
}

std::string baseUrl(const ScraperConfig& config) {
    std::string url = config.baseUrl;
    if (url.empty() || lowerAscii(url).find("screenscraper") != std::string::npos) {
        url = "https://thumbnails.libretro.com";
    }
    while (!url.empty() && url.back() == '/') url.pop_back();
    return url;
}

}  // namespace

LibretroScraper::LibretroScraper(ScraperConfig config, std::string cacheDir)
    : config_(std::move(config)), cacheDir_(std::move(cacheDir)) {}

ScraperTestResult LibretroScraper::test() {
    ScraperTestResult result;
    const std::string url = baseUrl(config_) + "/";
    const HttpResponse response = HttpClient::get(
        url, {"Accept: text/html", "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8"}, 30);

    if (!response.ok) {
        if (!response.error.empty()) {
            result.message = "连接 libretro 缩略图失败：" + response.error;
        } else {
            result.message = "连接 libretro 缩略图失败：HTTP " +
                             std::to_string(response.status);
        }
        Log::instance().warn("libretro test failed: " + result.message);
        return result;
    }
    if (response.body.find("Index of") == std::string::npos) {
        result.message = "libretro 返回了非目录页面，请检查 baseUrl。";
        return result;
    }

    result.ok = true;
    result.platformCount = 1;
    result.message = "连接成功，libretro 缩略图可访问。";
    return result;
}

bool LibretroScraper::fetch(const Game& game, ScrapedGame& out, std::string& error) {
    const std::string systemDir = libretroSystemDir(game.platformId);
    if (systemDir.empty()) {
        error = "libretro 没有该平台的封面目录：" + game.platformId;
        return false;
    }

    std::string romData;
    std::string readError;
    std::string internal;
    if (readRomPrefix(game, kMaxRomPrefix, romData, readError)) {
        internal = internalTitle(game.platformId, romData);
    } else {
        Log::instance().warn("libretro ROM read failed: " + readError);
    }
    const std::string fileName = game.romName.empty()
        ? util::baseNameNoExt(game.path)
        : game.romName;

    if (!internal.empty()) {
        Log::instance().info("libretro internal title: " + internal);
    }

    const std::string cachePath = cacheFileFor(cacheDir_, game.platformId);
    std::vector<std::string> covers;
    if (cacheIsFresh(cachePath)) {
        covers = readCachedNames(cachePath);
    }

    if (covers.empty()) {
        const std::string listUrl = baseUrl(config_) + "/" +
                                    HttpClient::urlEncode(systemDir) + "/Named_Boxarts/";
        const HttpResponse response = HttpClient::get(
            listUrl, {"Accept: text/html", "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8"}, 180);
        if (response.ok) {
            covers = parseCoverNames(response.body);
            if (!covers.empty()) {
                util::ensureDir(util::parentPath(cachePath));
                std::string cacheText;
                for (const auto& name : covers) {
                    cacheText += name;
                    cacheText += '\n';
                }
                if (!util::writeFile(cachePath, cacheText)) {
                    Log::instance().warn("libretro cache write failed: " + cachePath);
                }
            }
        } else {
            Log::instance().warn("libretro list failed: " + listUrl + " (" +
                                 (response.error.empty()
                                      ? "HTTP " + std::to_string(response.status)
                                      : response.error) +
                                 ")");
        }

        if (covers.empty()) {
            // 网络失败时允许使用过期缓存。
            covers = readCachedNames(cachePath);
        }
    }

    if (covers.empty()) {
        error = "无法获取 libretro 封面列表：" + systemDir;
        return false;
    }

    auto hits = matchCovers(internal, fileName, covers);
    if (hits.empty()) {
        error = internal.empty()
            ? "libretro 未找到匹配封面（文件名可能不是英文）"
            : "libretro 未找到匹配封面（内部标题：" + internal + "）";
        return false;
    }

    const std::string chosen = chooseBest(hits, config_.region);
    if (chosen.empty()) {
        error = "libretro 匹配结果为空";
        return false;
    }

    out = ScrapedGame{};
    out.providerGameId = chosen;
    out.title = game.name;
    out.coverUrl = baseUrl(config_) + "/" + HttpClient::urlEncode(systemDir) +
                   "/Named_Boxarts/" + HttpClient::urlEncode(chosen);

    Log::instance().info("libretro matched: " + chosen +
                         " (internal=" + (internal.empty() ? "-" : internal) +
                         ", file=" + fileName + ", candidates=" +
                         std::to_string(hits.size()) + ")");
    return true;
}

}  // namespace nascade
