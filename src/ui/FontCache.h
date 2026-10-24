#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include <SDL.h>
#include <SDL_ttf.h>

namespace nascade {

class FontCache {
public:
    FontCache() = default;
    ~FontCache();

    bool init(const std::string& fontPath, std::string& error);
    const std::string& fontPath() const { return fontPath_; }

    SDL_Texture* text(SDL_Renderer* renderer, const std::string& text, int ptSize,
                      SDL_Color color, int& width, int& height);
    int measureWidth(const std::string& text, int ptSize);
    int measureHeight(const std::string& text, int ptSize);
    void clear();

private:
    struct Entry {
        SDL_Texture* texture = nullptr;
        int width = 0;
        int height = 0;
        uint64_t lastUse = 0;
    };

    TTF_Font* fontFor(int ptSize);
    static uint32_t colorKey(SDL_Color color);

    std::string fontPath_;
    std::unordered_map<int, TTF_Font*> fonts_;
    std::unordered_map<std::string, Entry> textures_;
    uint64_t tick_ = 0;
    static constexpr size_t kMaxEntries = 512;
};

}  // namespace nascade
