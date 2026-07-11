# Session 36 - Undefined References Analysis

**Date**: 13 February 2026  
**Status**: Compilation 100% Complete, Linking Blocked  
**Total Undefined**: 43+ references across 10 functional groups

## Executive Summary

After fixing Windows library linking (binkstub/milesstub), OpenAL integration, and RegistryClass stubs, **ALL C++ source files compile successfully on Linux**. Only final executable linking is blocked by platform-specific undefined symbols.

**Critical Finding**: Most remaining symbols require **REAL implementation** (not stubs) for game functionality.

---

## Category 1: CRITICAL - Command Line Arguments

### Symbols
```
__argc
__argv
```

### Occurrences
- `CommandLine.cpp:1413` (3 references)

### What It Does
Global variables for command line argument count and array. Used by `parseCommandLine()` to process game launch parameters like `-win`, `-noshellmap`, `-mod`, etc.

### Current State
- **Windows**: Compiler provides `__argc`/`__argv` globals automatically
- **Linux**: C standard uses `argc`/`argv` from `main()` - no globals

### STUB or REAL?
**REAL IMPLEMENTATION REQUIRED** ⚠️

Without this, game cannot:
- Enter windowed mode (`-win`)
- Skip intro (`-noshellmap`)
- Load mods (`-mod`)
- Debug options (`-quickstart`)

### Solution Strategy
**Option A**: Store main() args in globals (fighter19 approach):
```cpp
// SDL3Main.cpp
int __argc = 0;
char** __argv = nullptr;

int main(int argc, char* argv[]) {
    __argc = argc;
    __argv = argv;
    // ... rest of startup
}
```

**Option B**: Refactor CommandLine.cpp to accept args (jmarshall approach):
```cpp
// Change signature
void parseCommandLine(const CommandLineParam* params, int argc, char** argv);
```

**Recommended**: Option A (minimal change, maintains Windows compatibility)

### References to Check
- `references/old-refs/fighter19-dxvk-port/GeneralsMD/Code/Main/` - SDL3Main.cpp globals
- `references/old-refs/jmarshall-win64-modern/Code/Main/` - Modern refactor approach

---

## Category 2: CRITICAL - Window Handle Global

### Symbols
```
ApplicationHWnd
```

### Occurrences
- `Debug.cpp:170` - Debug window parent
- `Debug.cpp:754` - Crash dialog parent
- `Debug.cpp:804` - Message box parent
- `W3DDisplay.cpp:717` - Display initialization

### What It Does
Global window handle (`HWND` on Windows) pointing to the main application window. Used for:
- Modal dialogs (parent window)
- Debug output windows
- DirectX device creation

### Current State
- **Windows**: Win32 API creates HWND during WindowProc initialization
- **Linux**: SDL3 provides `SDL_Window*` handle, not HWND

### STUB or REAL?
**REAL IMPLEMENTATION REQUIRED** ⚠️

Without this, game cannot:
- Create display device (CRITICAL!)
- Show error dialogs
- Debug output

### Solution Strategy
**Option A**: Typedef ApplicationHWnd to SDL_Window* (fighter19):
```cpp
// SDL3Main.cpp
#ifdef _UNIX
typedef SDL_Window* HWND;
#endif

HWND ApplicationHWnd = nullptr;

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    ApplicationHWnd = SDL_CreateWindow("Generals Zero Hour", ...);
}
```

**Option B**: Create HWND wrapper:
```cpp
// Separate SDL window from HWND stub
void* ApplicationHWnd = nullptr; // Dummy for Linux
SDL_Window* g_mainWindow = nullptr;
```

**Recommended**: Option A (matches fighter19 working implementation)

### References to Check
- `references/old-refs/fighter19-dxvk-port/GeneralsMD/Code/Main/SDL3Main.cpp` - Window creation
- `references/old-refs/fighter19-dxvk-port/GeneralsMD/Code/GameEngineDevice/Source/W3DDisplay.cpp` - Usage patterns

---

## Category 3: CRITICAL - Game Text File Globals

### Symbols
```
g_csfFile
g_strFile
```

### Occurrences
- `GameText.cpp:291` - CSF file handle
- `GameText.cpp:310` - STR file handle
- `GameText.cpp:340` - STR file handle (fallback)

### What It Does
Global file handles for game localization:
- **CSF**: Command & Conquer String File (audio labels, campaign text)
- **STR**: String Table Resource (UI strings, tooltips, menus)

Without these, **ALL game text appears as codes** (e.g., `GUI:MainMenu` instead of "Main Menu").

### Current State
- **Windows**: Files opened via Win32 `CreateFile()`, stored in globals
- **Linux**: Need FileSystem abstraction (StdLocalFileSystem already exists!)

### STUB or REAL?
**REAL IMPLEMENTATION REQUIRED** ⚠️

Without this, game is **completely unplayable** - no UI text visible!

### Solution Strategy
**Option A**: Initialize globals in GameTextManager::init():
```cpp
// GameText.cpp - Around line 291
#ifdef _UNIX
File* g_csfFile = nullptr;
File* g_strFile = nullptr;
#endif

void GameTextManager::init() {
    FileSystem* fs = TheFileSystem;
    g_csfFile = fs->openFile("data/generals.csf", File::READ);
    g_strFile = fs->openFile("data/generals.str", File::READ);
}
```

**Option B**: Refactor to class members (cleaner but bigger change):
```cpp
class GameTextManager {
    File* m_csfFile;
    File* m_strFile;
};
```

**Recommended**: Option A (maintains current architecture)

### References to Check
- Fighter19: Check if CSF/STR loading works (may be stubbed)
- jmarshall: Likely has this working (base game needs text!)
- TheSuperHackers upstream: Check if globals are in header or cpp

---

## Category 4: MEDIUM - OS Display Functions

### Symbols
```
OSDisplaySetBusyState(bool, bool)
OSDisplayWarningBox(AsciiString, AsciiString, unsigned int, unsigned int)
```

### Occurrences
- `OSDisplaySetBusyState`: 10+ references in GameLogic.cpp
- `OSDisplayWarningBox`: GameAudio.cpp:254

### What It Does
- **SetBusyState**: Changes cursor to hourglass/spinner during loading
- **WarningBox**: Shows modal alert dialog

### Current State
- **Windows**: Uses Win32 `SetCursor()` and `MessageBox()`
- **Linux**: SDL has cursor functions, no built-in message box

### STUB or REAL?
**STUB ACCEPTABLE** ✓ (non-critical)

Game can run without these:
- Missing cursor changes: Annoying but not breaking
- Missing warning box: stderr output works

### Solution Strategy
**Phase 1**: Empty stubs:
```cpp
// CompatLib/Source/wnd_compat.cpp
void OSDisplaySetBusyState(bool busy, bool force) {
    // TODO Phase 2: SDL_SetCursor(SDL_SYSTEM_CURSOR_WAIT)
}

void OSDisplayWarningBox(AsciiString title, AsciiString msg, unsigned int, unsigned int) {
    fprintf(stderr, "WARNING: %s - %s\n", title.Str(), msg.Str());
}
```

**Phase 2**: Real SDL implementation after game launches.

**Recommended**: Start with stubs, upgrade later.

### References to Check
- `references/old-refs/fighter19-dxvk-port/` - Check if implemented or stubbed
- SDL3 docs: `SDL_SetCursor()`, SDL3 has no native message box

---

## Category 5: LOW - Internet Explorer Browser

### Symbols
```
DX8WebBrowser::Initialize(...)
DX8WebBrowser::Shutdown()
```

### Occurrences
- `W3DDisplay.cpp:848` - Initialize
- `W3DDisplay.cpp:479` - Shutdown

### What It Does
Embeds Internet Explorer browser control for in-game web content (news, updates, online features).

**Legacy feature from 2003** - not essential for offline play.

### Current State
- **Windows**: Uses ActiveX IWebBrowser2 control
- **Linux**: No IE, would need WebKitGTK or similar

### STUB or REAL?
**STUB ACCEPTABLE** ✓ (legacy feature)

Game runs fine without browser:
- Offline skirmish: No impact
- Campaign: No impact
- Online lobby: May show blank panel (not critical)

### Solution Strategy
**Phase 1**: Empty stub class:
```cpp
// CompatLib/Source/browser_compat.cpp
class DX8WebBrowser {
public:
    static bool Initialize(const char*, const char*, const char*, const char*) {
        return false; // Browser unavailable
    }
    static void Shutdown() {}
};
```

**Phase 2+**: Investigate if WebKitGTK integration worthwhile.

**Recommended**: Stub indefinitely (low priority).

### References to Check
- Fighter19: Likely stubbed (no Linux browser)
- jmarshall: May have stub or refactor

---

## Category 6: LOW - CD-ROM Check

### Symbols
```
CreateCDManager()
```

### Occurrences
- `GameEngine.cpp:532` - Engine initialization

### What It Does
Checks for game CD in drive (2003 copy protection).

**Completely obsolete** - digital distribution era.

### Current State
- **Windows**: Win32 `GetDriveType()` API
- **Linux**: Irrelevant

### STUB or REAL?
**STUB ACCEPTABLE** ✓ (obsolete feature)

Modern game doesn't ship on CD!

### Solution Strategy
```cpp
// CompatLib/Source/cd_compat.cpp
void* CreateCDManager() {
    return nullptr; // No CD check on modern systems
}
```

Return `nullptr` - game handles missing CD manager gracefully (skip check).

**Recommended**: Permanent stub.

### References to Check
- Fighter19/jmarshall: Both likely stub this

---

## Category 7: CRITICAL - GameEngine Factory Methods

### Symbols
```cpp
GameEngine::createLocalFileSystem()
GameEngine::createArchiveFileSystem()
GameEngine::createGameLogic()
GameEngine::createGameClient()
GameEngine::createModuleFactory()
GameEngine::createThingFactory()
GameEngine::createFunctionLexicon()
GameEngine::createRadar()
GameEngine::createWebBrowser()
GameEngine::createParticleSystemManager()
```

### Occurrences
All in `SDL3GameEngine.cpp:302-386` - calling base class virtual methods

### What It Does
**Virtual factory pattern** - base `GameEngine` class declares pure virtual methods, derived class (e.g., `W3DGameEngine`) implements them to create game-specific subsystems.

### Current State
- **Windows**: Uses `W3DGameEngine` derived class (DirectX-based)
- **Linux**: Created new `SDL3GameEngine` but didn't implement factories

### STUB or REAL?
**REAL IMPLEMENTATION REQUIRED** ⚠️

These are **core engine initialization**! Without them:
- No file system = can't load game data
- No game logic = no gameplay
- No client = no rendering

**GAME CANNOT START!**

### Solution Strategy
**Option A**: SDL3GameEngine calls base implementations:
```cpp
// SDL3GameEngine.cpp
FileSystem* SDL3GameEngine::createLocalFileSystem() {
    return GameEngine::createLocalFileSystem(); // Calls parent
}
```

**Option B**: SDL3GameEngine provides own implementations:
```cpp
FileSystem* SDL3GameEngine::createLocalFileSystem() {
    return new StdLocalFileSystem(); // Linux-specific
}
```

**Critical Question**: Why are these undefined?

**Hypothesis**: `SDL3GameEngine.cpp` exists but doesn't link `GameEngine.cpp` base implementations OR base methods are pure virtual (=0).

**Investigation Needed**:
1. Check if `GameEngine::create*` methods exist in base class
2. Check if they're pure virtual or have default implementations
3. Verify SDL3GameEngine header declares overrides

**Recommended**: Investigate base class first, then implement missing methods.

### References to Check
- `Core/GameEngine/Source/Common/GameEngine.cpp` - Base implementations
- `GeneralsMD/Code/GameEngine/Include/Common/GameEngine.h` - Virtual declarations
- `references/old-refs/fighter19-dxvk-port/` - How fighter19 handled SDL3GameEngine

---

## Category 8: MEDIUM - Font Rendering

### Symbols
```
FontCharsClass::Update_Current_Buffer(int)
```

### Occurrences
- `render2dsentence.cpp:1879` - FreeType character caching

### What It Does
Updates GPU texture buffer with newly rasterized font glyphs. Part of FreeType font rendering pipeline.

### Current State
- **Windows**: Updates DirectX texture buffer
- **Linux**: Needs DXVK/OpenGL texture update

### STUB or REAL?
**REAL IMPLEMENTATION REQUIRED** ⚠️

Without this:
- Text renders as placeholder boxes
- UI becomes unreadable

### Solution Strategy
**Investigation Needed**: Check if method is:
1. **Declared but not defined** - need to implement
2. **Defined in Windows-only file** - need Linux version
3. **Missing from build** - need to add source file

Likely scenario: Method exists but in Windows-only section of WW3D2.

**Recommended**: Search codebase for definition, check fighter19/jmarshall ports.

### References to Check
- `Core/Libraries/Source/WWVegas/WW3D2/` - Font rendering files
- Fighter19: Check if font rendering works
- jmarshall: Likely has working FreeType integration

---

## Category 9: MEDIUM - Registry Functions (std::string)

### Symbols
```cpp
GetStringFromRegistry(std::string, std::string, std::string&)
GetUnsignedIntFromRegistry(std::string, std::string, unsigned int&)
SetStringInRegistry(std::string, std::string, std::string)
SetUnsignedIntInRegistry(std::string, std::string, unsigned int)
```

### Occurrences
- `urlBuilder.cpp:38-41` (WWDownload) - 4 references
- `MainMenuUtils.cpp:161-860` (GameSpy) - 3 references
- `OptionsMenu.cpp` - 2 references
- `SkirmishGameOptionsMenu.cpp` - 2 references
- `PopupPlayerInfo.cpp` - 1 reference

### What It Does
Registry access functions using `std::string` instead of C strings. Used for:
- Online server URLs
- Player preferences
- Battle honors tracking

### Current State
**GOOD NEWS**: Stubs already exist!

```cpp
// Core/Libraries/Source/WWVegas/WWDownload/registry.cpp (line ~175)
#else // _WIN32 - Linux: No registry support

bool getStringFromRegistry(HKEY, std::string, std::string, std::string&) { return false; }
bool getUnsignedIntFromRegistry(HKEY, std::string, std::string, unsigned int&) { return false; }
bool setStringInRegistry(HKEY, std::string, std::string, std::string) { return false; }
bool setUnsignedIntInRegistry(HKEY, std::string, std::string, unsigned int) { return false; }
```

**Problem**: Linker can't find `GetStringFromRegistry` (capital G) - different signature?

### STUB or REAL?
**INVESTIGATE LINKING ISSUE** 🔍

Stubs exist but aren't being linked. Likely causes:
1. **Name mismatch**: `getStringFromRegistry` vs `GetStringFromRegistry` (case!)
2. **Signature mismatch**: HKEY parameter vs no HKEY
3. **Namespace issue**: Functions in wrong namespace
4. **CMake issue**: WWDownload lib not linked to consumers

### Solution Strategy
**Step 1**: Find declarations of `GetStringFromRegistry`:
```bash
grep -r "GetStringFromRegistry" --include="*.h"
```

**Step 2**: Compare signatures:
```cpp
// WWDownload stub:
bool getStringFromRegistry(HKEY, std::string, std::string, std::string&);

// Caller expects:
bool GetStringFromRegistry(std::string, std::string, std::string&);
```

**Step 3**: Fix signatures OR add wrapper functions.

**Recommended**: Investigate first, likely simple name/signature fix.

### References to Check
- `Core/GameEngine/Source/Common/System/registry.cpp` - Already has `AsciiString` versions
- Check if `std::string` versions should be in GameEngine or WWDownload

---

## Priority Ranking for Implementation

### P0 - CRITICAL (Game won't start)
1. ✅ **__argc/__argv** - Command line parsing (fighter19 pattern)
2. ✅ **ApplicationHWnd** - Window handle (SDL3 integration)
3. ✅ **GameEngine::create\*** - Factory methods (investigate base class)
4. ✅ **g_csfFile/g_strFile** - Game text (UI strings)

### P1 - HIGH (Game starts but broken)
5. ✅ **FontCharsClass::Update_Current_Buffer** - Text rendering
6. ✅ **Registry std::string functions** - Linking fix

### P2 - MEDIUM (Game functional but incomplete)
7. ⚠️ **OSDisplaySetBusyState** - Cursor (stub Phase 1, implement Phase 2)
8. ⚠️ **OSDisplayWarningBox** - Dialogs (stub Phase 1, implement Phase 2)

### P3 - LOW (Legacy/optional features)
9. ⚠️ **DX8WebBrowser** - IE browser (permanent stub)
10. ⚠️ **CreateCDManager** - CD check (permanent stub)

---

## Recommended Action Plan for Next Session

### Session 37: Critical Globals & SDL3 Integration
**Goal**: Fix P0 items 1-2 (globals)

1. Implement `__argc`/`__argv` globals in SDL3Main.cpp
2. Implement `ApplicationHWnd` as SDL_Window* typedef
3. Validate with test build

**Expected Result**: Reduces undefined references from 43 to ~33

### Session 38: GameEngine Factories Investigation
**Goal**: Understand factory pattern, implement P0 item 3

1. Analyze `GameEngine` base class virtual methods
2. Determine if SDL3GameEngine should override or call base
3. Implement missing factory methods
4. Test compilation

**Expected Result**: Reduces undefined references from ~33 to ~23

### Session 39: Game Text System
**Goal**: Implement P0 item 4 (CSF/STR files)

1. Initialize `g_csfFile`/`g_strFile` globals
2. Integrate with StdLocalFileSystem
3. Test text loading

**Expected Result**: Reduces undefined references from ~23 to ~20

### Session 40: Font Rendering & Registry
**Goal**: Fix P1 items 5-6

1. Locate/implement `FontCharsClass::Update_Current_Buffer`
2. Fix Registry std::string linking
3. Full link test

**Expected Result**: ZERO undefined references, **EXECUTABLE CREATED!** 🎉

### Session 41: Stubs & Testing
**Goal**: Add P2/P3 stubs, first launch

1. Stub OSDisplay* functions
2. Stub DX8WebBrowser/CreateCDManager
3. **FIRST LAUNCH ATTEMPT!**
4. Debug startup crashes

**Expected Result**: Game window appears (may crash, but progress!)

---

## References to Consult

### fighter19 DXVK Port (PRIMARY for P0-P1)
```bash
references/old-refs/fighter19-dxvk-port/GeneralsMD/Code/Main/SDL3Main.cpp
references/old-refs/fighter19-dxvk-port/GeneralsMD/Code/GameEngineDevice/Source/SDL3GameEngine.cpp
references/old-refs/fighter19-dxvk-port/GeneralsMD/Code/GameEngine/Source/GameClient/GameText.cpp
```

### jmarshall Win64 Port (SECONDARY for patterns)
```bash
references/old-refs/jmarshall-win64-modern/Code/Main/
references/old-refs/jmarshall-win64-modern/Code/Audio/  # OpenAL patterns
```

### TheSuperHackers Upstream (BASE TRUTH)
```bash
references/old-refs/thesuperhackers-main/GeneralsMD/Code/GameEngine/Source/Common/GameEngine.cpp
```

---

## Session 36 Achievements Summary

**Compilation Phase**: ✅ 100% COMPLETE  
**Linking Phase**: ⏳ 80% COMPLETE (10 groups remaining)

### What We Fixed Today
1. ✅ Platform-specific library linking (if(WIN32) guards)
2. ✅ OpenAL audio library integration (find_package + link)
3. ✅ RegistryClass full stub implementation (47 methods)
4. ✅ d3d8lib INTERFACE target configuration
5. ✅ WWLib registry.cpp enabled for Linux

### Build Progress
- **Before Session 36**: [22/43] compilation failures
- **After Matrix4x4 fixes**: [4/4] compilation complete, 10 link errors
- **After library fixes**: [4/4] compilation complete, 2 link errors
- **After OpenAL/Registry**: [4/4] compilation complete, **43 undefined symbols**

### Lines of Code Changed
- CMakeLists.txt files: 6 files modified
- registry.cpp: 47 stub methods (Linux #else branch)
- Total: ~200 lines of platform abstraction

---

**Next Session**: Attack P0 Critical items - Start with __argc/__argv and ApplicationHWnd!
