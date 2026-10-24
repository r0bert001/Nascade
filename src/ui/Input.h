#pragma once

#include <string>

#include <SDL.h>

namespace nascade {

enum class Action {
    None,
    Up,
    Down,
    Left,
    Right,
    Confirm,
    Back,
    Option,
    Alt,
    Menu,
    PageUp,
    PageDown,
    Quit,
};

class Input {
public:
    Input() = default;
    ~Input();

    void init();
    void shutdown();
    Action handle(const SDL_Event& event);
    Action repeat();

    bool hasController() const { return controller_ != nullptr; }
    const std::string& controllerName() const { return controllerName_; }

private:
    Action mapKey(const SDL_KeyboardEvent& event) const;
    Action mapButton(Uint8 button) const;
    void openController(int deviceIndex);
    void closeController();
    void notePress(Action action);
    void noteRelease(Action action);
    void clearHeldDirection();

    SDL_GameController* controller_ = nullptr;
    std::string controllerName_;
    int lastAxisX_ = 0;
    int lastAxisY_ = 0;
    Action held_ = Action::None;
    Uint32 pressAt_ = 0;
    Uint32 nextRepeatAt_ = 0;
};

}  // namespace nascade
