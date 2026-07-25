// Window position presets — Cmd+Option+1..5 to snap the window to common positions
// on the active display. SDL3 does the work; works on every platform.

#include "SagePatch/Features.h"
#include "SagePatch/Logger.h"

#include <SDL3/SDL.h>

namespace sagepatch {

void moveWindow(SDL_Window* window, WindowPosition where) {
    if (!window) return;

    SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    SDL_Rect bounds{};
    if (display == 0 || !SDL_GetDisplayUsableBounds(display, &bounds)) {
        SAGEPATCH_LOG("WindowPosition: cannot resolve display bounds");
        return;
    }

    int w = 0, h = 0;
    SDL_GetWindowSize(window, &w, &h);

    int x = bounds.x, y = bounds.y;
    const char* name = "?";

    switch (where) {
        case WindowPosition::Center:
            x = bounds.x + (bounds.w - w) / 2;
            y = bounds.y + (bounds.h - h) / 2;
            name = "center";
            break;
        case WindowPosition::TopLeft:
            x = bounds.x;
            y = bounds.y;
            name = "top-left";
            break;
        case WindowPosition::TopRight:
            x = bounds.x + bounds.w - w;
            y = bounds.y;
            name = "top-right";
            break;
        case WindowPosition::BottomLeft:
            x = bounds.x;
            y = bounds.y + bounds.h - h;
            name = "bottom-left";
            break;
        case WindowPosition::BottomRight:
            x = bounds.x + bounds.w - w;
            y = bounds.y + bounds.h - h;
            name = "bottom-right";
            break;
    }

    SDL_SetWindowPosition(window, x, y);
    SAGEPATCH_LOG("Window position: %s (%d,%d)", name, x, y);
}

}
