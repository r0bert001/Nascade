#include "ui/Input.h"

#include "core/Log.h"

namespace nascade {

namespace {

bool isDirection(Action action) {
    return action == Action::Up || action == Action::Down ||
           action == Action::Left || action == Action::Right;
}

}  // namespace

Input::~Input() {
    closeController();
}

void Input::init() {
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        Log::instance().warn(std::string("SDL gamecontroller init failed: ") + SDL_GetError());
        return;
    }
    SDL_GameControllerEventState(SDL_ENABLE);

    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            openController(i);
            break;
        }
    }
    if (controller_) {
        Log::instance().info("controller: " + controllerName_);
    } else {
        Log::instance().info("no controller detected; keyboard input enabled");
    }
}

void Input::shutdown() {
    closeController();
    held_ = Action::None;
}

Action Input::repeat() {
    // 只对上下长按做重复；首次 350ms 后开始，之后 80ms 一次，
    // 持续 1.2s 后加速到 40ms 一次，便于快速浏览长列表。
    if (held_ != Action::Up && held_ != Action::Down) return Action::None;
    const Uint32 now = SDL_GetTicks();
    if (now < nextRepeatAt_) return Action::None;
    const Uint32 heldMs = now - pressAt_;
    nextRepeatAt_ = now + (heldMs >= 1200 ? 40 : 80);
    return held_;
}

void Input::notePress(Action action) {
    if (!isDirection(action)) return;
    held_ = action;
    pressAt_ = SDL_GetTicks();
    nextRepeatAt_ = pressAt_ + 350;
}

void Input::noteRelease(Action action) {
    if (held_ == action) held_ = Action::None;
}

void Input::clearHeldDirection() {
    if (isDirection(held_)) held_ = Action::None;
}

void Input::openController(int deviceIndex) {
    closeController();
    controller_ = SDL_GameControllerOpen(deviceIndex);
    if (!controller_) {
        Log::instance().warn(std::string("SDL_GameControllerOpen failed: ") + SDL_GetError());
        return;
    }
    const char* name = SDL_GameControllerName(controller_);
    controllerName_ = name ? name : "Game Controller";
}

void Input::closeController() {
    if (controller_) {
        SDL_GameControllerClose(controller_);
        controller_ = nullptr;
        controllerName_.clear();
    }
    lastAxisX_ = 0;
    lastAxisY_ = 0;
    clearHeldDirection();
}

Action Input::mapKey(const SDL_KeyboardEvent& event) const {
    const SDL_Keycode key = event.keysym.sym;
    const bool ctrl = (event.keysym.mod & KMOD_CTRL) != 0;

    switch (key) {
        case SDLK_UP:
        case SDLK_w:
            return Action::Up;
        case SDLK_DOWN:
        case SDLK_s:
            return Action::Down;
        case SDLK_LEFT:
        case SDLK_a:
            return Action::Left;
        case SDLK_RIGHT:
        case SDLK_d:
            return Action::Right;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_SPACE:
        case SDLK_z:
            return Action::Confirm;
        case SDLK_ESCAPE:
        case SDLK_BACKSPACE:
        case SDLK_x:
            return Action::Back;
        case SDLK_TAB:
        case SDLK_c:
            return Action::Option;
        case SDLK_y:
            return Action::Alt;
        case SDLK_F1:
            return Action::Menu;
        case SDLK_PAGEUP:
            return Action::PageUp;
        case SDLK_PAGEDOWN:
            return Action::PageDown;
        case SDLK_q:
            return ctrl ? Action::Quit : Action::PageUp;
        case SDLK_e:
            return Action::PageDown;
        case SDLK_F12:
            return Action::Quit;
        default:
            return Action::None;
    }
}

Action Input::mapButton(Uint8 button) const {
    switch (button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
            return Action::Up;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            return Action::Down;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            return Action::Left;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            return Action::Right;
        // Brick Pro 原厂固件的物理 A/B、X/Y 与 SDL 逻辑按键相反：
        // 物理 A→SDL B，物理 B→SDL A；物理 X→SDL Y，物理 Y→SDL X。
        case SDL_CONTROLLER_BUTTON_A:
            return Action::Back;
        case SDL_CONTROLLER_BUTTON_B:
            return Action::Confirm;
        case SDL_CONTROLLER_BUTTON_X:
            return Action::Alt;
        case SDL_CONTROLLER_BUTTON_Y:
            return Action::Option;
        case SDL_CONTROLLER_BUTTON_START:
            return Action::Menu;
        case SDL_CONTROLLER_BUTTON_BACK:
            return Action::Back;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
            return Action::PageUp;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
            return Action::PageDown;
        case SDL_CONTROLLER_BUTTON_GUIDE:
            return Action::Quit;
        default:
            return Action::None;
    }
}

Action Input::handle(const SDL_Event& event) {
    switch (event.type) {
        case SDL_QUIT:
            return Action::Quit;

        case SDL_KEYDOWN: {
            if (event.key.repeat) return Action::None;
            const Action action = mapKey(event.key);
            notePress(action);
            return action;
        }

        case SDL_KEYUP: {
            noteRelease(mapKey(event.key));
            return Action::None;
        }

        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                clearHeldDirection();
            }
            return Action::None;

        case SDL_CONTROLLERDEVICEADDED:
            if (!controller_) {
                openController(event.cdevice.which);
            }
            return Action::None;

        case SDL_CONTROLLERDEVICEREMOVED:
            if (controller_) {
                SDL_Joystick* joystick = SDL_GameControllerGetJoystick(controller_);
                if (joystick && SDL_JoystickInstanceID(joystick) == event.cdevice.which) {
                    closeController();
                }
            }
            return Action::None;

        case SDL_CONTROLLERBUTTONDOWN: {
            const Action action = mapButton(event.cbutton.button);
            notePress(action);
            return action;
        }

        case SDL_CONTROLLERBUTTONUP:
            noteRelease(mapButton(event.cbutton.button));
            return Action::None;

        case SDL_CONTROLLERAXISMOTION: {
            if (event.caxis.axis != SDL_CONTROLLER_AXIS_LEFTX &&
                event.caxis.axis != SDL_CONTROLLER_AXIS_LEFTY) {
                return Action::None;
            }
            const int threshold = 18000;
            const int value = event.caxis.value;
            const int direction = value > threshold ? 1 : (value < -threshold ? -1 : 0);

            if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
                if (direction == lastAxisX_) return Action::None;
                lastAxisX_ = direction;
                if (direction == 0) {
                    clearHeldDirection();
                    return Action::None;
                }
                const Action action = direction > 0 ? Action::Right : Action::Left;
                notePress(action);
                return action;
            }

            if (direction == lastAxisY_) return Action::None;
            lastAxisY_ = direction;
            if (direction == 0) {
                clearHeldDirection();
                return Action::None;
            }
            const Action action = direction > 0 ? Action::Down : Action::Up;
            notePress(action);
            return action;
        }

        default:
            return Action::None;
    }
}

}  // namespace nascade
