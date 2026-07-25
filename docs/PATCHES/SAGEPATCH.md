# SagePatch — Casual QoL Extras for GeneralsX

SagePatch is an optional, drop-in patch that adds quality-of-life features to
GeneralsX without modifying the game source. Inspired by the casual subset of
GenTool, it ships as a separate shared library that is loaded via the platform's
preload mechanism plus a small `GameData` INI override that the engine picks up
at startup.

## Platform support

| Platform | Mechanism | Status |
|---|---|---|
| **macOS** | `DYLD_INSERT_LIBRARIES` + `__DATA,__interpose` | ✅ Phase 1 |
| **Linux** | `LD_PRELOAD` + `dlsym(RTLD_NEXT, …)` | ✅ Phase 1 (X11 only for brightness) |
| **Windows** | proxy DLL pattern (deferred) | ⚠️ stubs build but no-op at runtime |

Windows is deferred because Win32 has no native preload mechanism — it would
need either a proxy `d3d8.dll` (the original GenTool's pattern) or in-process
inline hooking via Detours/MinHook. Both are larger than the macOS/Linux
implementations and were skipped to ship the rest. Windows users still have
the original GenTool available.

## Features

| Feature | Trigger | Notes |
|---|---|---|
| Screenshot | `F11` | PNG saved to `~/Pictures/GeneralsX/`. macOS: `screencapture`. Linux: ImageMagick `import` (X11) or `gnome-screenshot`. |
| Cursor lock toggle | `Scroll Lock` | Confines mouse to the game window. SDL3 — works on every platform. |
| Brightness up | `Ctrl + Page Up` | +8 step on the gamma curve, range −128…+128. |
| Brightness down | `Ctrl + Page Down` | −8 step. macOS: CoreGraphics. Linux: XF86VidMode (X11 only — no-op under Wayland). |
| Window snap: center | `Ctrl + 1` | SDL3 `SDL_SetWindowPosition` to display center. |
| Window snap: top-left | `Ctrl + 2` | |
| Window snap: top-right | `Ctrl + 3` | |
| Window snap: bottom-left | `Ctrl + 4` | |
| Window snap: bottom-right | `Ctrl + 5` | |
| Camera zoom range | (passive) | `MaxCameraHeight=800`, `MinCameraHeight=60`, `EnforceMaxCameraHeight=No`. |
| Camera pitch | (passive) | `CameraPitch=50` (vanilla ~63). |
| Keyboard scroll speed | (passive) | `KeyboardScrollSpeedFactor=1.0` (vanilla 0.5). |

Hot-key collisions: SagePatch eats the events it handles, so they do not
also reach the game.

### About FPS counters

The engine already ships a native FPS overlay at the top-left
(`W3DDisplay::drawFPSStats()`), gated by `#ifdef RTS_DEBUG` plus the runtime
`-benchmark <seconds>` CLI flag. SagePatch does not duplicate it. An earlier
revision of this patch defaulted `DXVK_HUD=fps` in the run wrapper as a
release-build alternative, but on macOS 26 the current MoltenVK SPIRV-Cross
back-end cannot translate DXVK's HUD pipeline shader (uses `DrawIndex`, no
MSL equivalent), causing the swap-chain blit pipeline to fail and the game
to hang at the EA logo. The default is now `DXVK_HUD=0` again. Users who
want a frame counter on macOS should either build with `RTS_DEBUG=ON` and
launch with `-benchmark 9999`, or set `DXVK_HUD=fps` themselves once
upstream MoltenVK ships the DrawIndex emulation patch.

## How to enable

### macOS

```bash
cmake --preset macos-vulkan -DRTS_BUILD_OPTION_SAGE_PATCH=ON
cmake --build build/macos-vulkan --target z_generals -j$(sysctl -n hw.logicalcpu)
./scripts/build/macos/deploy-macos-zh.sh
~/GeneralsX/GeneralsZH/run.sh -win
```

(The `macos-vulkan` preset already sets the flag `ON`.)

### Linux

```bash
cmake --preset linux64-deploy -DRTS_BUILD_OPTION_SAGE_PATCH=ON
cmake --build build/linux64-deploy --target z_generals -j$(nproc)
./scripts/build/linux/deploy-linux-zh.sh
~/GeneralsX/GeneralsZH/run.sh -win
```

(The `linux64-deploy` preset does **not** set the flag automatically — opt in
explicitly.)

### Disabling at runtime (no rebuild)

```bash
SAGE_PATCH_DISABLED=1 ~/GeneralsX/GeneralsZH/run.sh -win
```

This skips the preload step. The INI override remains active — delete
`Data/INI/GameData/SagePatch.ini` to revert camera/scroll values.

## Architecture

```
Game process (GeneralsXZH)
    │
    ├── DYLD_INSERT_LIBRARIES (macOS) / LD_PRELOAD (Linux) → libsage_patch.{dylib,so}
    │       │
    │       └── SDL_PollEvent gets replaced (interpose table on macOS,
    │           symbol override + dlsym RTLD_NEXT on Linux)
    │              │
    │              └── F11, Scroll Lock, Ctrl+PgUp/Dn, Cmd+Option+1..5 → SagePatch handlers
    │                      │
    │                      └── Per-platform: screencapture / ImageMagick,
    │                          CoreGraphics gamma / XF86VidMode, SDL_SetWindowPosition
    │
    └── Engine loads Data/INI/GameData/SagePatch.ini → camera/scroll overrides
```

No D3D8 proxy, no Vulkan layer, no engine source modifications.

## Why this approach

The original GenTool had to be a `d3d8.dll` proxy because Windows games of that
era exposed no other plugin surface. On macOS and Linux we have:

- **Symbol override at load time** — `__DATA,__interpose` (macOS) and
  `LD_PRELOAD` (Linux) replace `SDL_PollEvent` with our version without
  touching the host SDL3 library.
- **OS-native window capture** — both platforms expose tools that snapshot
  a single window without graphics-pipeline hooks.
- **Engine INI overrides** — the GeneralsX INI loader merges files in
  `Data/INI/Default/<Subsystem>/`, so parameter tweaks need zero code.

This keeps SagePatch ~600 lines instead of the ~3000-line full COM proxy
that a Windows-style implementation would require.

## Already in the engine — no patch needed

These GenTool-era options are already first-class engine command-line flags
in GeneralsX/TheSuperHackers (see `Common/CommandLine.cpp`). Use them
directly with `run.sh`:

| Flag | Effect |
|---|---|
| `-nologo` | Skip the EA / Westwood intro |
| `-noShellAnim` | Skip the animated main-menu camera |
| `-noshellmap` | Skip the animated background map |
| `-quickstart` | Combined fast-boot |
| `-xres N -yres N` | Any resolution (the engine no longer locks the list) |
| `-forcefullviewport` | Full viewport on UI mods like Control Bar Pro |
| `-win` / `-fullscreen` | Window mode |
| `-noaudio` / `-nomusic` / `-novideo` | Disable subsystems |

Game-engine bug fixes from the GenTool changelog (Scud bug, Tunnel bug,
Building bug, multiplayer movement crash, etc.) are handled **upstream by
TheSuperHackers/GeneralsGameCode**, which is the parent of GeneralsX and
maintained by the same author who wrote GenTool (xezon). The codebase
already carries 170+ `@bugfix` annotations including the Tunnel System
fixes by xezon. SagePatch does **not** duplicate those — duplicating fixes
would only create merge conflicts when GeneralsX next syncs with upstream.

## Limits / what's *not* in scope

By design, SagePatch sticks to **casual** QoL only:

- No replay tools (frame stepping, fog-of-war replay, controls bar)
- No competitive features (Money Display, Player Table, Random Balance)
- No anti-cheat (MDS, Game File Validator, version validation)
- No external server integrations (CNC Online, GameRanger, Upload Mode, ticker,
  ranked maps, ladder, GenTool updater)
- No engine bug fixes — those live in core source, gated by the existing
  `@bugfix` annotation system, contributed upstream where possible
- No in-game text overlay (clock, match timer, in-game settings menu) — these
  need a graphics-pipeline hook (D3D8 proxy or Vulkan layer); see the FPS
  counter via `DXVK_HUD` for an existing alternative

## File layout

```
Patches/SagePatch/
    CMakeLists.txt
    include/SagePatch/{Hooks.h, Features.h, Logger.h}
    src/
      common/                          # SDL3 — works on every platform
        Init.cpp
        KeyHandler.cpp
        CursorLock.cpp
        WindowPosition.cpp
      macos/                           # macOS-only: __DATA,__interpose, screencapture, CoreGraphics
        interposers_macos.cpp
        Screenshot_macos.cpp
        Brightness_macos.cpp
      linux/                           # Linux-only: LD_PRELOAD, ImageMagick, XF86VidMode
        interposers_linux.cpp
        Screenshot_linux.cpp
        Brightness_linux.cpp
      windows/                         # Windows: stubs only (Phase 2)
        Stubs_windows.cpp
    resources/Override.ini             # engine-side INI override (deployed to Data/INI/GameData/SagePatch.ini)
docs/PATCHES/SAGEPATCH.md              # this file
```

### A note on the INI override path

The engine loads `GameData` INIs in two passes:
`Data/INI/Default/GameData/...` first, then `Data/INI/GameData/...`. Each
`GameData` block parsed overwrites the previous values in `TheWritableGlobalData`
(`INI_LOAD_OVERWRITE` semantics). The vanilla camera defaults
(`MaxCameraHeight = 310`, etc.) live in the BIG-archived `Data/INI/GameData.ini`
which is parsed in the **second** pass, so an override placed in
`Data/INI/Default/GameData/` is silently undone by the second pass. The deploy
scripts therefore drop our `SagePatch.ini` into `Data/INI/GameData/` so it is
loaded last and the values stick.

```text
```
