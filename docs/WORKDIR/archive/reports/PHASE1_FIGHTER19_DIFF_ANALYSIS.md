# Phase 1: fighter19 DXVK Port vs GeneralsX - Rendering Diff Analysis

**Status**: Complete
**Goal**: Diagnose magenta screen (0xFF00FF) on main menu — menu text renders but no 3D terrain/objects appear
**Scope**: 8 investigation areas comparing `references/old-refs/fighter19-dxvk-port/` (working Linux build) with our codebase

---

## Executive Summary

After comprehensive file-by-file diffing of all rendering-critical code paths, the analysis identified **one critical bug** and **two high-risk architectural differences** between our codebase and fighter19's working Linux port:

| Priority | Finding | File | Impact |
|----------|---------|------|--------|
| **CRITICAL** | Double-draw bug: redundant `TheDisplay->step()/draw()` | `GameEngine.cpp:1033-1039` | Extra render pass per frame; state corruption risk |
| **HIGH** | FramePacer replaces original frame-rate limiter | `GameEngine.cpp:930-950` | Alters game loop timing, conditionally skips `step()` |
| **HIGH** | Headless guards gate ALL rendering init | `W3DDisplay.cpp:656,711,848` | If `m_headless` incorrectly set, 3D scenes never created |
| **MEDIUM** | GetBackBuffer missing error check | `dx8wrapper.cpp` | Could silently use NULL surface pointer |
| **LOW** | SurfaceClass Lock/DrawPixel API changes | `surfaceclass.cpp` | Signature differences in locking API |
| **INFO** | ww3d.cpp, ddsfile.cpp are **identical** | — | Rendering pipeline and DDS loading unchanged |
| **INFO** | Texture loading has only cosmetic diffs | texture.cpp, textureloader.cpp, dx8texman.cpp | nullptr→NULL, whitespace only |

**Recommendation**: Remove the double-draw block first (CRITICAL). If magenta persists, investigate `m_headless` flag value at runtime.

---

## Finding 1: CRITICAL — Double-Draw Bug in GameEngine::execute()

### Location
[GameEngine.cpp](GeneralsMD/Code/GameEngine/Source/Common/GameEngine.cpp#L1033-L1039)

### What We Have (our code)
```cpp
// In GameEngine::execute(), AFTER update() returns:

TheFramePacer->update();

// GeneralsX @build BenderAI 18/02/2026 - Call display step and draw every frame
// This was missing, causing only magenta screen (no UI rendering)
if (TheDisplay != nullptr)
{
    TheDisplay->step();
    TheDisplay->draw();
}
```

### What fighter19 Has (working port)
```cpp
// In GameEngine::execute(), AFTER update() returns:
// (frame rate limiting with Sleep/timeGetTime only — no display calls)
{
    if (TheTacticalView->getTimeMultiplier()<=1 && !TheScriptEngine->isTimeFast())
    {
        ::Sleep(1);
        // ... frame rate limiter using timeGetTime() ...
    }
}
```

### Why This Is a Bug

The call chain already draws the display:

1. `GameEngine::execute()` calls `update()`
2. `update()` calls `TheGameClient->UPDATE()`
3. `TheGameClient->UPDATE()` dispatches to `GameClient::update()` (GameClient.cpp)
4. `GameClient::update()` at [line 792](GeneralsMD/Code/GameEngine/Source/GameClient/GameClient.cpp#L792) calls `TheDisplay->DRAW()`
5. `GameClient::update()` at [line 783](GeneralsMD/Code/GameEngine/Source/GameClient/GameClient.cpp#L783) calls `TheDisplay->UPDATE()`

Then, `TheGameClient->step()` (conditionally, via FramePacer) calls `GameClient::step()` at [line 813](GeneralsMD/Code/GameEngine/Source/GameClient/GameClient.cpp#L813) which calls `TheDisplay->step()`.

The added block in `execute()` calls `TheDisplay->step()` and `TheDisplay->draw()` **a second time** after `update()` already did it. This means:

- **Two `Begin_Render` / `End_Render` cycles per frame** — the second call clears the backbuffer (black), renders scene, presents
- **Two `Present()` calls per frame** — potentially causing buffer flickering
- **State corruption risk** — the redundant render pass happens after FramePacer and before the next game tick, when scene state may be partially stale

### Recommended Fix
Delete lines 1033-1039 entirely. The display is already drawn via the `GameClient::update()` → `TheDisplay->DRAW()` path, which is how fighter19 and the original game work.

---

## Finding 2: HIGH — FramePacer Replaces Original Frame-Rate Limiter

### Location
[GameEngine.cpp](GeneralsMD/Code/GameEngine/Source/Common/GameEngine.cpp#L930-L950) (update method)
[GameEngine.cpp](GeneralsMD/Code/GameEngine/Source/Common/GameEngine.cpp#L1030) (execute method)

### What We Have
```cpp
// In update():
const Bool canUpdate = canUpdateGameLogic();
const Bool canUpdateLogic = canUpdate && !TheFramePacer->isGameHalted() && !TheFramePacer->isTimeFrozen();
const Bool canUpdateScript = canUpdate && !TheFramePacer->isGameHalted();

if (canUpdateLogic) {
    TheGameClient->step();
    TheGameLogic->UPDATE();
}
```
```cpp
// In execute(), after update():
TheFramePacer->update();
```

### What fighter19 Has
```cpp
// In update():
if ((TheNetwork == NULL && !TheGameLogic->isGamePaused()) ||
    (TheNetwork && TheNetwork->isFrameDataReady())) {
    TheGameLogic->UPDATE();
}
```
```cpp
// In execute(), after update():
::Sleep(1);
DWORD now = timeGetTime();
while (now - prevTime < limit) { ::Sleep(0); now = timeGetTime(); }
prevTime = now;
```

### Key Differences

| Aspect | fighter19 | Our code |
|--------|-----------|----------|
| Logic gate | Network + pause state | FramePacer halt/freeze |
| `TheGameClient->step()` | Called from inside `update()` naturally | Called only when `canUpdateLogic` is true |
| Frame limiting | `Sleep()` + `timeGetTime()` loop | `TheFramePacer->update()` |
| `TheGameLogic->UPDATE()` | Called after step naturally | Gated by FramePacer conditions |

### Risk
If `TheFramePacer->isGameHalted()` or `isTimeFrozen()` returns true during normal gameplay (e.g., FramePacer misconfigured on Linux), then:
- `TheGameClient->step()` is never called → `TheDisplay->step()` never fires
- `TheGameLogic->UPDATE()` never fires → game logic frozen
- **But** `TheGameClient->UPDATE()` still fires (before the FramePacer check) → `TheDisplay->DRAW()` still fires

This could cause the display to draw the same initial frame repeatedly — which with no terrain loaded would show the missing-texture magenta.

### Recommended Investigation
1. Check `TheFramePacer->isGameHalted()` and `isTimeFrozen()` values during shell map loading
2. Compare with original `TheNetwork == NULL && !TheGameLogic->isGamePaused()` condition
3. Consider whether FramePacer should be disabled on Linux until rendering is confirmed working

---

## Finding 3: HIGH — Headless Guards Gate All Rendering Initialization

### Location
[W3DDisplay.cpp](GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp#L656) — Scene creation
[W3DDisplay.cpp](GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp#L711) — WW3D::Init + Set_Render_Device
[W3DDisplay.cpp](GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp#L848) — init2DScene, init3DScene, W3DShaderManager

### What We Have
```cpp
if (!TheGlobalData->m_headless) {
    // Create 3D interface scene, 2D scene, 3D scene
    m_3DInterfaceScene = NEW_REF(RTS3DInterfaceScene, ());
    m_2DScene = NEW_REF(RTS2DScene, ());
    m_3DScene = NEW_REF(RTS3DScene, ());
    // ... lights setup ...
}
// ...
if (!TheGlobalData->m_headless) {
    WW3D::Init(ApplicationHWnd);
    WW3D::Set_Render_Device(...);
    // ... resolution, gamma ...
}
// ...
if (!TheGlobalData->m_headless) {
    init2DScene();
    init3DScene();
    W3DShaderManager::init();
}
```

### What fighter19 Has
**None of these headless guards exist.** All rendering initialization runs unconditionally.

### Risk
`m_headless` is initialized to `FALSE` in [GlobalData.cpp:640](GeneralsMD/Code/GameEngine/Source/Common/GlobalData.cpp#L640) and only set to `TRUE` via the `-headless` command-line flag at [CommandLine.cpp:420](GeneralsMD/Code/GameEngine/Source/Common/CommandLine.cpp#L420). Under normal operation, this should be `FALSE`.

However, if any code path incorrectly sets `m_headless = TRUE`, or if the GlobalData initialization order differs on Linux, then ALL 3D rendering infrastructure is skipped — the scenes, WW3D device, shaders, and 2D renderer would never be created.

The `W3DDisplay::draw()` method also checks `m_headless` and early-returns if true:
```cpp
void W3DDisplay::draw(void) {
    // ...
    if (TheGlobalData->m_headless)
        return;
    // ...
}
```

### Recommended Investigation
1. Add a `fprintf(stderr, "headless=%d\n", TheGlobalData->m_headless)` in `W3DDisplay::init()` to confirm value
2. If `m_headless` is confirmed FALSE, these guards are not the problem
3. The guards themselves are correct architecture (TheSuperHackers uses them for headless replay playback)

---

## Finding 4: MEDIUM — GetBackBuffer Missing Error Check

### Location
`GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/dx8wrapper.cpp` — in rendering setup

### What We Have
```cpp
DX8CALL(GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backBuffer));
// Uses backBuffer without checking if call succeeded
```

### What fighter19 Has
```cpp
DX8CALL_HRES(GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backBuffer), hres);
if (hres == D3D_OK) {
    // Only uses backBuffer if call succeeded
    backBuffer->GetDesc(&desc);
    backBuffer->Release();
}
```

### Risk
If DXVK's `GetBackBuffer` fails (e.g., device lost, format incompatible), our code would use a NULL or invalid pointer. This could cause silent corruption rather than an obvious crash.

### Recommended Fix
Switch to `DX8CALL_HRES` pattern with result check before using `backBuffer`.

---

## Finding 5: LOW — SurfaceClass API Signature Changes

### Location
`Core/Libraries/Source/WWVegas/WW3D2/surfaceclass.cpp`

### Key Differences

| Method | Our code | fighter19 |
|--------|----------|-----------|
| `Lock()` return | `LockedSurfacePtr` typedef | `void*` |
| `Lock()` overload | `Lock(pitch, min, max)` with region | No overload |
| `Draw_Pixel()` | Takes `bytesPerPixel, pBits, pitch` (pre-locked) | Takes only `x, y, color` (locks/unlocks per call) |
| `Get_Pixel()` | Takes `pBits, pitch` (pre-locked) | Locks/unlocks per call |
| Bytes-per-pixel | `Get_Bytes_Per_Pixel()` member function | `PixelSize()` free function |
| Pointer NULL | `nullptr` for RECT* in `LockRect()` | `0` (integer zero) |

### Risk
These are surface-level API changes from TheSuperHackers upstream that should be functionally equivalent. The pre-locked pattern (our code) is actually more efficient since it avoids repeated Lock/Unlock calls. However, if `nullptr` vs `0` causes different behavior in DXVK's `LockRect` implementation, that could be an issue.

### Recommendation
Low priority. Only investigate if other fixes don't resolve the magenta screen.

---

## Finding 6: INFO — Missing Texture Is Magenta 0x7FFF00FF

### Location
[missingtexture.cpp:92](Core/Libraries/Source/WWVegas/WW3D2/missingtexture.cpp#L92)

### Details
The missing texture fallback color is `0x7FFF00FF` = ARGB(127, 255, 0, 255) — **magenta with 50% alpha**.

This is the texture applied to any mesh when its texture fails to load. If the screen shows magenta on all 3D objects, it means textures are not being loaded from the BIG archives or the texture loading pipeline is failing.

fighter19 added `static_assert` checks for `D3DLOCKED_RECT::pBits` offset (64-bit compatibility), but the actual missing texture creation code is functionally identical.

### Implication
The magenta screen could mean:
1. Texture files not found (VFS/BIG archive path issue), OR
2. Texture loading silently failing (format conversion issue), OR
3. No 3D geometry being rendered at all (scene not initialized), OR
4. Clear color overwriting rendered content (double-draw)

---

## Finding 7: INFO — Identical Files (No Rendering Pipeline Changes)

These files are **byte-for-byte identical** between our code and fighter19:
- `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/ww3d.cpp` — WW3D engine core (Init, Begin_Render, End_Render, Set_Render_Device)
- `Core/Libraries/Source/WWVegas/WW3D2/ddsfile.cpp` — DDS texture file loading

This confirms the core rendering pipeline and DDS loading code are unchanged. The magenta screen issue is NOT caused by changes to these systems.

---

## Finding 8: INFO — Texture Loading Code Has Only Cosmetic Differences

### texture.cpp
- `nullptr` → `NULL`
- `#include "TARGA.h"` → `#include "targa.h"` (case only)
- No functional changes

### textureloader.cpp
- `nullptr` → `NULL`/`0`
- `D3DLOCKED_RECT locked_rects[12]={0}` → `D3DLOCKED_RECT locked_rects[12]` (our code zero-initializes, fighter19 doesn't)
- `.str()` → `.Peek_Buffer()` (StringClass API)
- `FALLTHROUGH` macro present (ours) vs absent (fighter19)
- No functional changes

### dx8texman.cpp
- `track=NULL` added after `delete track` in fighter19
- Whitespace only otherwise

### dx8caps.cpp
- Our code adds `VENDOR_VMWARE` detection (0x15AD)
- String comparison style: `stricmp(x,y) == 0` (ours) vs `!stricmp(x,y)` (fighter19)
- `static const char* const VendorNames[]` with `static_assert` (ours, better)
- No functional impact on texture loading or rendering

---

## Supplementary: dx8wrapper.cpp Full Diff Summary

Our `dx8wrapper.cpp` (4563 lines) vs fighter19 (4456 lines) — 107 extra lines, mostly:

| Category | Our additions | Risk |
|----------|--------------|------|
| Debug logging | `fprintf(stderr, "DEBUG: ...")` in Init, Create_Device | None (info only) |
| DbgHelpGuard | RAII guard in Init/Create_Device | None (TheSuperHackers bugfix) |
| module_compat.h | `#include "module_compat.h"` for LoadLibrary compat | None (needed for Linux) |
| Resize_And_Position_Window | Extracted method with SDL3 path | None (good refactor) |
| DX8WebBrowser guards | `#ifdef _WIN32` around web browser in Begin/End_Scene | None (correct isolation) |
| Set_World/View_Identity | Uses `Matrix4x4::Make_Identity()` | Low (functionally equivalent) |
| Transform init | `memset` vs explicit loop with `Vector4::Set` | Low (functionally equivalent) |

No high-risk differences in dx8wrapper.cpp.

---

## GDB Crash Note (Separate Issue)

The GDB log (`logs/gdb.log~`) shows a **SIGSEGV during Vulkan initialization** — specifically in `llvm::Regex::Regex()` called from `libvulkan_lvp.so` during `vkEnumerateInstanceExtensionProperties()`. This happens inside `SDL3GameEngine::init()` before any rendering occurs.

This is a **Lavapipe (software Vulkan) driver bug**, not a game code issue. It's triggered when SDL3 enumerates Vulkan ICDs and loads `libvulkan_lvp.so`. The fix is to either:
1. Set `VK_ICD_FILENAMES` to point only to hardware ICD (e.g., Intel, AMD)
2. Uninstall the `libvulkan_lvp.so` Mesa software renderer
3. Set `VK_LOADER_DRIVERS_DISABLE=*lvp*`

This crash is **separate** from the magenta screen issue.

---

## Recommended Fix Order

1. **Remove double-draw** (Finding 1) — Delete lines 1033-1039 in GameEngine.cpp
2. **Verify m_headless** (Finding 3) — Add debug print, confirm it's FALSE
3. **Check FramePacer** (Finding 2) — Verify `isGameHalted()`/`isTimeFrozen()` aren't blocking updates
4. **Fix GetBackBuffer** (Finding 4) — Add `DX8CALL_HRES` error check
5. If still magenta: check VFS/BIG file loading, texture format enumeration in dx8caps

---

## Files Analyzed

| File | Location (ours) | Location (fighter19) | Diff Result |
|------|-----------------|---------------------|-------------|
| dx8wrapper.cpp | GeneralsMD/Code/Libraries/.../WW3D2/ | GeneralsMD/Code/Libraries/.../WW3D2/ | +107 lines, mostly debug/compat |
| texture.cpp | Core/Libraries/.../WW3D2/ | GeneralsMD/Code/Libraries/.../WW3D2/ | Cosmetic only |
| textureloader.cpp | Core/Libraries/.../WW3D2/ | GeneralsMD/Code/Libraries/.../WW3D2/ | Cosmetic only |
| surfaceclass.cpp | Core/Libraries/.../WW3D2/ | GeneralsMD/Code/Libraries/.../WW3D2/ | API signature changes |
| missingtexture.cpp | Core/Libraries/.../WW3D2/ | GeneralsMD/Code/Libraries/.../WW3D2/ | static_assert added |
| dx8texman.cpp | Core/Libraries/.../WW3D2/ | GeneralsMD/Code/Libraries/.../WW3D2/ | Whitespace only |
| dx8caps.cpp | GeneralsMD/Code/Libraries/.../WW3D2/ | GeneralsMD/Code/Libraries/.../WW3D2/ | VMware vendor, style |
| ww3d.cpp | GeneralsMD/Code/Libraries/.../WW3D2/ | GeneralsMD/Code/Libraries/.../WW3D2/ | **IDENTICAL** |
| ddsfile.cpp | Core/Libraries/.../WW3D2/ | GeneralsMD/Code/Libraries/.../WW3D2/ | **IDENTICAL** |
| W3DDisplay.cpp | GeneralsMD/Code/GameEngineDevice/.../W3DDevice/ | Same | Headless guards, FramePacer, SDL3 |
| GameEngine.cpp | GeneralsMD/Code/GameEngine/.../Common/ | Same | Double-draw bug, FramePacer |
| GameClient.cpp | GeneralsMD/Code/GameEngine/.../GameClient/ | Same | Normal draw chain (reference) |
