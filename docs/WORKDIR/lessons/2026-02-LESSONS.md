# 2026-02 Lessons Learned - Phase 1 Linux Graphics Port

**Month**: February 2026  
**Sessions Covered**: 19-24, 28-30, 33, 34, 35, 66, 77  
**Focus Area**: Linux graphics port (DXVK + SDL3) + build system stabilization + compilation blockers + compat headers

---

## 🎯 Critical Lessons

### (Session 77). vcpkg in GitHub Actions CI — Never remove the toolchain to fix env vars 🔧

**Problem**: `gli CONFIG REQUIRED` failure in CI — `gli` is header-only and not available on Homebrew.

**Root Cause**: Session 72 changed `macos-vulkan` preset from `inherits: "default-vcpkg"` to `inherits: "default"`
to fix a `$env{VCPKG_ROOT}` resolution failure. But this removed the vcpkg CMake toolchain entirely. All
packages in `vcpkg.json` (gli, glm, zlib, freetype, fontconfig) stopped being provided.
Locally, `VCPKG_ROOT=/Users/felipebraz/vcpkg` was set in the shell — hid the problem for weeks.

**Correct Approach (✅ RIGHT)**:
1. Keep `inherits: "default-vcpkg"` in the preset — it sets `CMAKE_TOOLCHAIN_FILE`
2. In CI, use `lukka/run-vcpkg@v11` action to bootstrap vcpkg at the `builtin-baseline` commit from `vcpkg.json`
3. Export `VCPKG_ROOT` and `VCPKG_DEFAULT_TRIPLET=arm64-osx` to the Configure CMake step

**Wrong Approach (❌ WRONG)**:
- Changing `inherits: "default-vcpkg"` → `"default"` to avoid the env var issue (breaks vcpkg integration)
- Installing everything via Homebrew: `gli` is NOT on Homebrew

**Key Insight**: If a CMake preset fails with `$env{VCPKG_ROOT} not set`, the fix is to SET the
environment variable in CI, not to gut the preset. Use `lukka/run-vcpkg@v11`.

**Rule**: If `vcpkg.json` exists, always use `lukka/run-vcpkg@v11` in CI. Never sidestep vcpkg to use system packages alone.

---

### 0 (Session 66). Exit-time SIGSEGV — C++ Global Destructors on macOS 🚪

**Problem**: Every game exit triggered SIGSEGV in `ObjectPoolClass<X,256>::~ObjectPoolClass()` via
`__cxa_finalize_ranges`. Crash at corrupted `BlockListHead` (symbol: `AutoPoolClass<MultiListNodeClass,256>::Allocator`).

**Root Cause**: On macOS/Linux, returning from `main()` triggers `__cxa_finalize_ranges` which runs all C++
global/static object destructors. The `AutoPoolClass<X,256>::Allocator` global pools crash because their
block list memory is in an inconsistent state after game shutdown.
On Windows, `ExitProcess()` terminates immediately — C++ global destructors NEVER run. Game designed for that.

**Correct Approach (✅ RIGHT)**:
```cpp
// SDL3Main.cpp — end of main(), after SDL_Quit + shutdownMemoryManager:
_exit(exitcode);  // NOT return exitcode
```
`_exit()` terminates like `ExitProcess`: no atexit handlers, no C++ global destructors.

**Diagnosis**: Crash in `__cxa_finalize_ranges` / `exit()` call chain always means global dtor issue.
Use `nm -n binary | grep crash_offset` to identify the template instantiation.

---

### 1 (Session 35). Time/Clock Compatibility Layer Pattern — Pre-Stub Approach 🕐

**Problem**: W3DDisplay.cpp needs `QueryPerformanceCounter`/`QueryPerformanceFrequency` (Windows kernel32 APIs). GCC/Clang don't have these; must stub before code includes them.

**Attempted Approach (❌ WRONG)**:
- Define stubs inline in W3DDisplay.cpp
- Result: Conflicts with DXVK headers that also define them (ephemeral patch issue)
- Cause: Stubs must be defined BEFORE any conflicting includes

**Correct Approach (✅ RIGHT)**:
- Create compat header (`time_compat.h`) with pre-guards
- Define QueryPerformanceCounter/Frequency BEFORE anything includes Windows.h
- Use `#ifdef _WIN32` to expose Windows externs, `#else` for POSIX implementations

**Implementation**:
```cpp
// In GeneralsMD/Code/CompatLib/Include/time_compat.h
#ifdef _WIN32
extern "C" {
    BOOL __stdcall QueryPerformanceCounter(void* lpPerformanceCount);
    BOOL __stdcall QueryPerformanceFrequency(void* lpFrequency);
}
#else
// Linux: Implement using POSIX clock_gettime
inline BOOL QueryPerformanceCounter(void* lpPerformanceCount) {
    if (!lpPerformanceCount) return 0;
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    // Convert to 100-nanosecond intervals for Windows API compatibility
    int64_t* pCount = (int64_t*)lpPerformanceCount;
    *pCount = ts.tv_sec * 10000000LL + ts.tv_nsec / 100;
    return 1;
}
// Similar for QueryPerformanceFrequency...
#endif

// Then in source files that need it:
#include "compat/time_compat.h"  // Works everywhere
QueryPerformanceCounter(&tmp);   // Uses Windows API on Windows, clock_gettime on Linux
```

**Why This Works**:
- ✅ Defined in ONE place (no ephemeral patch issues)
- ✅ Pre-guard pattern prevents redefinition conflicts
- ✅ Inline implementations work for Linux stubs (no lib dependencies)
- ✅ Windows code uses native kernel32 implementations unchanged

**Reference**: Fighter19's approach + Session 33 Network.cpp pattern verification.

---

### 2 (Session 34-35). MSVC Compiler Builtins Are Not Portable — Use Standard Macros Instead 💻

**Problem**: MSVC has compiler intrinsics `__max`, `__min`, `__int64` not available in GCC/Clang.

**Anti-Pattern (❌ WRONG)**:
```cpp
Int buffer_size = __max(DEFAULT_SIZE, requested_size);  // MSVC only
Int64 timestamp = __int64 value;  // Platform-specific
```

**Correct Pattern (✅ RIGHT)**:
```cpp
// Use defined macros/typedefs from BaseTypeCore.h (already existing!)
Int buffer_size = MAX(DEFAULT_SIZE, requested_size);  // Portable
Int64 timestamp = value;  // Already typedef'd to int64_t on Linux
```

**Discovery**: The game codebase ALREADY has these macros/typedefs defined:
- `BaseTypeCore.h` defines `MAX(a,b)` macro
- `BaseTypeCore.h` defines `Int64` typedef = `int64_t`
- Sessions 34-35 just needed to find and use them

**Cleanup Audit**:
```bash
# After major compilation session, grep for remaining MSVC builtins:
grep -r "__max|__min|__int64" GeneralsMD/ Generals/ Core/ \
  | grep -v "// TheSuperHackers @" | head -20  # Find unchanged instances
```

---

### 3 (Session 35). Large Mono-Platform Functions Need Wrapper Wrapping, Not Line-By-Line Stubbing 📦

**Problem**: Screenshot function (CreateBMPFile, takeScreenShot) uses 80+ lines with 7+ Windows APIs (CreateFile, WriteFile, LocalAlloc, bitmap types, etc.).

**Anti-Pattern (❌ WRONG)**:
```cpp
// Stub each line individually
#ifdef _WIN32
    hf = CreateFile(...);
#else
    // Create a dummy handle?
#endif

#ifdef _WIN32
    WriteFile(hf, ...);
#else
    // Write to... what?
#endif
// Nightmare: 80 lines, 8+ guards, incomprehensible
```

**Correct Pattern (✅ RIGHT - Wrapper Wrapping)**:
```cpp
// Wrap ENTIRE function
#ifdef _WIN32
static void CreateBMPFile(LPTSTR pszFile, char *image, Int width, Int height) {
    // Full Windows impl (80 lines, all Windows-specific)
    HANDLE hf = CreateFile(...);
    WriteFile(...);
    LocalFree(...);
    CloseHandle(hf);
}
#else
// Linux stub - defer to Phase 3
static void CreateBMPFile(const char* pszFile, char *image, Int width, Int height) {
    // TODO (Phase 3): Implement SDL3-based screenshot capture
}
#endif
```

**When to Use Wrapper Wrapping**:
1. **Dependency Density**: >50% of function uses Windows APIs
2. **Type Complexity**: Uses 5+ Windows-specific types/constants
3. **Non-critical Feature**: Screenshot export, file save dialogs, admin ops (don't affect core gameplay determinism)

**When NOT to Use**:
- Low-dependency code (1-2 API calls) → use inline stubs
- Gameplay logic (must work on both platforms) → refactor into platform layer
- Performance-critical paths → profile before wrapping

---

### 1 (Session 19-30). CMake Asymmetry Spreads Like Wildfire 🔴

**Problem**: Initial CMake setup configured d3d8lib differently for Windows vs Linux builds.

**Impact**:
- Windows builds worked, Linux builds failed silently
- Issue took 3 sessions to diagnose
- Root cause: Platform-specific #ifdef inside CMakeLists.txt

**Lesson**: 
- ✅ **Always build BOTH target platforms** in same session (even if one fails)
- ✅ **CMake configuration must be symmetric** - platform differences belong in source code, not build config
- ✅ **Use feature flags** (`SAGE_USE_DX8`, `BUILD_WITH_DXVK`) not platform guards (#ifdef) in CMake

**Pattern**:
```cmake
# ❌ WRONG - CMake platform decision
if(WIN32)
    target_link_libraries(d3d8lib PUBLIC directx)
else()
    target_link_libraries(d3d8lib PUBLIC dxvk)
endif()

# ✅ RIGHT - Feature flag decision
if(SAGE_USE_DX8)
    target_link_libraries(d3d8lib PUBLIC directx)  # or dxvk
endif()
```

---

### 2. Legacy Code Has Hidden Platform Assumptions 🏚️

**Problem**: Dozens of `#include <windows.h>` scattered in old code, assumed to always be available.

**Impact**:
- Linux builds failed with 100+ "windows.h not found" errors
- Each fixed manually, one by one
- Spread across multiple build attempts

**Lesson**:
- ✅ **Create platform abstraction layer FIRST** before fixing code
- ✅ **Use header shim pattern** (`windows_compat.h`) to intercept system includes
- ✅ **Centralize platform guarding** rather than fixing each occurrence individually

**Anti-pattern** (saw this, don't do it):
```cpp
// ❌ SCATTERED across 20+ files
#ifdef _WIN32
    #include <windows.h>
    #define MY_CONSTANT 100
#else
    #define MY_CONSTANT 200
#endif
```

**Pattern** (do this):
```cpp
// ✅ Core/Libraries/Include/compat/windows_compat.h
#ifndef _WINDOWS_COMPAT_H_
    #include <my_platform_definitions>
#endif

// Then in source files
#include "compat/windows_compat.h"  // Works everywhere
```

---

### 3. Precompiled Headers Hide Real Errors 🙈

**Problem**: PCH (precompiled headers) compilation succeeded, but real source files had hundreds of errors.

**Impact**:
- Spent 1 session debugging "PCH won't compile" when real issue was source dependencies
- CMake didn't report errors until after PCH phase
- False sense of progress

**Lesson**:
- ✅ **Disable PCH early in porting** (`PCH_ENABLED=OFF` in CMake)
- ✅ **Compile real source first**, add PCH optimization later
- ✅ **PCH errors are red herrings** in cross-platform ports - fix underlying includes

**Workflow**:
```bash
# Phase 1: Disable PCH (find all real errors)
cmake --preset linux64-deploy -DPCH_ENABLED=OFF

# Phase 2: Enable PCH after core build works
cmake --preset linux64-deploy -DPCH_ENABLED=ON
```

---

### 4. Reference Code Differs More Than Expected 📖

**Problem**: fighter19's DXVK solution looked straightforward, but details varied from our codebase.

**Impact**:
- Copy-paste didn't work (type mismatches, function signatures)
- Required re-architecting 3 times to find the pattern
- Sessions 16-18 spent on this alone

**Lesson**:
- ✅ **Study reference code ARCHITECTURE, not implementation**
- ✅ **Adapt patterns to your codebase** rather than copy-paste
- ✅ **Reference code is proof-of-concept**, not production template

**Process**:
```
Reference → Understand Pattern → Adapt to OUR Code → Test → Refine
No:    Reference → Copy-Paste → Compile Error → Give Up
```

---

### 5. Windows Compatibility Testing is Early Validation 🔄

**Problem**: Made changes that broke Windows VC6 build, didn't test until late.

**Impact**:
- Had to revert changes
- Regression took 2 hours to diagnose
- Could have been caught immediately if tested both platforms

**Lesson**:
- ✅ **Test Windows build DAILY** even when focusing on Linux
- ✅ **Use CI/CD or scripting** to validate both platforms quickly
- ✅ **"Linux works, Windows broken" is still a failure**

**Pattern**:
```bash
# After EACH significant change
./scripts/docker-build-mingw-zh.sh mingw-w64-i686    # Windows cross-compile
./scripts/docker-build-linux-zh.sh linux64-deploy    # Linux native build
# Both must succeed
```

---

### 6. DXVK Pre-Guards Are Ineffective 🛡️

**Problem**: Attempted to prevent DXVK type conflicts by pre-defining guard macros in windows_compat.h.

**Impact**:
- Pre-guards failed completely (DXVK doesn't check them before defining types)
- Build stuck at [321/1254] for entire session debugging
- Manual ephemeral patches required in DXVK headers directly
- Patches lost on every clean build (`rm -rf build/`)

**Lesson**:
- ✅ **DXVK wrapper headers don't check custom guards** - must patch source directly
- ✅ **Ephemeral patches are dangerous** - lost on CMake reconfigure
- ✅ **Automate patching via CMake** - use `ExternalProject_Add` with `PATCH_COMMAND`
- ✅ **Guard pattern location matters** - add guards IN the header being included, not before

**Pattern**:
```cpp
// ❌ DOESN'T WORK - DXVK ignores this
// GeneralsMD/Code/CompatLib/Include/windows_compat.h
#ifndef _WIN32
    #define _MEMORYSTATUS_DEFINED 1    // Trying to prevent DXVK from defining
    #define _IUNKNOWN_DEFINED 1
#endif
#include <windows_base.h>              // DXVK still defines them anyway!

// ✅ WORKS - Patch DXVK header directly
// build/_deps/dxvk-src/include/dxvk/windows_base.h
#ifndef _MEMORYSTATUS_DEFINED
typedef struct MEMORYSTATUS {
    DWORD  dwLength;
    SIZE_T dwTotalPhys;
} MEMORYSTATUS;
#define _MEMORYSTATUS_DEFINED 1
#endif
```

**Future Fix**: Automate with CMake patch command (avoid manual edits).

---

### 7. Incremental Builds Mask True Complexity 📊

**Problem**: Build target count varied wildly (1254→936→128) between builds, making progress tracking confusing.

**Impact**:
- Hard to measure actual progress ("Did we fix things or just recompile less?")
- Ninja only rebuilds changed files + dependents
- CompatLib change = entire Core + Game rebuild (100s of targets)
- Clean build reveals true scale (1254 targets for full GeneralsXZH)

**Lesson**:
- ✅ **Target count is NOT progress metric** - use percentage of total instead
- ✅ **Understand Ninja dependency tracking** - file timestamp changes trigger cascading rebuilds
- ✅ **Clean builds reveal reality** - incremental hides technical debt
- ✅ **Track cumulative fixes, not current target count**

**Breakdown** (Clean Build Target Distribution):
```
Total: 1254 targets (GeneralsXZH linux64-deploy)
- SDL3 dependencies: ~600 targets
- GameSpy networking: ~50 targets  
- CompatLib: ~20 targets
- Core engine: ~100 targets
- Game logic: ~400 targets
- Tools: ~84 targets
```

**Why Counts Vary**:
- Change `windows_compat.h` → recompile Core + Game (~500 targets)
- Change `LocalFile.cpp` only → recompile dependents (~10 targets)
- Clean build → ALL 1254 targets

---

### 8. Historical Note: Temporary GameSpy Stubbing Was Acceptable During Linux Bring-Up

**Problem**: GameSpy networking code (lobby, SNMP, matchmaking) has Windows-specific APIs that don't port easily.

**Impact**:
- StagingRoomGameInfo.cpp blocker (44 SNMP errors)
- Concerns about breaking multiplayer functionality
- Temptation to invest weeks porting complex networking stack

**Lesson at the time**:
- ✅ **GameSpy online services shut down in 2013** - legacy services were not required for Linux bring-up
- ✅ **LAN multiplayer doesn't need GameSpy lobby** - direct IP connection works
- ✅ **Temporary stubbing of some online-facing paths was acceptable** for compile/runtime progress
- ✅ **fighter19/jmarshall both removed GameSpy** - proven reference for a narrower porting scope

**Current GeneralsX clarification**:

- GeneralsX should still preserve GameSpy-related compatibility when practical because future integration with community online services remains desirable.
- This lesson should be read as a **historical bring-up tactic**, not as a standing rule to remove or permanently stub networking features indiscriminately.

**What was considered safe to stub during that phase**:
- Online lobby/matchmaking tied only to dead legacy services
- GameSpy SNMP monitoring used only as a diagnostic dependency
- Account authentication paths tied only to legacy services

**What now requires more caution**:
- Any GameSpy-related code that may matter for future Generals Online compatibility
- NAT and peer negotiation paths that may be reused by community online backends

**What Must Work**:
- LAN discovery (local network)
- Direct IP connection
- Replay system
- Single-player campaign

**Pattern** (Reference from fighter19):
```cpp
// GameSpy lobby functions
#ifdef _WIN32
    // Full Windows GameSpy implementation
#else
    // Linux: Stub returns failure, LAN multiplayer unaffected
    bool ConnectToGameSpyLobby() { return false; }
#endif
```

---

## 🎓 Anti-Patterns to Avoid

### 1. ❌ Lazy Stubs for Platform-Specific Code

**Seen**: Empty `{}` implementations for Linux-only functions, planning to "do it later"

**Problem**: Breaks determinism, hides actual requirements, technical debt spirals

**Alternative**: 
- Document stub with `TODO:` or `FIXME:` comment
- Assert/log if called unexpectedly
- Make it visible, not silent

### 2. ❌ Mixing Platform Logic into Game Logic

**Seen**: `#ifdef __linux__` inside gameplay calculations

**Problem**: Breaks gameplay determinism, makes replays fragile, hard to test

**Alternative**: 
- Isolate platform code to `Core/GameEngineDevice/` and `Core/Libraries/Source/Platform/`
- Inject platform-specific behavior via callbacks/interfaces
- Keep game logic pure and testable

### 3. ❌ "Works on My Mac" Testing

**Seen**: Testing only on macOS Docker, not in actual target environments

**Problem**: Doesn't catch Linux-specific issues, terminal assumptions, library paths

**Alternative**:
- Test in Linux VM sometimes (not just Docker)
- Test Windows build at least weekly
- Use GitHub Actions for continuous validation

---

## 🏆 Patterns That Worked Well

### 1. ✅ Docker-Based Development

**Why**: Isolated build environment, reproducible results, no Mac pollution

**How**: Used `./scripts/docker-build-linux-zh.sh` consistently

**Result**: Reliable builds, easy to validate in clean environment

### 2. ✅ Handoff Documentation

**Why**: SESSION handoff document captures context for next session

**How**: Each session end = detailed handoff (blockers, achievements, next steps)

**Result**: Rapid context recovery, no "what were we doing?" moment

### 3. ✅ Minimal Diffs Approach

**Why**: Small, focused changes easier to validate and debug

**How**: Fix ONE category of issues per session (e.g., "Windows compatibility layer" or "D3D type definitions")

**Result**: Fast debugging, clear cause-effect, easier to revert if needed

---

## 📈 Process Improvements Discovered

### 1. Build Validation Script Pattern

**Current**: Manual `docker-build-linux-zh.sh` commands  
**Improved**: Created consolidated `validate-both-platforms.sh`

```bash
#!/bin/bash
# Validate Windows + Linux builds before commit
docker-build-mingw-zh.sh mingw-w64-i686 || exit 1
docker-build-linux-zh.sh linux64-deploy || exit 1
echo "✓ Both platforms validated"
```

### 2. Session Checklist

**Current**: Free-form session work  
**Improved**: Use standardized checklist in handoff

```markdown
## Session Checklist
- [ ] Windows VC6 build still works
- [ ] Linux docker build compiles to [X%]
- [ ] No Windows regression
- [ ] Documented blockers clearly
- [ ] Next session has clear "first task"
```

### 3. Architecture Decision Log (ADL)

**Current**: Decisions scattered in handoffs  
**Improved**: Consolidate in `planning/ARCHITECTURE_DECISIONS.md`

```markdown
| Date | Decision | Rationale | Trade-offs |
|------|----------|-----------|-----------|
| 2026-02-09 | DXVK wrapper pattern | Minimal game code changes | Abstraction layer overhead |
```

---

## 🔮 Recommendations for Future Phases

### Phase 2 (Audio) - What We Learned

- **Use OpenAL abstraction layer** pattern from start (don't repeat graphics layer mistakes)
- **Miles → OpenAL mapping** simpler than DirectX → Vulkan, validate early
- **Consider audio determinism** requirement (replays need matching audio events)

### Phase 3+ (Video) - Considerations

- **Reference solution too complex** - investigate simpler video skipping approach
- **Balance features vs complexity** - full video support may not be worth it

### General Process Improvement

- **Establish "Platform Compatibility Review" gate** - before merging any code, check both platforms build
- **Monthly lessons consolidation** - don't let knowledge scatter
- **Reference code audit sprint** - map all 3 reference repos to patterns once, document once

---

## Session-Specific Insights

### Session 19-20: Foundation Shakes ⚠️
- Hidden complexity of DirectX8 type availability
- SDL3 headers dependency chain very deep
- Learned: **Architecture decisions early pay dividends**

### Session 21-22: Hard Yards 💪
- Network/FTP code surprisingly platform-portable
- Small fixes compound into visible progress
- Learned: **Persistence on small issues unlocks momentum**

### Session 23-24: Breakthrough Approach 🚀
- Build progression tracking visible (112→200+ files)
- Handoff documentation clarity improves velocity
- Learned: **Transparency into progress boosts morale**

### Session 28-30: Systematic Blocker Elimination 🔨
- DXVK conflicts require direct patching (pre-guards insufficient)
- 64-bit pointer cast pattern established (20+ occurrences fixed)
- Platform isolation working (`#ifdef _WIN32` guards compile cleanly)
- "100 targets/day" question answered: **viable when clean, blockers more valuable than raw speed**
- Progress: [321/1254] → [~800/1254] (25.6% → 63.8%)
- Learned: **Methodical blocker resolution beats rushing toward arbitrary metrics**

---

## Metrics & Velocity

| Metric | Value | Trend |
|--------|-------|-------|
| Build completion % | 112→200+ files (S19-24), 321→800/1254 (S28-30) | ⬆️ Accelerating |
| Error categories resolved | 8+ (per phase), 11 blockers (S28-30) | ⬆️ Growing |
| Session handoff clarity | Sessions 19-24, 28-30 | ⬆️ Improving |
| Windows regression count | 1 (Session 23, fixed) | ➡️ Stable |
| Phase 1 compilation progress | 63.8% (800/1254 targets) | ⬆️ Strong |
| Pointer cast fixes | 20+ cumulative (S28-30) | ⬆️ Pattern established |

---

## Conclusion

**Phase 1 Linux Graphics Port** has surfaced critical patterns for cross-platform game development:

1. **Platform symmetry matters** - build BOTH platforms, always
2. **Architecture abstractions save time** - witness our CMake+compat layer pattern
3. **Reference code is guide, not gospel** - adapt, don't copy-paste
4. **Testing both platforms is non-negotiable** - Windows break = Phase failure
5. **Handoff documentation multiplies velocity** - next session starts ahead

**For Phase 2 (Audio)**: Apply these lessons immediately. **No lazy stubs. No platform logic in game code. Test both platforms.**

---

**Written**: 2026-02-11 (Sessions 19-24), Updated: 2026-02-13 (Sessions 28-30), Updated: 2026-02-18 (Session 43)
**Based on**: Sessions 19-24, 28-30 handoffs and reports, Session 43 asset loading investigation
**Next review**: 2026-03-15 (after Phase 2 completion)

---

## Session 43 Addendum — Steam ZH Standalone & Language Detection (2026-02-18)

### Lesson A: Steam ZH standalone uses `ZH_Generals/` subdir, not siblings

**Problem**: On a fresh Steam install of Zero Hour, the base Generals .big files
(`Textures.big`, `W3D.big`, `shaders.big`, etc.) are inside `GeneralsMD/ZH_Generals/`,
not in a sibling `Generals/` directory. Our `GetStringFromGeneralsRegistry` fallback
only looked for `../Generals/`, which doesn't exist on a pure Steam install.

**Correct Approach**:
- Check `ZH_Generals/` FIRST (Steam standalone layout, most common for Linux users)
- Fall back to `../Generals/` (dev split install, project dev layout)
- Both paths probe via `stat()` to avoid any hardcoding

**Implementation location**: `registry.cpp` → `tryRelativeGeneralsPath()`

---

### Lesson B: Language must be auto-detected from BIG file presence on Linux

**Problem**: `GetRegistryLanguage()` defaults to `"english"` when the Windows registry
is unavailable (Linux). A Brazilian Portuguese Steam install has `BrazilianZH.big`
but no `EnglishZH.big`. The CSF loader tried `data/english/generals.csf` → crash.

**Correct Approach**:
- In `GetStringFromRegistry` for key `"Language"`, probe for `*ZH.big` files in CWD
- Return the corresponding language string that the game loader expects
- Support all known localizations: brazilian, english, german, french, spanish, chinese, korean, polish

**Key Insight**: The language string used for BIG file detection MUST match the language
string the CSF loader expects for its path (`data/<language>/generals.csf`).

**Implementation location**: `registry.cpp` → `tryAutoDetectLanguage()`

---

### Lesson C: Never add duplicate draw() calls outside GameClient

**Problem**: A previous session added `TheDisplay->step()` + `TheDisplay->draw()`
to `GameEngine::update()` after `TheFramePacer->update()`. This caused double-drawing
every frame because `GameClient::update()` already dispatches `TheDisplay->DRAW()`.

**Why it was wrong**:
- `GameEngine::update()` calls `TheGameClient->UPDATE()` (unconditionally)
- `GameClient::update()` calls `TheDisplay->DRAW()` at the bottom
- The extra call after FramePacer ran AGAIN, potentially presenting a second frame
  with wrong/empty state

**Fighter19 reference confirms**: No `TheDisplay->draw()` call exists in
fighter19's `GameEngine.cpp`. The dispatch is exclusively via `GameClient`.

**Rule**: `TheDisplay->draw()` is owned by `GameClient`. Do not call it from
`GameEngine` or any other location outside of `GameClient`.

---

### Lesson D: Always verify env var NAMES match between scripts and C++ code

**Problem**: The run script used `CNC_ZH_INSTALLPATH` but `GetStringFromGeneralsRegistry`
reads `CNC_GENERALS_INSTALLPATH`. These map to DIFFERENT C++ functions and have
different semantics. A mismatch silently fails (no error, no warning).

**Correct Approach**:
- `CNC_GENERALS_<KEY>` → `GetStringFromGeneralsRegistry()` (base Generals settings)
- `CNC_ZH_<KEY>` → `GetStringFromRegistry()` (Zero Hour settings)
- Document the mapping clearly in both the script and registry.cpp

**Rule**: When adding env var overrides, verify the prefix matches the C++ function
being called. Test with `export CNC_GENERALS_INSTALLPATH=/path && run-linux-zh.sh`.


---

## Lesson S47: Binary File Format Structs Must Use Fixed-Width Integer Types

**Session**: 47 (2026-02-20)
**Context**: 4 TGA textures failing as TEX_MISSING on Linux 64-bit despite files existing in BIG archives

**The Problem**: `TGA2Footer` in `TARGA.h` used `long` for file offset fields:
```cpp
typedef struct _TGA2Footer {
    long Extension;   // 4 bytes on Windows, 8 bytes on Linux 64-bit!
    long Developer;   // same issue
    char Signature[16];
    char RsvdChar;
    char BZST;
} TGA2Footer;
```
`sizeof(TGA2Footer)`: Windows = 26, Linux 64-bit = **34** (wrong!)

`Targa::Open()` hardcodes `File_Seek(-26, SEEK_END)` then tries to read `sizeof(TGA2Footer)` = 34 bytes → only 26 available → read returns 26 ≠ 34 → TGAERR_READ → texture fails.

**The Fix**: Use `int32_t` for all fields that must be exactly 4 bytes per the file format spec:
```cpp
#include <stdint.h>
typedef struct _TGA2Footer {
    int32_t Extension;   // always 4 bytes everywhere
    int32_t Developer;   // always 4 bytes everywhere
    ...
} TGA2Footer;
static_assert(sizeof(TGA2Footer) == 26, "TGA2Footer size mismatch");
```

**Golden Rule**: **Any struct that is read/written as binary file data must use `<stdint.h>` fixed-width types** (`int32_t`, `uint32_t`, `uint16_t`, `int16_t`, etc.) instead of `long`, `int`, `short`. Add `static_assert(sizeof(Struct) == N, "...")` to catch future regressions.

**ABI Context**:
- Windows: LLP64 — `long` = 4 bytes on both 32-bit and 64-bit
- Linux 64-bit: LP64 — `long` = 8 bytes
- This is the same category of bug as Session 45's `void* Surface` in `LegacyDDSURFACEDESC2`

**Key Files**: `Core/Libraries/Source/WWVegas/WWLib/TARGA.h`

---

## LESSON-51: DataChunk Unicode Strings — sizeof(WideChar) Ruins Map Parsing on Linux

**Session**: 51 (2026-02-20)
**Files**: `Core/GameEngine/Source/Common/System/DataChunk.cpp`, `Generals/Code/GameEngine/Source/Common/System/DataChunk.cpp`

### The Bug

`DataChunk::readUnicodeString()` and `writeUnicodeString()` used `sizeof(WideChar)` to compute how many bytes to read/write per character:

```cpp
file->read(buf, len * sizeof(WideChar));
info->decrementDataLeft(len * sizeof(WideChar));
```

`WideChar = wchar_t`. On **Windows** `sizeof(wchar_t)` = **2** (UTF-16). On **Linux** `sizeof(wchar_t)` = **4** (UTF-32).

The `.map` file format stores Unicode strings as UTF-16LE (2 bytes/char, same as Windows). On Linux the code reads 4 bytes per character instead of 2, then decrements `dataLeft` by 4 per char instead of 2. This desynchronizes the chunk parser: all sub-chunks after the first Unicode string appear consumed, so they parse as empty.

### The Symptom Trail

The bug was silent — no crash, no error. Symptoms:

1. Shell map terrain rendered (no Unicode fields before terrain)
2. **Zero units, frozen camera, no scripts** (all come from `PlayerScriptsList` sub-chunk, which is parsed AFTER player display names — Unicode fields)
3. `ScriptEngine` reported `scripts=0 groups=0` for ALL 11 player sides
4. Diagnostic confirmed `canUpdateLogic=1` (game loop was fine, logic was running)

### The Investigation Method That Found It

Conservative: log `canUpdateLogic` first → logic fine → log object count after `startNewGame()` → 0 → log script count per player → 0 for all 11 sides → inspect `SidesList::ParseSidesDataChunk` → find `readUnicodeString` uses `sizeof(WideChar)` → testable hypothesis → fix.

### The Fix

```cpp
// Always 2 bytes per char on disk (UTF-16LE, matches Windows .map file format)
static const Int WCHAR_DISK_SIZE = sizeof(UnsignedShort);

// Read: load 2-byte chars from file, widen to wchar_t on Linux
for (Int i = 0; i < (Int)len; i++) {
    UnsignedShort ch16 = 0;
    file->read(&ch16, WCHAR_DISK_SIZE);
    buf[i] = (WideChar)ch16;
}
info->decrementDataLeft(len * WCHAR_DISK_SIZE);
```

Write is the mirror: narrow `WideChar` to `UnsignedShort` before writing.

### Golden Rule

**Any code that reads binary file data into `wchar_t` or `WideChar` must use the on-disk byte width (2 for UTF-16LE), NOT `sizeof(wchar_t)`**.

This is the same category as LESSON-50 (`long` in TGA structs) and LESSON-45 (`void*` in DDS struct): **never use ABI-dependent types when reading fixed-width binary file formats**.

**Checklist**:
- `sizeof(wchar_t)` appearing near file I/O → potential bug on Linux
- `sizeof(WideChar)` appearing near chunk `decrementDataLeft` → definite bug
- Any struct with `wchar_t[]` fields loaded via `memcpy` or `fread` → likely broken on Linux

**Key Files**: `Core/GameEngine/Source/Common/System/DataChunk.cpp`

---

## Lesson: SDL Mouse Coordinate Scaling is MANDATORY (Session 52)

**Problem**: SDL3 (and SDL2) report mouse coordinates in physical window pixels. The game's UI hit-testing uses its internal logical resolution (e.g. 800×600). Without scaling, clicks land at wrong positions and every button press is silently ignored by the game. The symptom is: menus render fine, cursor moves, but nothing is clickable.

**Root Cause Pattern**: Any SDL port that copies raw `event.x / event.y` values directly into `MouseIO::pos` without scaling will exhibit this bug. It is platform and SDL-version independent.

**Fix**: Add `scaleMouseCoordinates(rawX, rawY, windowID, scaledX, scaledY)` (fighter19 reference) and apply it in the central `translateEvent()` after dispatching to type-specific handlers. Always extract `windowID` from the SDL event and call `SDL_GetWindowSize` to compute the scale factor against `TheDisplay->getWidth/Height()`.

**Applies To**: `SDL3Mouse::translateEvent()` — all three event types (motion, button, wheel). For wheel events use `event.wheel.mouse_x/mouse_y`, not a separate `SDL_GetMouseState()` call.

**Reference**: `references/old-refs/fighter19-dxvk-port/GeneralsMD/Code/GameEngineDevice/Source/SDL3Device/GameClient/SDL3Mouse.cpp` → `scaleMouseCoordinates()`

---

## Lesson: SDL3Mouse MUST set m_inputMovesAbsolute = TRUE in init() (Session 53)

**Problem**: Even with correct coordinate scaling, menu clicks still did not register. SDL3 reports absolute window-pixel positions, but `Mouse::processMouseEvent()` defaults to `MOUSE_MOVE_RELATIVE` mode (adds incoming coordinate to tracked position as a delta). The internal tracked cursor drifts exponentially from the real cursor position. No click ever lands where it should.

**Root Cause**: `SDL3Mouse::init()` was missing `m_inputMovesAbsolute = TRUE`, which both `Win32Mouse::init()` and fighter19's `SDL3Mouse::init()` set explicitly.

**Fix**: In `SDL3Mouse::init()`, after calling `Mouse::init()`:
```cpp
m_inputMovesAbsolute = TRUE;
```

**Rule**: Any SDL mouse backend that supplies absolute positions MUST set this flag. If it is missing, the cursor and click positions will diverge immediately and no UI interaction will work. Check this first when porting any new SDL mouse backend.


---

## Lesson: TheSuperHackers API Differences From fighter19 (Session 54 — Phase 2 Audio)

When porting code from fighter19's DXVK branch to TheSuperHackers base, expect these systematic API differences:

| fighter19 | TheSuperHackers | Reason |
|---|---|---|
| `BitTestEA(mask, bit)` | `BitIsSet(mask, bit)` | Renamed in TSH to avoid `winnt.h` macro conflict (BaseType.h line 90) |
| `req->deleteInstance()` | `deleteInstance(req)` | Free function from GameMemory.h — `MemoryPoolObject` subclasses do NOT have a `deleteInstance` member |
| `getUninterruptable()` | `getUninterruptible()` | Spelling corrected upstream |
| `#include "Common/File.h"` | `#include "Common/file.h"` | Linux filesystem is case-sensitive; TSH uses lowercase `file.h` |
| `) AL_API_NOEXCEPT17 {` | `) noexcept {` | Ubuntu 24.04 OpenAL 1.23.1 package does not define `AL_API_NOEXCEPT17` |
| OpenAL debug extension symbols | `#ifdef AL_EXT_debug` guard | Debug extension (`AL_EXT_debug`) not present in Ubuntu's OpenAL package |

**Scan for all of these before building** — they appear in any audio or game logic file ported directly from fighter19.

---

## Lesson: CMake FindFFmpeg.cmake pkg-config Variable Prefix Bug (Session 54)

**Problem**: The `FindFFmpeg.cmake` module (from jmarshall) uses a `find_component()` macro that constructs hint paths as `PC_LIB${_component}_INCLUDEDIR` (e.g. `PC_LIBAVCODEC_INCLUDEDIR`), but `pkg_check_modules()` actually sets `PC_${_component}_INCLUDEDIR` (e.g. `PC_AVCODEC_INCLUDEDIR`). This mismatch means the paths are never found, causing configure to fail even when FFmpeg is installed.

**Fix**: Replace `find_package(FFmpeg REQUIRED COMPONENTS ...)` entirely with:
```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavcodec
    libavformat
    libavutil
    libswresample)
target_link_libraries(target PRIVATE PkgConfig::FFMPEG)
```

This bypasses FindFFmpeg.cmake entirely and is more reliable on Linux.

---

## Lesson: OpenAL + FFmpeg Audio Port Strategy (Session 54)

The fastest path to working audio on Linux is to port fighter19's implementation wholesale rather than building from scratch:

1. fighter19 already has a complete 3,490-line Zero Hour OpenAL + FFmpeg implementation
2. The adaptation diff is small (7 known API differences documented above)
3. Structure: `OpenALAudioManager.cpp` + `OpenALAudioCache.cpp` (FFmpeg decoder) + `OpenALAudioStream.cpp` (buffer-queue) + `FFmpegFile.cpp` (AVIO custom reader)
4. Keep old stub (`Source/OpenALAudioManager.cpp`) as fallback for `SAGE_USE_OPENAL=OFF` builds
5. Add `Source/OpenALAudioDevice/` as PRIVATE include dir so `OpenALAudioCache.h` is not exposed

**Result**: Menu music and audio backend confirmed working in one session.

---

## Lesson: macOS Apple Silicon — Two Homebrew Prefixes (Phase 5, Session 60)

**Problem**: Building arm64 target but linker fails with `symbol(s) not found for architecture arm64` for FFmpeg symbols despite FFmpeg being installed.

**Root Cause**: macOS Apple Silicon has two Homebrew installations:
- `/usr/local/` — Intel (Rosetta 2) Homebrew, default system `pkg-config`, all dylibs are **x86_64**
- `/opt/homebrew/` — Apple Silicon Homebrew, dylibs are **arm64**

CMake's `pkg_check_modules` uses `/usr/local/bin/pkg-config` (in `$PATH` by default), which resolves FFmpeg to Intel x86_64 dylibs at `/usr/local/Cellar/ffmpeg/`. Linking arm64 objects against x86_64 dylibs fails at link time.

**Fix**: Set `PKG_CONFIG_PATH=/opt/homebrew/lib/pkgconfig` in the CMake configure preset's `"environment"` block:
```json
"environment": {
    "PKG_CONFIG_PATH": "/opt/homebrew/lib/pkgconfig"
}
```

**Corollary**: Universal Binary (`arm64;x86_64`) requires ALL external dylibs to be fat binaries. When third-party dylibs (FFmpeg, openal-soft) are single-arch, build arm64-only first. Revisit Universal Binary in the polish phase.

---

## Lesson: fontconfig Static Library Needs Explicit iconv on macOS (Phase 5, Session 60)

**Problem**: Linking a static fontconfig from vcpkg fails with `_iconv`, `_iconv_open`, `_iconv_close` undefined.

**Root Cause**: fontconfig's `fcfreetype.c` uses `iconv()` for charset conversion. When fontconfig is a static library, its iconv dependency must be explicitly resolved at link time.

**Fix**: Add `find_package(Iconv REQUIRED)` and `Iconv::Iconv` linkage wherever fontconfig is linked:
```cmake
if(UNIX)
    find_package(Fontconfig REQUIRED)
    find_package(Iconv REQUIRED)  # fontconfig static lib needs iconv
endif()
target_link_libraries(mytarget INTERFACE
    $<$<PLATFORM_ID:Darwin>:Fontconfig::Fontconfig>
    $<$<PLATFORM_ID:Darwin>:Iconv::Iconv>
)
```
On macOS, CMake's `FindIconv` resolves to `/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib/libiconv.tbd` (system stub — correct for arm64).

---

## Session 62 — 24/02/2026: Meson from Rosetta Homebrew Produces x86_64 Even on Apple Silicon

### Problem
`dlopen(libdxvk_d3d8.dylib)` failed with "incompatible architecture (have 'x86_64', need 'arm64')".
The game process is `arm64` (native Apple Silicon) but the dylib was built as `x86_64`.

### Root Cause
`/usr/local/bin/meson` is the Intel (x86_64) Homebrew binary running under Rosetta 2.
Without explicit `-arch arm64` flags, Clang defaults to the Rosetta process's arch (`x86_64`).
The entire DXVK build came out x86_64 even though the Mac is Apple Silicon.

### Fix
Pass explicit architecture flags via `cmake -E env` before `meson setup` in `cmake/dx8.cmake`:
```cmake
# Detect host arch (arm64 on Apple Silicon, x86_64 on Intel)
execute_process(COMMAND uname -m OUTPUT_VARIABLE DXVK_HOST_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)

CONFIGURE_COMMAND
  ${CMAKE_COMMAND} -E env
    CC=clang CXX=clang++
    "CFLAGS=-arch ${DXVK_HOST_ARCH}"
    "CXXFLAGS=-arch ${DXVK_HOST_ARCH}"
    "LDFLAGS=-arch ${DXVK_HOST_ARCH}"
  ${MESON_EXECUTABLE} setup ...
```

### Manual Rebuild Command (arm64)
```bash
DXVK_SRC="build/macos-vulkan/_deps/dxvk-src"
rm -rf /tmp/dxvk-arm64-build
CC=clang CXX=clang++ CFLAGS="-arch arm64" CXXFLAGS="-arch arm64" LDFLAGS="-arch arm64" \
  /usr/local/bin/meson setup /tmp/dxvk-arm64-build "$DXVK_SRC" -Ddxvk_native_wsi=sdl3 --buildtype=release
ninja -j8 -C /tmp/dxvk-arm64-build src/d3d8/libdxvk_d3d8.0.dylib
# Validation:
file /tmp/dxvk-arm64-build/src/d3d8/libdxvk_d3d8.0.dylib
# → Mach-O 64-bit dynamically linked shared library arm64  ✅
```

### Key Rule
**Always run `file` on produced dylibs when dlopen fails on macOS.**  
On machines with dual Homebrew installs (Intel + arm64), always pass explicit `-arch` flags.
---

## Lesson (Session 63). DXVK Vulkan Loader: macOS Library Names — vulkan_loader.cpp

**Problem**: After Patch 6 fixed `getExeName()`, DXVK reached `createLibraryLoader()` → `loadVulkanLibrary()` and logged:
```
err:   Vulkan: vkGetInstanceProcAddr not found
[1]    segmentation fault  ./GeneralsXZH
```
Crash in `DxvkInstance::createLibraryLoader` (null function call, PC=0x0).

**Root Cause**: `vulkan_loader.cpp` `loadVulkanLibrary()` only tries:
```cpp
#ifdef _WIN32
  "winevulkan.dll", "vulkan-1.dll"
#else
  "libvulkan.so", "libvulkan.so.1"    // Linux names only
#endif
```
On macOS, `dlopen("libvulkan.so")` fails (no such file). The function returns `{ nullptr, nullptr }`. DXVK then tries to call through the null `vkGetInstanceProcAddr` → SIGSEGV at 0x0.

**Important**: `libvulkan.1.4.341.dylib` from the Vulkan SDK was appearing in crash binary images because `DYLD_LIBRARY_PATH` or `dlopen` indirectly loaded it — but not through DXVK's explicit search.

**Fix (Patch 7)**:

Split `dllNames` into per-platform static arrays (`std::array` size varies per platform):
```cpp
#ifdef _WIN32
  static const std::array<const char*, 2> dllNames = {{ "winevulkan.dll", "vulkan-1.dll" }};
#elif defined(__APPLE__)
  // LunarG Vulkan SDK installs libvulkan.dylib; MoltenVK is the ICd driver.
  static const std::array<const char*, 4> dllNames = {{
    "libvulkan.dylib", "libvulkan.1.dylib", "libvulkan.1.4.341.dylib", "libMoltenVK.dylib"
  }};
#else
  static const std::array<const char*, 2> dllNames = {{ "libvulkan.so", "libvulkan.so.1" }};
#endif
```

**Deploy Fix**: `deploy-macos-zh.sh` was not copying Vulkan/MoltenVK libs to the runtime dir. Added:
- Auto-detect Vulkan SDK at `~/VulkanSDK/*/macOS`
- Copy `libvulkan.dylib`, `libvulkan.1.dylib`, `libMoltenVK.dylib` to runtime dir
- Write `MoltenVK_icd.json` with `"library_path": "./libMoltenVK.dylib"` (relative)
- Update `run.sh` to set `VK_ICD_FILENAMES="${SCRIPT_DIR}/MoltenVK_icd.json"`

**Rule**:
> **Every non-Windows DXVK port needs explicit Vulkan library names for the target OS.**
> On macOS: `libvulkan.dylib` (LunarG SDK loader), NOT `libvulkan.so`.
> The Vulkan loader must also be deployed alongside the binary with a valid MoltenVK ICD JSON.

**Commits**: `95c7911b3`

---

## LESSON: D3D8Batcher StateChange Null VkBuffer — Post-Flush State Restore Bug (Session 64)

**Category**: DXVK macOS Porting

**Problem**: Game crashes in dxvk-cs thread at `MVKBuffer::getMTLBuffer() + 20` during first
`DrawIndexedPrimitive`. ARM64: `ldr x0, [x0, #0x58]` with x0=null.

**Root Cause**: `D3D8Batcher::StateChange()` — after flushing batched draws via
`DrawIndexedPrimitiveUP` — calls `m_device->SetStreamSource(0, GetD3D9Nullable(m_stream), ...)`.
`m_stream` is always a `D3D8BatchBuffer(pDevice, nullptr, ...)` — null D3D9 backing.
`GetD3D9Nullable` returns null → D3D9 stream 0 gets overwritten with null.

This races with `D3D8Device::SetStreamSource`: caller sets D3D9 stream 0 to a real buffer,
then triggers `StateChange()` via the batcher, which nullifies the just-set buffer.
`DrawIndexedPrimitive` then finds stream 0 = null → null VkBuffer → null MVKBuffer → crash.

**Fix**: Remove `m_device->SetStreamSource()` and `m_device->SetIndices()` from `StateChange()`.
`DrawIndexedPrimitiveUP` already unbinds them per D3D9 spec; the D3D8Device layer re-sets
correct state before every subsequent draw call.

**DXVK Patch**: Patch 11 in `cmake/dxvk-macos-patches.py`, targets `src/d3d8/d3d8_batch.h`.

**Rule**: When porting DXVK to macOS (MoltenVK), any code path that binds a null Vulkan buffer
as active stream source crashes in `MVKBuffer::getMTLBuffer()`. Audit all DXVK batcher flush
paths that attempt to "restore state" using CPU-only objects with no D3D9/Vulkan backing.

---

## LESSON: Null VkBuffer in updateVertexBufferBindings — MoltenVK nullDescriptor (Session 65)

**Category**: DXVK macOS Porting

**Problem**: Game crashes in dxvk-cs thread at `MVKBuffer::getMTLBuffer() + 20` from
`DxvkContext::updateVertexBufferBindings()` → `vkCmdBindVertexBuffers2`. ARM64: `ldr x0, [x0, #0x58]` with x0=null.

**Root Cause**: `DxvkContext::updateVertexBufferBindings()` iterates pipeline input layout bindings.
If a slot has no buffer (`length() == 0`), DXVK sets `buffers[i] = VK_NULL_HANDLE` and then
calls `vkCmdBindVertexBuffers2` with that null handle included.

Per Vulkan spec, `VK_NULL_HANDLE` in vertex buffers requires `nullDescriptor` (`VK_EXT_robustness2`).
MoltenVK does NOT support `nullDescriptor` — it casts the handle directly to `MVKBuffer*`,
so `VK_NULL_HANDLE` (0x0) → null pointer dereference → SIGSEGV.

**Fix**: Replace `buffers[i] = VK_NULL_HANDLE` with `buffers[i] = m_common->dummyResources().bufferHandle()`.
DXVK already uses `dummyResources().bufferHandle()` for transform feedback null slots in the same
function — same pattern applied to vertex buffer slots.

**DXVK Patch**: Patch 12 in `cmake/dxvk-macos-patches.py`, targets `src/dxvk/dxvk_context.cpp`.

**Rule**: MoltenVK does not support `nullDescriptor` and will crash on any `VK_NULL_HANDLE` passed
as a bound Vulkan resource. Always substitute with `m_common->dummyResources()` handles when
targeting MoltenVK/macOS — never pass `VK_NULL_HANDLE` to `vkCmdBind*` functions.

---

## Lesson (Session 66): DXVK PS1.x Sampler Spec Constants — MSL Undeclared Identifier on macOS (Patch 13)

**Context**: DXVK 2.6.0 + MoltenVK 1.4.1. D3D8/D3D9 terrain shaders fail with 80 Metal errors:
`use of undeclared identifier 's0_cube_shadowSmplr'` etc.
18 `VK_ERROR_INITIALIZATION_FAILED` pipeline failures. All terrain/asset textures invisible.

**Root Cause**: In `dxso_compiler.cpp::emitDclSampler()`:
```cpp
const bool implicit = m_programInfo.majorVersion() < 2 || m_moduleInfo.options.forceSamplerTypeSpecConstants;
```
`majorVersion() < 2` is always true for D3D8/PS1.x shaders. This ALWAYS forces "implicit" mode
(all 5 sampler type variants per slot declared in SPIR-V via spec constants). SPIRV-Cross generates
`ps_main()` with all 5 variants as params. MoltenVK's MSL entry-point wrapper only initializes the
actually-bound 2D descriptors; it does NOT generate dummy variable declarations for the non-2D
variants. Metal compiler then fails on undeclared identifiers like `s0_cubeSmplr`.

**False Lead**: `d3d9.forceSamplerTypeSpecConstants = True` in `dxvk.conf` did fix PS2.x shaders
(errors 99 → 49) where `implicit` previously wasn't forced. For PS1.x, it was always forced by
`majorVersion() < 2`, making the option irrelevant. **Key diagnostic**: if the failing shader hash
is IDENTICAL across option changes, the SPIR-V is NOT changing — look for another condition.

**Fix (DXVK Patch 13)**: Remove `majorVersion() < 2` override: `implicit = forceSamplerTypeSpecConstants` only.
With `False` (default), PS1.x declares only the detected type (2D), SPIRV-Cross generates clean
minimal MSL params, no undeclared identifiers.

**Files**: `cmake/dxvk-macos-patches.py` (Patch 13), `build/../dxso_compiler.cpp` (live), `GeneralsMD/Run/dxvk.conf`

**Rules**:
1. If shader hash is identical across config changes → option is not changing SPIR-V → different code path.
2. MoltenVK SPIRV-Cross does NOT generate null/dummy declarations for unused sampler type variants in MSL.
3. Always clear `*.dxvk-cache` after patching DXVK shader compilation or changing dxvk.conf.
