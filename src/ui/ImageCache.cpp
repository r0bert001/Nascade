#include "ui/ImageCache.h"

#include <algorithm>
#include <cmath>
#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#pragma GCC diagnostic pop

#include "core/Log.h"
#include "core/Util.h"

namespace nascade {

ImageCache::~ImageCache() {
    clear();
}

SDL_Texture* ImageCache::load(SDL_Renderer* renderer, const std::string& path,
                              int& width, int& height) {
    width = 0;
    height = 0;
    if (path.empty() || renderer == nullptr) return nullptr;

    auto it = entries_.find(path);
    if (it != entries_.end()) {
        it->second.lastUse = ++tick_;
        width = it->second.width;
        height = it->second.height;
        return it->second.texture;
    }

    int channels = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (!pixels) {
        Log::instance().warn("image load failed: " + path);
        width = 0;
        height = 0;
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STATIC, width, height);
    if (!texture) {
        stbi_image_free(pixels);
        width = 0;
        height = 0;
        return nullptr;
    }
    SDL_UpdateTexture(texture, nullptr, pixels, width * 4);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    stbi_image_free(pixels);

    entries_[path] = Entry{texture, width, height, ++tick_};

    while (entries_.size() > kMaxEntries) {
        auto victim = entries_.begin();
        for (auto i = entries_.begin(); i != entries_.end(); ++i) {
            if (i->second.lastUse < victim->second.lastUse) victim = i;
        }
        if (victim->second.texture) SDL_DestroyTexture(victim->second.texture);
        entries_.erase(victim);
    }
    return texture;
}

void ImageCache::invalidate(const std::string& path) {
    const auto it = entries_.find(path);
    if (it == entries_.end()) return;
    if (it->second.texture) SDL_DestroyTexture(it->second.texture);
    entries_.erase(it);
}

void ImageCache::clear() {
    for (auto& item : entries_) {
        if (item.second.texture) SDL_DestroyTexture(item.second.texture);
    }
    entries_.clear();
}

bool saveScaledPng(const std::string& inputPath, const std::string& outputPath,
                   int targetWidth, std::string& error) {
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load(inputPath.c_str(), &width, &height, &channels, 4);
    if (!pixels) {
        error = "无法读取封面: " + inputPath;
        return false;
    }
    if (width <= 0 || height <= 0 || targetWidth <= 0) {
        stbi_image_free(pixels);
        error = "封面尺寸无效";
        return false;
    }

    const int targetHeight = std::max(1, static_cast<int>(
        std::lround(static_cast<double>(height) * targetWidth / width)));
    std::vector<unsigned char> scaled(static_cast<size_t>(targetWidth) * targetHeight * 4);

    for (int y = 0; y < targetHeight; ++y) {
        const double sy = (targetHeight > 1)
            ? (static_cast<double>(y) * (height - 1) / (targetHeight - 1))
            : 0.0;
        const int y0 = std::max(0, std::min(height - 1, static_cast<int>(sy)));
        const int y1 = std::min(height - 1, y0 + 1);
        const double fy = sy - y0;

        for (int x = 0; x < targetWidth; ++x) {
            const double sx = (targetWidth > 1)
                ? (static_cast<double>(x) * (width - 1) / (targetWidth - 1))
                : 0.0;
            const int x0 = std::max(0, std::min(width - 1, static_cast<int>(sx)));
            const int x1 = std::min(width - 1, x0 + 1);
            const double fx = sx - x0;

            for (int c = 0; c < 4; ++c) {
                const double p00 = pixels[(static_cast<size_t>(y0) * width + x0) * 4 + c];
                const double p10 = pixels[(static_cast<size_t>(y0) * width + x1) * 4 + c];
                const double p01 = pixels[(static_cast<size_t>(y1) * width + x0) * 4 + c];
                const double p11 = pixels[(static_cast<size_t>(y1) * width + x1) * 4 + c];
                const double top = p00 + (p10 - p00) * fx;
                const double bottom = p01 + (p11 - p01) * fx;
                scaled[(static_cast<size_t>(y) * targetWidth + x) * 4 + c] =
                    static_cast<unsigned char>(std::lround(top + (bottom - top) * fy));
            }
        }
    }
    stbi_image_free(pixels);

    util::ensureDir(util::parentPath(outputPath));
    if (!stbi_write_png(outputPath.c_str(), targetWidth, targetHeight, 4,
                        scaled.data(), targetWidth * 4)) {
        error = "无法写入封面: " + outputPath;
        return false;
    }
    return true;
}

}  // namespace nascade
