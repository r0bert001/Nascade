#pragma once

#include <string>
#include <vector>

#include <SDL.h>

#include "ui/FontCache.h"

namespace nascade {

struct Color {
    Uint8 r = 0;
    Uint8 g = 0;
    Uint8 b = 0;
    Uint8 a = 255;
};

namespace theme {
constexpr Color kBg{12, 16, 21, 255};
constexpr Color kPanel{24, 31, 39, 255};
constexpr Color kPanelAlt{30, 39, 49, 255};
constexpr Color kPanelHover{37, 48, 60, 255};
constexpr Color kText{230, 237, 243, 255};
constexpr Color kMuted{139, 152, 165, 255};
constexpr Color kAccent{240, 164, 58, 255};
constexpr Color kAccentSoft{66, 49, 27, 255};
constexpr Color kGreen{93, 196, 138, 255};
constexpr Color kRed{226, 102, 102, 255};
constexpr Color kBorder{48, 60, 72, 255};
constexpr Color kOverlay{0, 0, 0, 170};
}  // namespace theme

enum class Align { Left, Center, Right };

class Painter {
public:
    Painter(SDL_Renderer* renderer, FontCache& fonts);

    void begin(Color clearColor);
    void end();

    void fillRect(int x, int y, int w, int h, Color color);
    void roundedRect(int x, int y, int w, int h, int radius, Color color);
    void roundedOutline(int x, int y, int w, int h, int radius, int thickness,
                        Color border, Color inner);
    void line(int x1, int y1, int x2, int y2, Color color);

    int text(const std::string& value, int x, int y, int ptSize, Color color,
             Align align = Align::Left, int clipWidth = -1);
    int textIn(const std::string& value, int x, int y, int ptSize, Color color,
               const SDL_Rect& clip, Align align = Align::Left);
    int textWidth(const std::string& value, int ptSize);
    int textHeight(const std::string& value, int ptSize);

    void pushClip(const SDL_Rect& clip);
    void popClip();

    SDL_Renderer* raw() const { return renderer_; }
    FontCache& fonts() { return fonts_; }

private:
    void setColor(Color color);
    SDL_Renderer* renderer_ = nullptr;
    FontCache& fonts_;
    std::vector<SDL_Rect> clipStack_;
    bool hasClip_ = false;
};

}  // namespace nascade
