#include "scrape/ScreenScraper.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/Log.h"
#include "core/Util.h"
#include "scrape/HttpClient.h"

namespace nascade {

using json = nlohmann::json;

namespace {

std::string textOf(const json& value) {
    if (value.is_string()) return value.get<std::string>();
    if (value.is_number_integer()) return std::to_string(value.get<long long>());
    if (value.is_number_unsigned()) return std::to_string(value.get<unsigned long long>());
    if (value.is_number_float()) return value.dump();
    if (value.is_object()) {
        if (value.contains("text")) return textOf(value["text"]);
        if (value.contains("nom")) return textOf(value["nom"]);
        if (value.contains("name")) return textOf(value["name"]);
    }
    return "";
}

std::string localized(const json& list, const std::string& language) {
    if (list.is_string()) return list.get<std::string>();
    if (list.is_object()) {
        if (list.contains("text")) return textOf(list["text"]);
        if (list.contains("nom")) return textOf(list["nom"]);
    }
    if (!list.is_array()) return "";

    const auto pick = [&](const std::string& lang) -> std::string {
        for (const auto& item : list) {
            if (!item.is_object()) continue;
            if (item.value("langue", std::string()) == lang) {
                const std::string value = textOf(item);
                if (!value.empty()) return value;
            }
        }
        return "";
    };

    std::string value = pick(language);
    if (value.empty() && language != "en") value = pick("en");
    if (value.empty()) {
        for (const auto& item : list) {
            value = textOf(item);
            if (!value.empty()) return value;
        }
    }
    return value;
}

std::string regionText(const json& dates, const std::string& preferredRegion) {
    if (dates.is_string()) return dates.get<std::string>();
    if (!dates.is_array()) return textOf(dates);

    const std::vector<std::string> regions = {preferredRegion, "cn", "jp", "us", "eu", "wor"};
    for (const auto& region : regions) {
        if (region.empty()) continue;
        for (const auto& item : dates) {
            if (!item.is_object()) continue;
            if (item.value("region", std::string()) == region) {
                const std::string value = textOf(item);
                if (!value.empty()) return value;
            }
        }
    }
    for (const auto& item : dates) {
        const std::string value = textOf(item);
        if (!value.empty()) return value;
    }
    return "";
}

std::string mediaUrl(const json& medias, const std::string& type,
                     const std::string& preferredRegion) {
    if (!medias.is_array()) return "";
    const std::vector<std::string> regions = {preferredRegion, "cn", "jp", "us", "eu", "wor"};
    for (const auto& region : regions) {
        if (region.empty()) continue;
        for (const auto& item : medias) {
            if (!item.is_object()) continue;
            if (item.value("type", std::string()) != type) continue;
            if (item.value("region", std::string()) != region) continue;
            const std::string url = item.value("url", std::string());
            if (!url.empty()) return url;
        }
    }
    for (const auto& item : medias) {
        if (!item.is_object()) continue;
        if (item.value("type", std::string()) == type) {
            const std::string url = item.value("url", std::string());
            if (!url.empty()) return url;
        }
    }
    return "";
}

std::string systemIdFor(const std::string& platform) {
    static const std::map<std::string, std::string> ids = {
        {"fc", "3"}, {"nes", "3"},
        {"sfc", "4"}, {"snes", "4"},
        {"gb", "9"}, {"gbc", "10"}, {"gba", "12"},
        {"md", "1"}, {"genesis", "1"},
        {"ps", "57"}, {"psx", "57"},
        {"n64", "14"}, {"pce", "8"},
        {"arcade", "75"}, {"psp", "61"}, {"dc", "23"},
    };
    const auto it = ids.find(util::toLower(platform));
    return it == ids.end() ? "" : it->second;
}

std::string joinWith(const std::vector<std::string>& values, const std::string& separator) {
    std::string out;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) out += separator;
        out += values[i];
    }
    return out;
}

int countSystems(const json& value) {
    if (value.is_array()) return static_cast<int>(value.size());
    if (value.is_object()) {
        for (auto it = value.begin(); it != value.end(); ++it) {
            if (it.key() == "systemes" || it.key() == "systeme") {
                if (it->is_array()) return static_cast<int>(it->size());
                if (it->is_object()) {
                    const auto nested = it->find("systeme");
                    if (nested != it->end()) return countSystems(*nested);
                }
            }
        }
        for (auto it = value.begin(); it != value.end(); ++it) {
            const int found = countSystems(*it);
            if (found > 0) return found;
        }
    }
    return 0;
}

std::string findErrorMessage(const json& value) {
    if (!value.is_object()) return "";
    for (auto it = value.begin(); it != value.end(); ++it) {
        const std::string key = util::toLower(it.key());
        if (key == "erreur" || key == "error") {
            const std::string direct = textOf(*it);
            if (!direct.empty()) return direct;
            const std::string nested = findErrorMessage(*it);
            if (!nested.empty()) return nested;
        }
        if (key == "message" && it->is_string()) {
            return it->get<std::string>();
        }
    }
    for (auto it = value.begin(); it != value.end(); ++it) {
        if (!it->is_structured()) continue;
        const std::string nested = findErrorMessage(*it);
        if (!nested.empty()) return nested;
    }
    return "";
}

}  // namespace

ScreenScraper::ScreenScraper(ScraperConfig config)
    : config_(std::move(config)) {}

std::string ScreenScraper::baseUrl() const {
    std::string url = config_.baseUrl.empty() ? "https://api.screenscraper.fr/api2" : config_.baseUrl;
    while (!url.empty() && url.back() == '/') url.pop_back();
    return url;
}

std::string ScreenScraper::authQuery() const {
    const std::string soft = config_.softName.empty() ? "Nascade" : config_.softName;
    std::string query = "devid=" + HttpClient::urlEncode(config_.devId) +
                        "&devpassword=" + HttpClient::urlEncode(config_.devPassword) +
                        "&softname=" + HttpClient::urlEncode(soft) +
                        "&output=json";
    if (!config_.ssId.empty()) {
        query += "&ssid=" + HttpClient::urlEncode(config_.ssId);
    }
    if (!config_.ssPassword.empty()) {
        query += "&sspassword=" + HttpClient::urlEncode(config_.ssPassword);
    }
    if (!config_.language.empty()) {
        query += "&langue=" + HttpClient::urlEncode(config_.language);
    }
    return query;
}

ScraperTestResult ScreenScraper::test() {
    ScraperTestResult result;

    if (config_.devId.empty() || config_.devPassword.empty()) {
        result.message = "请先填写 ScreenScraper 的 devid 和 devpassword。";
        return result;
    }

    const std::string url = baseUrl() + "/systemeListe.php?" + authQuery();
    const HttpResponse response = HttpClient::get(
        url, {"Accept: application/json", "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8"}, 25);

    if (!response.ok) {
        if (response.status == 401 || response.status == 403) {
            result.message = "认证失败，请检查 ScreenScraper 凭据。";
        } else if (response.status == 429 || response.status == 503) {
            result.message = "ScreenScraper 配额或限流，请稍后再试。";
        } else if (!response.error.empty()) {
            result.message = "连接失败：" + response.error;
        } else {
            result.message = "连接失败：HTTP " + std::to_string(response.status);
        }
        Log::instance().warn("ScreenScraper test failed: " + result.message);
        return result;
    }

    try {
        const json root = json::parse(response.body);
        const std::string apiError = findErrorMessage(root);
        const int systems = countSystems(root);

        if (systems <= 0) {
            result.message = apiError.empty()
                ? "ScreenScraper 未返回平台列表。"
                : "ScreenScraper 返回错误：" + util::htmlUnescape(apiError);
            return result;
        }

        result.ok = true;
        result.platformCount = systems;
        result.message = "连接成功，读取到 " + std::to_string(systems) + " 个平台。";
        return result;
    } catch (const std::exception& e) {
        result.message = std::string("响应解析失败：") + e.what();
        Log::instance().warn("ScreenScraper JSON parse failed: " +
                             util::redactSecrets(response.body.substr(0, 300)));
        return result;
    }
}

bool ScreenScraper::fetch(const Game& game, ScrapedGame& out, std::string& error) {
    if (config_.devId.empty() || config_.devPassword.empty()) {
        error = "未配置 ScreenScraper 凭据";
        return false;
    }

    const std::string fileName = game.path.empty() ? game.romName : util::baseName(game.path);
    if (fileName.empty()) {
        error = "游戏文件名为空";
        return false;
    }

    std::string url = baseUrl() + "/jeuInfos.php?" + authQuery();
    url += "&romnom=" + HttpClient::urlEncode(fileName);
    url += "&romtaille=" + std::to_string(game.size);
    const std::string systemId = systemIdFor(game.platformId);
    if (!systemId.empty()) {
        url += "&systemeid=" + HttpClient::urlEncode(systemId);
    }

    const HttpResponse response = HttpClient::get(
        url, {"Accept: application/json", "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8"}, 30);
    if (!response.ok) {
        if (response.status == 401 || response.status == 403) {
            error = "ScreenScraper 认证失败";
        } else if (response.status == 429 || response.status == 503) {
            error = "ScreenScraper 配额或限流";
        } else {
            error = "ScreenScraper 请求失败：" +
                    (response.error.empty() ? "HTTP " + std::to_string(response.status) : response.error);
        }
        return false;
    }

    try {
        const json root = json::parse(response.body);
        json gameJson;
        if (root.contains("response") && root["response"].is_object() &&
            root["response"].contains("jeu")) {
            gameJson = root["response"]["jeu"];
        } else if (root.contains("jeu")) {
            gameJson = root["jeu"];
        } else {
            const std::string apiError = findErrorMessage(root);
            error = apiError.empty() ? "ScreenScraper 未返回游戏数据"
                                     : "ScreenScraper 返回错误：" + util::htmlUnescape(apiError);
            return false;
        }

        out = ScrapedGame{};
        out.providerGameId = gameJson.contains("id") ? textOf(gameJson["id"]) : "";
        out.title = gameJson.contains("noms") ? localized(gameJson["noms"], config_.language) : "";
        if (out.title.empty() && gameJson.contains("nom")) out.title = textOf(gameJson["nom"]);
        if (out.title.empty()) out.title = game.name;

        out.description = gameJson.contains("synopsis")
            ? localized(gameJson["synopsis"], config_.language)
            : (gameJson.contains("description") ? textOf(gameJson["description"]) : "");
        out.developer = gameJson.contains("developpeur") ? textOf(gameJson["developpeur"]) : "";
        out.publisher = gameJson.contains("editeur") ? textOf(gameJson["editeur"]) : "";
        out.releaseDate = gameJson.contains("dates") ? regionText(gameJson["dates"], config_.region) : "";
        out.players = gameJson.contains("joueurs") ? textOf(gameJson["joueurs"]) : "";
        out.rating = gameJson.contains("note") ? textOf(gameJson["note"]) : "";

        if (gameJson.contains("genres") && gameJson["genres"].is_array()) {
            std::vector<std::string> genres;
            for (const auto& genre : gameJson["genres"]) {
                std::string name;
                if (genre.is_object() && genre.contains("noms")) {
                    name = localized(genre["noms"], config_.language);
                } else {
                    name = textOf(genre);
                }
                if (!name.empty()) genres.push_back(name);
                if (genres.size() >= 3) break;
            }
            out.genre = joinWith(genres, ", ");
        }

        if (gameJson.contains("medias")) {
            out.coverUrl = mediaUrl(gameJson["medias"], "box-2D", config_.region);
            if (out.coverUrl.empty()) {
                out.coverUrl = mediaUrl(gameJson["medias"], "box-2D-back", config_.region);
            }
            out.screenshotUrl = mediaUrl(gameJson["medias"], "ss", config_.region);
        }

        out.title = util::htmlUnescape(out.title);
        out.description = util::htmlUnescape(out.description);
        out.developer = util::htmlUnescape(out.developer);
        out.publisher = util::htmlUnescape(out.publisher);
        out.genre = util::htmlUnescape(out.genre);

        if (out.providerGameId.empty() && out.title.empty()) {
            error = "ScreenScraper 未找到匹配的游戏";
            return false;
        }

        Log::instance().info("ScreenScraper matched: " + out.title +
                             " (id=" + out.providerGameId + ")");
        return true;
    } catch (const std::exception& e) {
        error = std::string("ScreenScraper 响应解析失败：") + e.what();
        Log::instance().warn("ScreenScraper fetch JSON parse failed: " +
                             util::redactSecrets(response.body.substr(0, 300)));
        return false;
    }
}

}  // namespace nascade
