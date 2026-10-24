#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include <SDL.h>

namespace nascade {

class ImageCache {
public:
    ImageCache() = default;
    ~ImageCache();

    SDL_Texture* load(SDL_Renderer* renderer, const std::string& path, int& width, int& height);
    void invalidate(const std::string& path);
    void clear();

private:
    struct Entry {
        SDL_Texture* texture = nullptr;
        int width = 0;
        int height = 0;
        uint64_t lastUse = 0;
    };

    std::unordered_map<std::string, Entry> entries_;
    uint64_t tick_ = 0;
    static constexpr size_t kMaxEntries = 64;
};

bool saveScaledPng(const std::string& inputPath, const std::string& outputPath,
                   int targetWidth, std::string& error);

}  // namespace nascade
