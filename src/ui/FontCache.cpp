#include "ui/FontCache.h"

#include <algorithm>
#include <cstdio>

#include "core/Log.h"

namespace nascade {

FontCache::~FontCache() {
    clear();
}

bool FontCache::init(const std::string& fontPath, std::string& error) {
    clear();
    fontPath_ = fontPath;

    if (TTF_WasInit() == 0) {
        if (TTF_Init() != 0) {
            error = std::string("TTF_Init failed: ") + TTF_GetError();
            return false;
        }
    }

    TTF_Font* font = TTF_OpenFont(fontPath.c_str(), 28);
    if (!font) {
        error = std::string("无法加载字体 ") + fontPath + ": " + TTF_GetError();
        fontPath_.clear();
        return false;
    }
    TTF_SetFontHinting(font, TTF_HINTING_LIGHT);
    fonts_[28] = font;
    return true;
}

TTF_Font* FontCache::fontFor(int ptSize) {
    const auto it = fonts_.find(ptSize);
    if (it != fonts_.end()) return it->second;

    TTF_Font* font = TTF_OpenFont(fontPath_.c_str(), ptSize);
    if (!font) {
        Log::instance().error(std::string("TTF_OpenFont failed: ") + TTF_GetError());
        return nullptr;
    }
    TTF_SetFontHinting(font, TTF_HINTING_LIGHT);
    fonts_[ptSize] = font;
    return font;
}

uint32_t FontCache::colorKey(SDL_Color color) {
    return (static_cast<uint32_t>(color.r) << 24) |
           (static_cast<uint32_t>(color.g) << 16) |
           (static_cast<uint32_t>(color.b) << 8) |
           static_cast<uint32_t>(color.a);
}

SDL_Texture* FontCache::text(SDL_Renderer* renderer, const std::string& value, int ptSize,
                             SDL_Color color, int& width, int& height) {
    width = 0;
    height = 0;
    if (value.empty() || renderer == nullptr) return nullptr;

    const std::string key = std::to_string(ptSize) + "|" +
                            std::to_string(colorKey(color)) + "|" + value;
    auto it = textures_.find(key);
    if (it != textures_.end()) {
        it->second.lastUse = ++tick_;
        width = it->second.width;
        height = it->second.height;
        return it->second.texture;
    }

    TTF_Font* font = fontFor(ptSize);
    if (!font) return nullptr;

    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, value.c_str(), color);
    if (!surface) return nullptr;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    const int w = surface->w;
    const int h = surface->h;
    SDL_FreeSurface(surface);
    if (!texture) return nullptr;

    textures_[key] = Entry{texture, w, h, ++tick_};
    width = w;
    height = h;

    while (textures_.size() > kMaxEntries) {
        auto victim = textures_.begin();
        for (auto i = textures_.begin(); i != textures_.end(); ++i) {
            if (i->second.lastUse < victim->second.lastUse) victim = i;
        }
        if (victim->second.texture) SDL_DestroyTexture(victim->second.texture);
        textures_.erase(victim);
    }
    return texture;
}

int FontCache::measureWidth(const std::string& value, int ptSize) {
    TTF_Font* font = fontFor(ptSize);
    if (!font) return 0;
    int w = 0;
    int h = 0;
    if (TTF_SizeUTF8(font, value.c_str(), &w, &h) != 0) return 0;
    return w;
}

int FontCache::measureHeight(const std::string& value, int ptSize) {
    TTF_Font* font = fontFor(ptSize);
    if (!font) return 0;
    int w = 0;
    int h = 0;
    if (TTF_SizeUTF8(font, value.c_str(), &w, &h) != 0) return 0;
    return h;
}

void FontCache::clear() {
    for (auto& item : textures_) {
        if (item.second.texture) SDL_DestroyTexture(item.second.texture);
    }
    textures_.clear();
    for (auto& item : fonts_) {
        if (item.second) TTF_CloseFont(item.second);
    }
    fonts_.clear();
}

}  // namespace nascade
