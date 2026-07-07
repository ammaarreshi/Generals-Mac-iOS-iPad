#pragma once

#include <string.h>  // For memset, used by GlobalMemoryStatus stub

// GeneralsX @build BenderAI 12/02/2026 Pre-define guards to prevent DXVK conflicts
// CRITICAL: Define these BEFORE including windows_base.h so DXVK skips its incomplete versions!
#ifndef _WIN32
#define _MEMORYSTATUS_DEFINED  // Tell DXVK: we'll provide the full 8-field MEMORYSTATUS later
#define _IUNKNOWN_DEFINED      // Tell DXVK: we'll provide IUnknown via DECLARE_INTERFACE later
#endif

// GeneralsX @build BenderAI 12/02/2026 Include DXVK Windows types FIRST
// CRITICAL: On Linux, DXVK provides core Windows types (WINBOOL, LARGE_INTEGER, PALETTEENTRY, etc.)
// Must be included BEFORE our compatibility layer so d3d8types.h can find them!
#ifndef _WIN32
#include <windows_base.h>  // DXVK's minimal Windows API (DWORD, BOOL, WINBOOL, LARGE_INTEGER, etc.)
#endif

#ifndef CALLBACK
#define CALLBACK
#endif

#if !defined(__stdcall)
#define __stdcall
#endif

// GeneralsX @build BenderAI 12/02/2026 Windows __fastcall passes first 2 args in ECX/EDX registers
#if !defined(__fastcall)
#define __fastcall
#endif

#ifndef WINAPI
#define WINAPI
#endif

#if !defined _MSC_VER
#if !defined(__cdecl)
#if defined __has_attribute && __has_attribute(cdecl) && defined(__i386__)
#define __cdecl __attribute__((cdecl))
#else
#define __cdecl
#endif
#endif /* !defined __cdecl */
#endif

#ifndef __forceinline
#if defined __has_attribute && __has_attribute(always_inline)
#define __forceinline __attribute__((always_inline)) inline
#else
#define __forceinline inline
#endif
#endif

static unsigned int GetDoubleClickTime()
{
  return 500;
}

// CRITICAL: bittype.h from WWLib defines basic Win32 types (DWORD, BYTE, UINT, etc.)
// Must be included BEFORE d3d8.h or any DirectX headers
#include "bittype.h"

// types_compat.h now complements bittype.h instead of conflicting
// bittype.h defines basic types (DWORD, ULONG, etc.)
// types_compat.h adds Windows-specific types (HANDLE, HWND, etc.)
#include "types_compat.h"

// GeneralsX @build BenderAI 10/02/2026 - Win32 window management API stubs
#include "wnd_compat.h"

// GeneralsX @build BenderAI 13/02/2026 pthread-based threading (fighter19 pattern)
#ifndef _WIN32
#include "thread_compat.h"
#endif

// const void* type (Windows API)
#ifndef LPCVOID
typedef const void *LPCVOID;
#endif

// FAILED macro (DirectX HRESULT checking)
#ifndef FAILED
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#endif

#ifndef SUCCEEDED
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#endif

// FAR pointer qualifier (legacy 16-bit Windows, stubbed for modern builds)
#ifndef FAR
#define FAR
#endif

// HRESULT success value (S_OK)
#ifndef S_OK
#define S_OK ((HRESULT)0L)
#endif

// HRESULT severity and facility codes (COM/OLE error codes)
#ifndef SEVERITY_ERROR
#define SEVERITY_ERROR 1
#endif

#ifndef FACILITY_ITF
#define FACILITY_ITF 4  // Interface-specific facility code
#endif

// Note: PALETTEENTRY, LARGE_INTEGER, RGNDATA provided by DXVK windows_base.h on Linux
// (included via d3d8.h). These types are Windows SDK on Windows builds.

#define HIWORD(value) ((((uint32_t)(value) >> 16) & 0xFFFF))
#define LOWORD(value) (((uint32_t)(value) & 0xFFFF))

// Copied from windows_base.h from DXVK-Native
#ifndef MAKE_HRESULT
#define MAKE_HRESULT(sev, fac, code) \
	((HRESULT)(((unsigned long)(sev) << 31) | ((unsigned long)(fac) << 16) | ((unsigned long)(code))))
#endif

#ifndef _MAX_PATH
#define _MAX_PATH 1024
#endif

#ifndef MAX_PATH
#define MAX_PATH _MAX_PATH
#endif

#ifndef _MAX_DIR
#define _MAX_DIR _MAX_PATH
#endif

#ifndef _MAX_FNAME
#define _MAX_FNAME 256
#endif

#ifndef _MAX_EXT
#define _MAX_EXT 256
#endif

// TheSuperHackers @build 10/02/2026 Bender
// Threading functions: GetCurrentThreadId() and Sleep() for Linux
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

// GeneralsX @TheSuperHackers @build BenderAI 11/02/2026 Windows crash dump types (stubbed for Linux)
#ifndef _WIN32
struct _EXCEPTION_POINTERS;
typedef struct _EXCEPTION_POINTERS EXCEPTION_POINTERS;
#endif

// GeneralsX @build BenderAI 11/02/2026 - Windows version info stubs (GameState.cpp)
#ifndef _WIN32

// OSVERSIONINFO structure for version checking (stubbed on Linux)
typedef struct _OSVERSIONINFO {
    DWORD dwOSVersionInfoSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformId;
    char szCSDVersion[128];
} OSVERSIONINFO;

// GetVersionEx stub (always returns false on Linux)
inline BOOL GetVersionEx(OSVERSIONINFO* lpVersionInfo) {
    (void)lpVersionInfo;
    return FALSE; // Not Windows
}

// Locale/time formatting constants (not used on Linux, but needed for compilation)
#define LOCALE_SYSTEM_DEFAULT 0x0800
#define TIME_NOSECONDS 0x0002
#define TIME_FORCE24HOURFORMAT 0x0008
#define TIME_NOTIMEMARKER 0x0004

// Platform ID constants
#define VER_PLATFORM_WIN32_WINDOWS 1
#define VER_PLATFORM_WIN32_NT 2

#endif // !_WIN32

// TheSuperHackers @build 10/02/2026 Bender
// Timing functions: timeBeginPeriod(), timeEndPeriod() for Linux
// GeneralsX @bugfix 07/07/2026 Use the monotonic clock, not gettimeofday: wall time steps
// backward on NTP/carrier sync (frequent on iOS), and every elapsed-time gate in the engine
// (shell updates, animations, network resends) then stalls until the clock catches back up.
static inline DWORD timeGetTime(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (DWORD)((unsigned long long)ts.tv_sec * 1000ull + (unsigned long long)ts.tv_nsec / 1000000ull);
}

static inline DWORD timeBeginPeriod(DWORD period)
{
    // NOOP on Linux (timer resolution is kernel controlled)
    return 0;
}

static inline DWORD timeEndPeriod(DWORD period)
{
    // NOOP on Linux
    return 0;
}

// Only include compat headers if old Dependencies/Utility/Utility/compat.h hasn't been included
// to avoid conflicts between old and new compat systems
#ifndef DEPENDENCIES_UTILITY_COMPAT_H
// Threading functions now defined above, no need for thread_compat.h
// #include "thread_compat.h"  // TheSuperHackers @build 10/02/2026 BenderAI - Commented out (functions now inline above)
#include "tchar_compat.h"
#include "time_compat.h"
#include "string_compat.h"
#include "keyboard_compat.h"
#include "memory_compat.h"
#include "module_compat.h"
#include "wchar_compat.h"
#include "gdi_compat.h"
#include "wnd_compat.h"
#include "file_compat.h"
#include "socket_compat.h"  // GeneralsX @build fbraz 10/02/2026 - Win32 Sockets → POSIX BSD sockets (WWDownload)
//#include "intrin_compat.h"

// ================================================================================================
// MUTEX & ERROR HANDLING STUBS (ClientInstance multi-instance check)
// GeneralsX @build BenderAI 11/02/2026 - Multi-instance protection (always allows on Linux)
// ================================================================================================

// GetLastError - returns pseudo error code (always 0 = success on Linux stub)
inline unsigned long GetLastError() {
    return 0;  // ERROR_SUCCESS
}

// Error constants
#ifndef ERROR_ALREADY_EXISTS
#define ERROR_ALREADY_EXISTS 183  // Windows error code for existing mutex
#endif

// CreateMutex stub - always succeeds on Linux (no actual mutex created)
// Returns dummy handle (1) to indicate success
inline void* CreateMutex(void* lpMutexAttributes, int bInitialOwner, const char* lpName) {
    (void)lpMutexAttributes;
    (void)bInitialOwner;
    (void)lpName;
    return (void*)1;  // Fake handle (non-null = success)
}

// CloseHandle stub - no-op on Linux (nothing to close)
inline int CloseHandle(void* hObject) {
    (void)hObject;
    return 1;  // TRUE
}

// __max and __min - MSVC intrinsics (use safe macro wrapping)
#ifndef __max
#define __max(a,b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef __min
#define __min(a,b) (((a) < (b)) ? (a) : (b))
#endif

// Process spawning stubs (WorldBuilder/external tools)
// GeneralsX @build BenderAI 12/02/2026 - MSVC _spawnl not available on Linux
// Used to launch WorldBuilder.exe (Windows-only map editor) via Main Menu button
// Phase 1 (compilation): Stub returns -1 (error) since .exe won't run on Linux anyway
// Future: Could use wine/proton to launch WorldBuilder, or build native Linux map editor
#ifndef _P_NOWAIT
#define _P_NOWAIT 1  // Async spawn mode (original value unimportant for stub)
#endif

inline int _spawnl(int mode, const char* cmdname, const char* arg0, ...) {
    (void)mode;
    (void)cmdname;
    (void)arg0;
    return -1;  // Error code - external tool not available on Linux
}

#endif
// GeneralsX @build BenderAI 24/02/2026 Phase 5 - malloc.h compatibility for macOS ONLY
// macOS does not ship malloc.h; Linux has it natively and works without this stub.
// Do NOT apply to Linux - it would shadow the real malloc.h and break alloca/memalign.
#ifdef __APPLE__
#ifndef _MALLOC_H_
#define _MALLOC_H_
#include <stdlib.h>
#endif
#endif