#include "SagePatch/Hooks.h"
#include "SagePatch/Features.h"
#include "SagePatch/Logger.h"

namespace sagepatch {

bool handleKeyDown(const SDL_KeyboardEvent& ev) {
    if (ev.repeat) return false;

    SDL_Window* window = SDL_GetWindowFromID(ev.windowID);
    if (!window) return false;

    // GeneralsX @bugfix MrMeeseeks 16/06/2026 Use command on macOS, control on Linux, and require Alt to avoid conflict with gameplay group binding
#ifdef __APPLE__
    const bool modifier = (ev.mod & SDL_KMOD_GUI) != 0;
#else
    const bool modifier = (ev.mod & SDL_KMOD_CTRL) != 0;
#endif
    const bool alt = (ev.mod & SDL_KMOD_ALT) != 0;

    switch (ev.key) {
        case SDLK_F11:
            takeScreenshot(window);
            return true;

        case SDLK_SCROLLLOCK:
            toggleCursorLock(window);
            return true;

        case SDLK_PAGEUP:
            if (modifier) { adjustBrightness(+8); return true; }
            break;
        case SDLK_PAGEDOWN:
            if (modifier) { adjustBrightness(-8); return true; }
            break;

        case SDLK_1:
            if (modifier && alt) { moveWindow(window, WindowPosition::Center); return true; }
            break;
        case SDLK_2:
            if (modifier && alt) { moveWindow(window, WindowPosition::TopLeft); return true; }
            break;
        case SDLK_3:
            if (modifier && alt) { moveWindow(window, WindowPosition::TopRight); return true; }
            break;
        case SDLK_4:
            if (modifier && alt) { moveWindow(window, WindowPosition::BottomLeft); return true; }
            break;
        case SDLK_5:
            if (modifier && alt) { moveWindow(window, WindowPosition::BottomRight); return true; }
            break;

        default:
            break;
    }
    return false;
}

}
