# Phase 0: fighter19 DXVK Integration Analysis

**Created**: 2026-02-07
**Status**: In Progress
**Reference**: `references/old-refs/fighter19-dxvk-port/`

## Objective

Understand fighter19's DXVK integration approach and confirm the hypothesis:
- **Abstraction layer created, core code untouched**
- DXVK translates DX8→Vulkan at runtime
- SDL3 replaces Win32 windowing
- Platform-specific code isolated

## Key Hypothesis to Confirm

> fighter19 did NOT modify core game logic. Instead, created wrapper/abstraction layers to intercept DX8 calls and translate to DXVK/SDL3.

If true, we can follow the same pattern for Zero Hour.

## Research Questions

1. **Where is the DXVK wrapper layer?**
   - Expected: `Core/GameEngineDevice/` modifications
   - Check for DXVK header includes
   - Check for compile-time switches (`#ifdef BUILD_WITH_DXVK`)

2. **How is SDL3 integrated?**
   - Window creation replacement
   - Input handling
   - Event loop modifications

3. **What files were changed?**
   - List all modified files
   - Categorize: Platform layer vs Core logic
   - Zero Hour applicability notes

4. **How is platform selection handled?**
   - Compile flags
   - CMake presets
   - Runtime detection

## Investigation Plan

### Step 1: Survey Changes
- [ ] Use `cognitionai/deepwiki` to query fighter19 repo
- [ ] Ask: "Where is the DXVK wrapper implemented?"
- [ ] Ask: "How is SDL3 integrated without touching core code?"
- [ ] Ask: "What files were modified for Linux port?"

### Step 2: Analyze DXVK Layer
- [ ] Document wrapper architecture
- [ ] Identify DX8→DXVK translation points
- [ ] Check for existing abstraction interfaces

### Step 3: Analyze SDL3 Integration
- [ ] Document window creation changes
- [ ] Document input handling changes
- [ ] Identify Win32 replacement patterns

### Step 4: Evaluate Zero Hour Applicability
- [ ] Confirm: Both Generals and Zero Hour covered
- [ ] List potential gotchas
- [ ] Document differences (if any)

## Findings

### DXVK Wrapper Architecture ✅ CONFIRMED

**Abstraction Pattern**: `DX8Wrapper` class (thin insulation layer)
- **Location**: `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/dx8wrapper.cpp/h`
- **Approach**: Intercepts DirectX 8 calls WITHOUT modifying core game logic
- **Runtime Loading**: Dynamically loads `libdxvk_d3d8.so` on Linux
- **Function**: `DX8Wrapper::Init()` loads DXVK, obtains `Direct3DCreate8` pointer
- **Design**: Direct wrapping (not high-level abstraction), preserves DX8 API surface

**Key Files Modified**:
- `CMakeLists.txt` - Fetch DXVK library
- `GeneralsMD/Code/CMakeLists.txt` - Include DXVK headers, install runtime dependencies
- `GeneralsMD/Code/GameEngine/CMakeLists.txt` - Add DXVK include directories
- `GeneralsMD/Code/CompatLib/CMakeLists.txt` - Define `d3d8lib` interface, link DXVK
- `dx8wrapper.cpp/h` - Implement wrapper class with dynamic loading
- `SDL3Main.cpp` - Set `DXVK_WSI_DRIVER="SDL3"`, load Vulkan library

**Build System**: `FetchContent_Declare` downloads `dxvk-native-2.6` from GitHub

### SDL3 Integration Points ✅ CONFIRMED

**Abstraction Pattern**: Inheritance-based abstraction
- **Base Classes**: `Mouse`, `Keyboard`, `GameEngine` (abstract interfaces)
- **SDL3 Implementations**: `SDL3Mouse`, `SDL3Keyboard`, `SDL3GameEngine` (concrete)
- **Location**: `GameEngineDevice/SDL3Device/` subdirectories
- **Core Isolation**: Game logic interacts with abstract interfaces ONLY

**Key Components**:
- `SDL3GameEngine::initializeAppWindows()` - Creates SDL3 window via `SDL_CreateWindow`
- `SDL3GameEngine::serviceWindowsOS()` - Polls events via `SDL_PollEvent`, dispatches to input handlers
- `SDL3Mouse::addSDLEvent()` - Buffers SDL3 mouse events
- `SDL3Mouse::getMouseEvent()` - Translates to internal `MouseIO` format
- `SDL3Keyboard::addSDLEvent()` - Buffers SDL3 keyboard events
- `SDL3Keyboard::getKey()` - Translates SDL keycodes to `KeyDefType`
- `wnd_compat.cpp` - Win32 API compatibility layer (e.g., `SetWindowPos` → SDL3)

**Compilation**: `SAGE_USE_SDL3` flag controls SDL3 vs Win32 device backend

### Changed Files List
**Build System**:
- `CMakeLists.txt` - DXVK fetching
- `GeneralsMD/Code/CMakeLists.txt` - DXVK headers/runtime
- `GameEngineDevice/CMakeLists.txt` - Conditional SDL3 compilation

**Graphics Layer**:
- `Libraries/Source/WWVegas/WW3D2/dx8wrapper.cpp/h` - DXVK wrapper

**Windowing/Input Layer**:
- `GameEngineDevice/SDL3Device/*` - SDL3 implementations
- `CompatLib/wnd_compat.cpp` - Win32→SDL3 translation
- `Main/SDL3Main.cpp` - Entry point, DXVK/SDL3 initialization

### Build System Configuration ✅ DOCUMENTED

**CMake Options** (CMakeLists.txt):
- `SAGE_USE_DX8` - Use DirectX 8 (Windows, default ON)
- `SAGE_USE_SDL3` - Use SDL3 windowing (Linux, OFF by default)
- `SAGE_USE_OPENAL` - Use OpenAL audio (conditional on FFMPEG)
- `SAGE_USE_MILES` - Use Miles Sound System (default ON)

**Dependency Fetching**:
```cmake
if(SAGE_USE_DX8)
  # Windows: Fetch min-dx8-sdk from GitHub
else()
  # Linux: Fetch DXVK Native v2.6 from doitsujin/dxvk releases
  FetchContent_Declare(
    dxvk
    URL https://github.com/doitsujin/dxvk/releases/download/v2.6/dxvk-native-2.6-steamrt-sniper.tar.gz
  )
endif()
```

**CMakePresets.json** - Key Presets:

1. **`linux64-deploy`** (PRIMARY LINUX TARGET):
   - Generator: Ninja
   - Build Type: RelWithDebInfo
   - vcpkg triplet: `x64-linux-dynamic`
   - **Sets**: `SAGE_USE_SDL3: ON` (enables SDL3)
   - **Sets**: `SAGE_USE_OPENAL: ON` (OpenAL audio)
   - **Sets**: `SAGE_USE_FFMPEG: ON` (video codec)

2. **`win32-deploy`** (Windows baseline):
   - Generator: Visual Studio 17 2022
   - Architecture: Win32
   - Uses DX8 + Miles (default)

3. **`linux64-testing`** (Debug variant):
   - Same as deploy but Debug build type
   - Includes test building

**Platform Definitions**:
- Windows: `-D_WINDOWS -DWINVER=0x400 -D_WIN32_WINNT=0x0400`
- Linux: `-D_LINUX -D_UNIX`
- macOS: `-D_MACOSX -D_UNIX`

## Suspected Abstraction Pattern

Based on generalsx.instructions.md, expected pattern:
```cpp
// Core/GameEngineDevice/Include/w3dgraphicsdevice.h
#ifdef BUILD_WITH_DXVK
    #include "dxvk_adapter.h"
#else
    #include <d3d8.h>
#endif
```

This keeps Windows DX8 path intact while enabling Linux DXVK path.

### Zero Hour Specific Concerns ✅ VERIFIED

- [X] **Verified**: fighter19 port FULLY covers Zero Hour
  - `SAGE_BUILD_ZEROHOUR` option in CMakeLists.txt
  - `GeneralsMD/` directory contains Zero Hour code
  - All DXVK/SDL3 changes apply to both Generals and Zero Hour
- [X] **Confirmed**: Expansion-specific rendering handled correctly
  - Same `dx8wrapper.cpp` for both games
  - No Zero Hour-only DX8 usage identified
- [X] **Assessment**: Direct port to our Zero Hour codebase is viable

## Summary: What We Need to Port

### Minimal DXVK Integration (Core Change)
**File**: `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/dx8wrapper.cpp` (line ~297)
```cpp
// BEFORE (our code):
D3D8Lib = LoadLibrary("D3D8.DLL");

// AFTER (add ifdef):
#ifdef _WIN32
    D3D8Lib = LoadLibrary("D3D8.DLL");
#else
    D3D8Lib = LoadLibrary("libdxvk_d3d8.so");
#endif
```
**That's the ONLY change to dx8wrapper.cpp!** ✅

### SDL3 Integration (New Files)
**Entry Point**:
- `GeneralsMD/Code/Main/SDL3Main.cpp` - Replaces WinMain for Linux
  - Sets `DXVK_WSI_DRIVER=SDL3` environment variable
  - Loads Vulkan library
  - Creates window with `SDL_WINDOW_VULKAN` flag

**Device Layer** (new directory: `GameEngineDevice/SDL3Device/`):
- `SDL3GameEngine.{h,cpp}` - Extends `GameEngine` base class
- `SDL3Mouse.{h,cpp}` - Extends `Mouse` base class
- `SDL3Keyboard.{h,cpp}` - Extends `Keyboard` base class
- `SDL3CDManager.{h,cpp}` - CD handling (stub on Linux)
- `SDL3IMEManager.{h,cpp}` - Input Method Editor
- `SDL3OSDisplay.{h,cpp}` - Display management

### Build System Changes
**CMakeLists.txt**:
1. Add `SAGE_USE_SDL3` option
2. Add DXVK FetchContent (when NOT `SAGE_USE_DX8`)
3. Conditional compilation of SDL3Device sources

**CMakePresets.json**:
1. Add `linux64-deploy` preset with `SAGE_USE_SDL3: ON`
2. Set vcpkg triplet to `x64-linux-dynamic`

### Audio Status (Phase 2)
fighter19 uses **BOTH** Miles (Windows) AND OpenAL (Linux) via conditional compilation:
- `SAGE_USE_MILES` - Windows default
- `SAGE_USE_OPENAL` - Linux option (requires adaptation from jmarshall)

## Notes

- fighter19 has WORKING graphics on Linux (native ELF) ✅
- Audio backend switchable (Miles vs OpenAL) ✅
- Zero Hour fully supported ✅
- This is our PRIMARY reference for graphics layer ✅
- **Complexity**: Lower than expected - dx8wrapper.cpp needs ONE ifdef change!

## Next Steps

1. Query fighter19 repo via DeepWiki
2. Confirm abstraction layer approach
3. Document wrapper pattern
4. List all changed files with analysis
