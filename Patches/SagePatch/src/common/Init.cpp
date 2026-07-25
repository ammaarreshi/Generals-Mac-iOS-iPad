#include "SagePatch/Hooks.h"
#include "SagePatch/Logger.h"

namespace sagepatch {

#if defined(__APPLE__)
#  define SAGE_LOAD_MECHANISM "DYLD_INSERT_LIBRARIES"
#elif defined(__linux__)
#  define SAGE_LOAD_MECHANISM "LD_PRELOAD"
#else
#  define SAGE_LOAD_MECHANISM "preload"
#endif

__attribute__((constructor))
static void onLoad() {
    SAGEPATCH_LOG("Loaded (version %s) via %s", SAGE_PATCH_VERSION, SAGE_LOAD_MECHANISM);
    init();
}

__attribute__((destructor))
static void onUnload() {
    shutdown();
    SAGEPATCH_LOG("Unloaded");
}

void init() {
    SAGEPATCH_LOG("Hot-keys:");
    SAGEPATCH_LOG("  F11             screenshot (PNG to ~/Pictures/GeneralsX)");
    SAGEPATCH_LOG("  Scroll Lock     toggle cursor lock");
    SAGEPATCH_LOG("  Ctrl+PageUp/Dn  brightness +/-");
    SAGEPATCH_LOG("  Cmd+Option+1..5 window position (center / TL / TR / BL / BR)");
}

void shutdown() {}

}
