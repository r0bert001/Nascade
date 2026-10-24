#include "ui/Painter.h"

#include <algorithm>
#include <cmath>

namespace nascade {

Painter::Painter(SDL_Renderer* renderer, FontCache& fonts)
    : renderer_(renderer), fonts_(fonts) {}

void Painter::begin(Color clearColor) {
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    setColor(clearColor);
    SDL_RenderClear(renderer_);
    clipStack_.clear();
    hasClip_ = false;
}

void Painter::end() {
    SDL_RenderPresent(renderer_);
}

void Painter::setColor(Color color) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
}

void Painter::fillRect(int x, int y, int w, int h, Color color) {
    if (w <= 0 || h <= 0) return;
    setColor(color);
    SDL_Rect rect{x, y, w, h};
    SDL_RenderFillRect(renderer_, &rect);
}

void Painter::roundedRect(int x, int y, int w, int h, int radius, Color color) {
    if (w <= 0 || h <= 0) return;
    radius = std::max(0, std::min(radius, std::min(w, h) / 2));
    if (radius == 0) {
        fillRect(x, y, w, h, color);
        return;
    }

    setColor(color);
    for (int row = 0; row < h; ++row) {
        int inset = 0;
        if (row < radius) {
            const double dy = radius - row - 0.5;
            inset = radius - static_cast<int>(std::sqrt(std::max(0.0, radius * radius - dy * dy)));
        } else if (row >= h - radius) {
            const double dy = row - (h - radius) + 0.5;
            inset = radius - static_cast<int>(std::sqrt(std::max(0.0, radius * radius - dy * dy)));
        }
        SDL_Rect rect{x + inset, y + row, w - 2 * inset, 1};
        SDL_RenderFillRect(renderer_, &rect);
    }
}

void Painter::roundedOutline(int x, int y, int w, int h, int radius, int thickness,
                             Color border, Color inner) {
    if (thickness < 1) thickness = 1;
    roundedRect(x, y, w, h, radius, border);
    roundedRect(x + thickness, y + thickness, w - 2 * thickness, h - 2 * thickness,
                std::max(0, radius - thickness), inner);
}

void Painter::line(int x1, int y1, int x2, int y2, Color color) {
    setColor(color);
    SDL_RenderDrawLine(renderer_, x1, y1, x2, y2);
}

int Painter::text(const std::string& value, int x, int y, int ptSize, Color color,
                  Align align, int clipWidth) {
    int w = 0;
    int h = 0;
    SDL_Texture* texture = fonts_.text(renderer_, value, ptSize, SDL_Color{color.r, color.g, color.b, color.a}, w, h);
    if (!texture) return 0;

    int drawX = x;
    if (align == Align::Center) drawX = x - w / 2;
    else if (align == Align::Right) drawX = x - w;

    if (clipWidth >= 0) {
        // 水平裁剪时给字形上下各留 1px，避免纹理边缘被裁掉。
        SDL_Rect clip{drawX, y - 1, clipWidth, h + 2};
        pushClip(clip);
    }

    SDL_Rect dst{drawX, y, w, h};
    SDL_RenderCopy(renderer_, texture, nullptr, &dst);

    if (clipWidth >= 0) popClip();
    return w;
}

int Painter::textIn(const std::string& value, int x, int y, int ptSize, Color color,
                    const SDL_Rect& clip, Align align) {
    int w = 0;
    int h = 0;
    SDL_Texture* texture = fonts_.text(renderer_, value, ptSize, SDL_Color{color.r, color.g, color.b, color.a}, w, h);
    if (!texture) return 0;

    int drawX = x;
    if (align == Align::Center) drawX = x - w / 2;
    else if (align == Align::Right) drawX = x - w;

    // 调用方的矩形通常按行高/面板高度估算，可能比实际字形矮几个像素；
    // 垂直方向扩展到完整纹理高度，只保留水平裁剪，避免字体底部被截断。
    SDL_Rect adjusted = clip;
    const int clipBottom = clip.y + clip.h;
    const int textBottom = y + h;
    adjusted.y = std::min(clip.y, y);
    adjusted.h = std::max(clipBottom, textBottom) - adjusted.y;

    pushClip(adjusted);
    SDL_Rect dst{drawX, y, w, h};
    SDL_RenderCopy(renderer_, texture, nullptr, &dst);
    popClip();
    return w;
}

int Painter::textWidth(const std::string& value, int ptSize) {
    return fonts_.measureWidth(value, ptSize);
}

int Painter::textHeight(const std::string& value, int ptSize) {
    return fonts_.measureHeight(value, ptSize);
}

void Painter::pushClip(const SDL_Rect& clip) {
    SDL_Rect previous{};
    SDL_RenderGetClipRect(renderer_, &previous);
    clipStack_.push_back(previous);
    hasClip_ = true;
    SDL_RenderSetClipRect(renderer_, &clip);
}

void Painter::popClip() {
    if (clipStack_.empty()) {
        SDL_RenderSetClipRect(renderer_, nullptr);
        hasClip_ = false;
        return;
    }
    const SDL_Rect previous = clipStack_.back();
    clipStack_.pop_back();
    if (clipStack_.empty() && previous.w == 0 && previous.h == 0) {
        SDL_RenderSetClipRect(renderer_, nullptr);
        hasClip_ = false;
    } else {
        SDL_RenderSetClipRect(renderer_, &previous);
    }
}

}  // namespace nascade
