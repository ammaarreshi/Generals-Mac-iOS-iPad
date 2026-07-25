#include "SagePatch/Hooks.h"
#include "SagePatch/Features.h"
#include "SagePatch/Logger.h"

namespace sagepatch {

bool handleKeyDown(const SDL_KeyboardEvent& ev) {
    if (ev.repeat) return false;

    SDL_Window* window = SDL_GetWindowFromID(ev.windowID);
    if (!window) return false;

    const bool ctrl = (ev.mod & SDL_KMOD_CTRL) != 0;
    // Window snap uses Cmd+Option: the game binds Ctrl+1..0 to control groups,
    // and on macOS Cmd also acts as Ctrl in-game, so any bare-modifier+number
    // combo would shadow them. Cmd+Option+number is unbound in the game.
    const bool gui = (ev.mod & SDL_KMOD_GUI) != 0;
    const bool alt = (ev.mod & SDL_KMOD_ALT) != 0;
    const bool snapMod = gui && alt;

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
            if (snapMod) { moveWindow(window, WindowPosition::Center); return true; }
            break;
        case SDLK_2:
            if (snapMod) { moveWindow(window, WindowPosition::TopLeft); return true; }
            break;
        case SDLK_3:
            if (snapMod) { moveWindow(window, WindowPosition::TopRight); return true; }
            break;
        case SDLK_4:
            if (snapMod) { moveWindow(window, WindowPosition::BottomLeft); return true; }
            break;
        case SDLK_5:
            if (snapMod) { moveWindow(window, WindowPosition::BottomRight); return true; }
            break;

        default:
            break;
    }
    return false;
}

}
