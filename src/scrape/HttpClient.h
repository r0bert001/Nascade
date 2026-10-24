#pragma once

#include <string>
#include <vector>

namespace nascade {

struct HttpResponse {
    bool ok = false;
    long status = 0;
    std::string body;
    std::string error;
};

class HttpClient {
public:
    static HttpResponse get(const std::string& url,
                            const std::vector<std::string>& headers = {},
                            long timeoutSec = 20);
    static bool download(const std::string& url, const std::string& outputPath,
                         std::string& error, long timeoutSec = 120);
    static std::string urlEncode(const std::string& value);
    static void setCaBundle(const std::string& path);

private:
    static void ensureInit();
};

}  // namespace nascade
