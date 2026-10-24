#include "scrape/HttpClient.h"

#include <cstdio>
#include <mutex>

#include <curl/curl.h>

#include "core/Log.h"
#include "core/Util.h"

namespace nascade {

namespace {

std::once_flag g_curlOnce;
std::string g_caBundle;

struct StringSink {
    std::string* body = nullptr;
    size_t maxBytes = 16 * 1024 * 1024;
    bool overflow = false;
};

size_t writeString(char* data, size_t size, size_t count, void* userdata) {
    const size_t bytes = size * count;
    auto* sink = static_cast<StringSink*>(userdata);
    if (sink->body->size() + bytes > sink->maxBytes) {
        sink->overflow = true;
        return 0;
    }
    sink->body->append(data, bytes);
    return bytes;
}

size_t writeFile(char* data, size_t size, size_t count, void* userdata) {
    FILE* file = static_cast<FILE*>(userdata);
    return std::fwrite(data, size, count, file);
}

void configure(CURL* curl, const std::string& url, long timeoutSec) {
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSec);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Nascade/0.1 (TrimUI Brick Pro)");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    static const std::string kFallbackCa = "assets/cacert.pem";
    std::string bundle = g_caBundle;
    if (bundle.empty() && util::fileExists(kFallbackCa)) {
        bundle = kFallbackCa;
    }
    if (!bundle.empty()) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, bundle.c_str());
    }
}

}  // namespace

void HttpClient::ensureInit() {
    std::call_once(g_curlOnce, []() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    });
}

void HttpClient::setCaBundle(const std::string& path) {
    g_caBundle = path;
}

HttpResponse HttpClient::get(const std::string& url,
                             const std::vector<std::string>& headers,
                             long timeoutSec) {
    ensureInit();
    HttpResponse response;

    CURL* curl = curl_easy_init();
    if (!curl) {
        response.error = "curl_easy_init failed";
        return response;
    }

    configure(curl, url, timeoutSec);

    struct curl_slist* headerList = nullptr;
    for (const auto& header : headers) {
        headerList = curl_slist_append(headerList, header.c_str());
    }
    if (headerList) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);

    StringSink sink;
    sink.body = &response.body;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);

    const CURLcode result = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);

    if (result != CURLE_OK) {
        response.error = curl_easy_strerror(result);
        Log::instance().warn("HTTP GET failed: " + response.error + " " + util::redactSecrets(url));
    } else if (sink.overflow) {
        response.error = "response too large";
    } else {
        response.ok = response.status >= 200 && response.status < 300;
        if (!response.ok) {
            response.error = "HTTP status " + std::to_string(response.status);
        }
    }

    if (headerList) curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);
    return response;
}

bool HttpClient::download(const std::string& url, const std::string& outputPath,
                          std::string& error, long timeoutSec) {
    ensureInit();
    util::ensureDir(util::parentPath(outputPath));

    const std::string partPath = outputPath + ".part";
    FILE* file = std::fopen(partPath.c_str(), "wb");
    if (!file) {
        error = "无法创建文件: " + partPath;
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::fclose(file);
        std::remove(partPath.c_str());
        error = "curl_easy_init failed";
        return false;
    }

    configure(curl, url, timeoutSec);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFile);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);

    const CURLcode result = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    std::fclose(file);

    if (result != CURLE_OK) {
        error = curl_easy_strerror(result);
        std::remove(partPath.c_str());
        Log::instance().warn("HTTP download failed: " + error + " " + util::redactSecrets(url));
        return false;
    }
    if (status < 200 || status >= 300) {
        error = "HTTP status " + std::to_string(status);
        std::remove(partPath.c_str());
        return false;
    }
    std::remove(outputPath.c_str());
    if (std::rename(partPath.c_str(), outputPath.c_str()) != 0) {
        error = "无法重命名临时文件: " + outputPath;
        std::remove(partPath.c_str());
        return false;
    }
    return true;
}

std::string HttpClient::urlEncode(const std::string& value) {
    ensureInit();
    CURL* curl = curl_easy_init();
    if (!curl) return value;
    char* encoded = curl_easy_escape(curl, value.c_str(), static_cast<int>(value.size()));
    std::string out = encoded ? encoded : value;
    if (encoded) curl_free(encoded);
    curl_easy_cleanup(curl);
    return out;
}

}  // namespace nascade
