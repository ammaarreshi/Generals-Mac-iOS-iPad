#include "SagePatch/Hooks.h"
#include "SagePatch/Features.h"
#include "SagePatch/Logger.h"

namespace sagepatch {

bool handleKeyDown(const SDL_KeyboardEvent& ev) {
    if (ev.repeat) return false;

    SDL_Window* window = SDL_GetWindowFromID(ev.windowID);
    if (!window) return false;

    const bool ctrl = (ev.mod & SDL_KMOD_CTRL) != 0;
    // @fix window-snap used Ctrl+1..5, which shadows Generals' native
    // Ctrl+<number> control-group hotkeys (SDL_PollEvent interposer here
    // consumes the event before the game ever sees it). Moved to Alt so both
    // features work: Alt+1..5 for window position, Ctrl+1..9/0 for control groups.
    const bool alt = (ev.mod & SDL_KMOD_ALT) != 0;

    switch (ev.key) {
        case SDLK_F11:
            takeScreenshot(window);
            return true;

        case SDLK_SCROLLLOCK:
            toggleCursorLock(window);
            return true;

        case SDLK_PAGEUP:
            if (ctrl) { adjustBrightness(+8); return true; }
            break;
        case SDLK_PAGEDOWN:
            if (ctrl) { adjustBrightness(-8); return true; }
            break;

        case SDLK_1:
            if (alt) { moveWindow(window, WindowPosition::Center); return true; }
            break;
        case SDLK_2:
            if (alt) { moveWindow(window, WindowPosition::TopLeft); return true; }
            break;
        case SDLK_3:
            if (alt) { moveWindow(window, WindowPosition::TopRight); return true; }
            break;
        case SDLK_4:
            if (alt) { moveWindow(window, WindowPosition::BottomLeft); return true; }
            break;
        case SDLK_5:
            if (alt) { moveWindow(window, WindowPosition::BottomRight); return true; }
            break;

        default:
            break;
    }
    return false;
}

}
