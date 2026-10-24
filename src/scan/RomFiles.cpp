#include "scan/RomFiles.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <utility>

#include "core/Util.h"

namespace nascade {

namespace {

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

bool isDocName(const std::string& lowerName) {
    static const std::set<std::string> names = {
        "copying", "license", "licence", "readme", "changelog", "changes",
        "authors", "contributors", "gpl", "notice", "todo", "install",
        "makefile", "manifest", "version",
    };
    if (names.count(lowerName) > 0) return true;
    return lowerName.rfind("readme", 0) == 0;
}

bool skipDir(const std::string& lowerName) {
    return lowerName == "system volume information" || lowerName == "$recycle.bin" ||
           lowerName == ".git" || lowerName == ".svn";
}

void walk(const std::string& dir, const std::string& relative, int depth, int maxDepth,
          const std::set<std::string>& extensions, bool includeExtensionless,
          std::vector<RomFileEntry>& out) {
    for (const auto& entry : util::listDir(dir)) {
        if (entry.name.empty() || entry.name.front() == '.') continue;
        const std::string lowerName = util::toLower(entry.name);
        const std::string childRelative = relative.empty()
            ? entry.name
            : util::joinPath(relative, entry.name);

        if (entry.isDir) {
            if (depth < maxDepth && !skipDir(lowerName)) {
                walk(util::joinPath(dir, entry.name), childRelative, depth + 1, maxDepth,
                     extensions, includeExtensionless, out);
            }
            continue;
        }

        const std::string ext = util::extensionLower(entry.name);
        if (ext == "db" || ext == "part") continue;
        bool accepted = extensions.count(ext) > 0;
        if (!accepted && ext.empty() && includeExtensionless && !isDocName(lowerName)) {
            accepted = true;
        }
        if (!accepted) continue;

        RomFileEntry file;
        file.relativePath = childRelative;
        file.name = entry.name;
        file.size = entry.size;
        file.mtime = entry.mtime;
        out.push_back(std::move(file));
    }
}

}  // namespace

std::vector<std::string> augmentPlatformExtensions(
    const std::string& platformId, const std::vector<std::string>& configured) {
    static const std::map<std::string, std::vector<std::string>> extras = {
        {"atari2600", {"7z", "a26", "bin", "rom"}},
        {"atari7800", {"7z", "a78", "bin", "rom"}},
        {"lynx", {"7z", "lnx"}},
        {"fc", {"7z"}},
        {"nes", {"7z"}},
        {"sfc", {"7z", "smd"}},
        {"snes", {"7z", "smd"}},
        {"gb", {"7z"}},
        {"gbc", {"7z"}},
        {"gba", {"7z"}},
        {"md", {"7z"}},
        {"genesis", {"7z"}},
        {"gg", {"7z", "gg", "bin"}},
        {"ms", {"7z"}},
        {"n64", {"7z", "rom", "ndd"}},
        {"nds", {"nds", "srl", "dsi", "7z"}},
        {"pce", {"7z", "iso", "chd"}},
        {"ps", {"7z", "iso"}},
        {"psx", {"7z", "iso"}},
        {"psp", {"7z", "chd", "zip"}},
        {"ppsspp", {"7z", "chd", "zip", "iso", "cso", "pbp"}},
        {"dc", {"7z", "zip", "cue", "bin", "iso"}},
        {"ss", {"7z", "cue", "bin", "iso", "chd", "m3u"}},
        {"saturn", {"7z", "cue", "bin", "iso", "chd", "m3u"}},
        {"ws", {"7z", "ws", "wsc"}},
        {"wsc", {"7z", "ws", "wsc"}},
        {"ngp", {"7z"}},
        {"ngpc", {"7z"}},
        {"neogeo", {"7z"}},
        {"arcade", {"7z"}},
        {"mame", {"7z"}},
        {"mame2003plus", {"7z"}},
        {"mame2010", {"7z"}},
        {"fbneo", {"7z"}},
        {"cps1", {"7z"}},
        {"cps2", {"7z"}},
        {"cps3", {"7z"}},
        {"pgm", {"7z"}},
        {"bbk", {"7z"}},
        {"pico8", {"p8", "png"}},
        {"openbor", {"zip"}},
        {"java", {"zip"}},
    };

    std::vector<std::string> result = configured;
    const auto it = extras.find(platformKey(platformId));
    if (it == extras.end()) return result;

    std::set<std::string> seen;
    for (const auto& ext : result) seen.insert(util::toLower(ext));
    for (const auto& ext : it->second) {
        if (seen.insert(ext).second) result.push_back(ext);
    }
    return result;
}

bool platformAllowsExtensionless(const std::string& platformId) {
    const std::string key = platformKey(platformId);
    return key == "fc" || key == "nes" || key == "sfc" || key == "snes";
}

std::vector<RomFileEntry> listRomFiles(const std::string& root,
                                       const std::set<std::string>& extensions,
                                       bool includeExtensionless,
                                       int maxDepth) {
    std::vector<RomFileEntry> out;
    if (root.empty()) return out;
    walk(root, "", 0, std::max(0, maxDepth), extensions, includeExtensionless, out);
    std::sort(out.begin(), out.end(), [](const RomFileEntry& a, const RomFileEntry& b) {
        return util::toLower(a.relativePath) < util::toLower(b.relativePath);
    });
    return out;
}

std::vector<RomFileEntry> filterDiscCompanions(const std::vector<RomFileEntry>& files) {
    std::set<std::string> discBases;
    for (const auto& file : files) {
        const std::string ext = util::extensionLower(file.name);
        if (ext != "cue" && ext != "m3u" && ext != "gdi") continue;
        const std::string parent = util::parentPath(file.relativePath);
        const std::string base = util::toLower(util::baseNameNoExt(file.name));
        discBases.insert(parent + "\x1f" + base);
    }

    std::vector<RomFileEntry> out;
    out.reserve(files.size());
    for (const auto& file : files) {
        const std::string ext = util::extensionLower(file.name);
        if (ext == "bin" || ext == "iso") {
            const std::string parent = util::parentPath(file.relativePath);
            const std::string base = util::toLower(util::baseNameNoExt(file.name));
            if (discBases.count(parent + "\x1f" + base) > 0) continue;
        }
        out.push_back(file);
    }
    return out;
}

}  // namespace nascade
