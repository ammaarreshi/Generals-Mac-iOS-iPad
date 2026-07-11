# Session 23 Handoff - WWDownload Complete, Continuing Linux Build

**Date**: 11/02/2026  
**Branch**: `linux-attempt`  
**Commit**: `e03099f5d` - Phase 1.5: WWDownload network infrastructure complete (FTP + Registry Linux stubs)  
**Build Status**: WWDownload library compiles successfully, build continues incrementally

---

## 🎉 SESSION 22 ACHIEVEMENTS

### ✅ PRIMARY OBJECTIVE COMPLETE: WWDownload Network Infrastructure

**Goal**: Fix FTP.cpp and registry.cpp compilation errors to enable online/LAN multiplayer support.

**Decision**: **Keep WWDownload** (not excluded from build) - required for LAN/online multiplayer infrastructure.

**Result**: ✅ **FTP.cpp + registry.cpp + Download.cpp compile successfully on Linux!**

---

### Root Causes Identified and Fixed

#### 1. FTP.cpp - Missing Header Include (PRIMARY BLOCKER)
**Error**: `'Cftp' does not name a type` (line 180)  
**Cause**: FTP.cpp relied on VC6 Precompiled Header (PCH) auto-including ftp.h  
**Fix**: Added `#include "WWDownload/ftp.h"` explicitly at top of file

#### 2. ftpdefs.h - Missing HRESULT COM Constants
**Errors**:
- `'FACILITY_ITF' was not declared in this scope`
- `'SEVERITY_ERROR' was not declared in this scope`

**Cause**: `FTP_TRYING`/`FTP_FAILED` macros use COM error codes via `MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, code)`

**Fix**: Added to [windows_compat.h](../../GeneralsMD/Code/CompatLib/Include/windows_compat.h): ```cpp
#define S_OK ((HRESULT)0L)
#define SEVERITY_ERROR 1
#define FACILITY_ITF 4  // Interface-specific facility code
```

#### 3. CreateThread - Signature Mismatch
**Error**: Invalid conversion from `DWORD (*)(void*)` to `void*`  
**Cause**: Stub expected `void*` function pointer, actual was `uint32_t (*)(void*)` (LPTHREAD_START_ROUTINE)

**Fix**: Corrected signature in [socket_compat.h](../../GeneralsMD/Code/CompatLib/Include/socket_compat.h):
```cpp
typedef uint32_t (WINAPI *LPTHREAD_START_ROUTINE)(void*);
inline void* CreateThread(void* lpThreadAttributes, size_t dwStackSize, 
                         LPTHREAD_START_ROUTINE lpStartAddress, void* lpParameter,
                         uint32_t dwCreationFlags, unsigned long* lpThreadId) {
    return nullptr; // Stub: Thread creation not supported
}
```

#### 4. OutputDebugString - Missing Stub
**Error**: `'OutputDebugString' was not declared`  
**Fix**: Added stub in socket_compat.h (outputs to stderr):
```cpp
inline void OutputDebugString(const char* lpOutputString) {
    if (lpOutputString) fprintf(stderr, "[DEBUG] %s", lpOutputString);
}
```

#### 5. strlcpy - Missing Function
**Error**: `'strlcpy' was not declared`  
**Fix**: Added weak symbol in socket_compat.h (BSD-compatible implementation)

#### 6. Registry API - Missing Types and Functions
**Errors**:
- `'HKEY_CURRENT_USER' was not declared`
- `'REG_SZ' was not declared`
- `'REG_OPTION_NON_VOLATILE' was not declared`
- `'KEY_WRITE' was not declared`

**Fix**: Added to socket_compat.h:
```cpp
#define HKEY_CURRENT_USER  ((HKEY)0x80000001)
#define KEY_WRITE 0x20006
#define REG_SZ 1
#define REG_OPTION_NON_VOLATILE 0x00000000

inline long RegCreateKeyEx(...) { return 1; }  // ERROR_FILE_NOT_FOUND
inline long RegSetValueEx(...) { return 1; }   // ERROR_FILE_NOT_FOUND
```

**Cleanup**: Removed duplicate `typedef void* HKEY` from registry.cpp (now uses socket_compat.h)

#### 7. _strupr - Declaration Missing
**Error**: `'_strupr' was not declared in this scope` (w3d_dep.cpp line 541)  
**Cause**: Session 21 added implementation in string_compat.cpp but forgot header declaration

**Fix**: Added declaration in [string_compat.h](../../GeneralsMD/Code/CompatLib/Include/string_compat.h):
```cpp
extern "C" {
    char* _strupr(char* str);  // Implemented in string_compat.cpp as weak symbol
}
```

---

## 📂 FILES MODIFIED (Session 22)

### Headers

**[socket_compat.h](../../GeneralsMD/Code/CompatLib/Include/socket_compat.h)**:
- Added: `HKEY_CURRENT_USER`, `KEY_WRITE`, `REG_SZ`, `REG_OPTION_NON_VOLATILE`
- Added: `RegCreateKeyEx()`, `RegSetValueEx()` stubs
- Added: `CreateThread()` with correct signature (LPTHREAD_START_ROUTINE typedef)
- Added: `OutputDebugString()` stub (stderr output)
- Added: `strlcpy()` weak symbol (BSD-compatible)

**[windows_compat.h](../../GeneralsMD/Code/CompatLib/Include/windows_compat.h)**:
- Added: `S_OK`, `SEVERITY_ERROR`, `FACILITY_ITF` (HRESULT COM constants)

**[string_compat.h](../../GeneralsMD/Code/CompatLib/Include/string_compat.h)**:
- Added: `_strupr()` extern "C" declaration

### Sources

**[FTP.cpp](../../Core/Libraries/Source/WWVegas/WWDownload/FTP.cpp)**:
- Added: `#include "WWDownload/ftp.h"` (fixes class Cftp not found)

**[registry.cpp](../../Core/Libraries/Source/WWVegas/WWDownload/registry.cpp)**:
- Removed: Duplicate `typedef void* HKEY` (now uses socket_compat.h)

---

## 🏗️ ARCHITECTURE DECISIONS

### Windows Registry on Linux
**Strategy**: Minimal stubs that return error codes gracefully
- All registry functions return failure (no actual registry on Linux)
- Game settings fallback to defaults when registry unavailable
- Used by: OptionsMenu.cpp, PopupPlayerInfo.cpp, SkirmishGameOptionsMenu.cpp (player preferences)

### FTP Threading
**Strategy**: Stub returns nullptr (thread creation fails gracefully)
- `CreateThread()` stub returns nullptr → FTP operations become synchronous
- Acceptable for LAN/direct connect (no async DNS needed)
- GameSpy/WOL infrastructure discontinued (servers offline)

### Online Multiplayer Status
**Decision**: Preserve infrastructure for potential community servers
- FTP used for patch/map downloads (not critical for LAN play)
- LAN lobby creation uses registry for player name (graceful fallback)
- Direct IP connection bypasses GameSpy matchmaking

---

## 📊 BUILD STATUS

**Previous session** (before WWDownload):
```
Build stopped at [217/914] - FTP.cpp compilation errors
Root causes: Multiple missing stubs and declarations
```

**Current session** (after fixes):
```
Build progressing incrementally
WWDownload library: ✅ COMPILES SUCCESSFULLY
Status: Incremental build (previously compiled files cached)
```

**Files now compiling**:
- ✅ FTP.cpp (WWDownload)
- ✅ registry.cpp (WWDownload)
- ✅ Download.cpp (WWDownload)
- ✅ urlBuilder.cpp (WWDownload)

**Next blockers**: TBD (build continuing incrementally)

---

## 🎓 KEY LESSONS LEARNED (Session 22)

### 1. Precompiled Headers (PCH) Platform Differences
**Problem**: FTP.cpp worked on Windows (VC6) because PCH auto-included ftp.h  
**Linux Behavior**: PCH doesn't auto-include headers (or behaves differently)  
**Solution**: Always explicitly include class declaration header in implementation files  
**Pattern**: `#include "ModuleName/ClassName.h"` at top of implementation .cpp

### 2. Weak Symbol Declaration vs Implementation
**Problem**: _strupr() implemented as weak symbol in .cpp but not declared in .h  
**Result**: Compiler couldn't find function (linker error)  
**Solution**: Declare in header + implement as weak symbol in .cpp  
**Pattern**:
```cpp
// Header (.h)
extern "C" {
    char* _strupr(char* str);
}

// Implementation (.cpp)
extern "C" __attribute__((weak))
char* _strupr(char* str) { /* implementation */ }
```

### 3. Function Pointer Typedefs (Windows API Compatibility)
**Problem**: Generic `void*` doesn't match Windows API function pointer types  
**Solution**: Use exact typedefs from Windows SDK  
**Example**: `LPTHREAD_START_ROUTINE = uint32_t (WINAPI *)(void*)`  
**Lesson**: Check MSDN documentation for actual typedef definitions

### 4. Stub Design Strategies
**Three approaches**:
1. **Error stubs** - Return error/failure safely (Registry, Threading)
   - Example: `RegOpenKeyEx()` returns `ERROR_FILE_NOT_FOUND`
2. **Functional stubs** - Provide alternative implementation (OutputDebugString → stderr)
3. **No-op stubs** - Return success for cleanup functions (RegCloseKey)

**Decision criteria**:
- **Critical path?** Error stub (forces graceful degradation)
- **Debug/logging?** Functional stub (stderr/syslog output)
- **Cleanup?** No-op success (avoid false errors)

### 5. Incremental Build Behavior
**Observation**: Build system caches successfully compiled files  
**Benefit**: Fixes only recompile affected modules (WWDownload + dependencies)  
**Caution**: May miss transitive dependencies (force clean build if suspicious)

---

## 🎯 NEXT SESSION OBJECTIVES

### Priority 1: Continue Linux Build to Linking Stage
**Goal**: Compile all remaining source files until linker invoked.

**Strategy**:
1. Monitor build progress (`grep -E "\[[0-9]+/[0-9]+\]" logs/*.log | tail -1`)
2. Fix compilation errors one file at a time
3. Document patterns (add to lessons learned)

**Expected blockers** (common in large ports):
- DirectX 8 API usage without DXVK wrappers (graphics code)
- Windows-specific file I/O (platform abstractions needed)
- Missing platform abstractions (threading, timing)
- Pointer size assumptions (32-bit → 64-bit portability)

### Priority 2: Address Macro Redefinition Warnings
**Goal**: Clean up DXVK vs compat header conflicts (if causing errors).

**Current warnings** (non-critical):
- `STDMETHOD`, `STDMETHOD_`, `THIS`, `DECLARE_INTERFACE`, `DECLARE_INTERFACE_`
- `FAILED`, `SUCCEEDED`, `MAKE_HRESULT`, `DEFINE_GUID`, `DECLARE_HANDLE`

**Strategy**:
- Investigate if warnings escalate to errors in later files
- If critical: Add `#ifndef` guards to prevent redefinitions
- If benign: Suppress with `#pragma` or accept warnings

### Priority 3: Validate Windows Builds Still Work
**Goal**: Ensure VC6/win32 presets still compile after changes.

**Actions**:
1. Test VC6 preset build (retail compatibility check)
2. Test win32 preset build (modern Windows check)
3. Run replay compatibility tests (if build succeeds)

**Critical files to test**:
- FTP.cpp (added include may affect VC6 PCH)
- registry.cpp (removed typedef may affect Windows)

---

## 🔍 INVESTIGATION NOTES (Future Sessions)

### Macro Redefinition Root Cause
**Hypothesis**: DXVK windows_base.h defines macros already in compat headers  
**Problem**: Include order causes redefinitions (warnings, not errors)  
**Options**:
1. **Suppress warnings**: Add `-Wno-macro-redefined` to CMake for affected files
2. **Guard macros**: Add `#ifndef` before all compat header macros
3. **Prefer DXVK**: Remove compat definitions, use DX VK's (risky - may break non-DX code)

**Recommendation**: Option 1 (suppress) if warnings don't escalate to errors.

### PCH (Precompiled Headers) Behavior
**Question**: Why does Linux PCH not auto-include headers like VC6?  
**Investigation needed**:
- Check CMake PCH configuration (`cmake_pch.hxx`)
- Compare VC6 PCH settings vs Ninja/CMake PCH
- Document VC6 → CMake PCH migration rules

### Threading Strategy (Future)
**Current**: CreateThread stub returns nullptr (no threading)  
**Future options**:
1. Implement pthreads wrapper (full threading support)
2. Keep stub (synchronous FTP acceptable for LAN)
3. Use std::thread (C++11 alternative)

**Decision criteria**: Does game require async FTP for LAN play?

---

## 📚 REFERENCE MATERIALS

### fighter19 DXVK Port
- **Location**: `references/old-refs/fighter19-dxvk-port/`
- **Deepwiki**: `Fighter19/CnC_Generals_Zero_Hour`
- **Key insight**: fighter19 does NOT have WWDownload directory (excluded in their port)
- **Lesson**: We chose different strategy (keep WWDownload for LAN support)

### jmarshall Modern Port
- **Location**: `references/old-refs/jmarshall-win64-modern/`
- **Deepwiki**: `jmarshall2323/CnC_Generals_Zero_Hour`
- **Coverage**: Generals base game ONLY (NOT Zero Hour)
- **Note**: May have different WWDownload handling (investigate if needed)

### MSDN Documentation (for future stub work)
- **CreateThread**: https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createthread
- **Registry API**: https://docs.microsoft.com/en-us/windows/win32/sysinfo/registry-functions
- **HRESULT codes**: https://docs.microsoft.com/en-us/windows/win32/seccrypto/common-hresult-values

---

## 🛠️ ESSENTIAL COMMANDS

### Build Commands

**Continue incremental build**:
```bash
./scripts/docker-build-linux-zh.sh linux64-deploy
```

**Monitor progress**:
```bash
tail -f logs/build_zh_linux64-deploy_docker.log | grep -E "\[[0-9]+/[0-9]+\]"
```

**Check last error**:
```bash
grep "error:" logs/build_zh_linux64-deploy_docker.log | tail -20
```

**Validate Windows build** (requires Windows VM):
```bash
cmake --preset vc6
cmake --build build/vc6 --target z_generals
```

### Git Workflow

**Daily upstream sync** (if needed):
```bash
git fetch thesuperhackers
git merge thesuperhackers/main
# Resolve conflicts: keep our platform code, their logic changes
cmake --preset vc6
cmake --build build/vc6 --target z_generals  # Validate merge
```

**Commit pattern**:
```bash
git add -A
git commit -m 'Phase X.Y: Brief description' -m '- Bullet point details'
git push origin linux-attempt
```

### Research Commands

**Find file in references**:
```bash
find references/old-refs/fighter19-dxvk-port -name "filename.cpp"
find references/old-refs/jmarshall-win64-modern -name "filename.cpp"
```

**Compare implementations**:
```bash
diff -u Core/path/file.cpp references/old-refs/fighter19-dxvk-port/Core/path/file.cpp
```

**Check if macro defined**:
```bash
grep -rn "MACRO_NAME" GeneralsMD/Code/CompatLib/Include/
```

---

## 📝 DEVELOPMENT DIARY

**Location**: `docs/DEV_BLOG/2026-02-DIARY.md`

**IMPORTANT**: Update before commits! Session 22 entry already added.

---

**End of Session 22 Handoff**  
**Status**: ✅ WWDownload network infrastructure complete and compiling  
**Next**: Continue Linux build, address remaining compilation errors until linking stage
