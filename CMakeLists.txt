cmake_minimum_required(VERSION 3.25)

# Use packagename_ROOT for FindPackage.
if(POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
endif()

# Disable default MSVC setting CRT type so we can set it ourselves.
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

# Disable default MSVC warning level so we can set it ourselves.
if(POLICY CMP0092)
    cmake_policy(SET CMP0092 NEW)
endif()

# Allow specifying MSVC debug configurations.
#if(POLICY CMP0141)
#    cmake_policy(SET CMP0141 NEW)
#endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

include(FeatureSummary)
include(CMakeDependentOption)

# We don't support in tree builds, so help people make the right choice.
if (CMAKE_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
    message(FATAL_ERROR "Building in-source is not supported! Create a build dir and remove ${CMAKE_SOURCE_DIR}/CMakeCache.txt")
endif()

# Top level project, doesn't really affect anything.
project(genzh LANGUAGES C CXX)

# GeneralsX @build BenderAI 10/02/2026
# Linux port: Make CompatLib headers globally accessible so Core libraries (WWLib, WWMath, etc.)
# can access platform compatibility types when building for non-Windows platforms
if(UNIX)
    include_directories(${CMAKE_SOURCE_DIR}/Generals/Code/CompatLib/Include)
endif()

# Compiler cache (ccache) – must be included before compilers.cmake so the
# launcher is registered before CMake finalizes the compiler selection.
include(cmake/ccache.cmake)

# This file handles extra settings wanted/needed for different compilers.
include(cmake/compilers.cmake)

# Debug symbol stripping for Release builds (MinGW)
include(cmake/debug_strip.cmake)

include(FetchContent)

# MinGW-w64 specific configuration
if(MINGW)
    include(cmake/mingw.cmake)
    include(cmake/reactos-atl.cmake)
    include(cmake/widl.cmake)
endif()

# Find/Add build dependencies and stubs shared by all projects
if((WIN32 OR "${CMAKE_SYSTEM}" MATCHES "Windows") AND ${CMAKE_SIZEOF_VOID_P} EQUAL 4)
    include(cmake/miles.cmake)
    include(cmake/bink.cmake)
endif()

# Always include DX8 headers (needed for both Windows and Linux builds)
# On Windows: min-dx8-sdk headers
# On Linux: DXVK headers (for D3D8→Vulkan translation)
include(cmake/dx8.cmake)

# SDL3 windowing/input for Linux builds (Phase 1)
# GeneralsX @build BenderAI 11/02/2026
if(SAGE_USE_SDL3)
    include(cmake/sdl3.cmake)
endif()

# Define a dummy stlport target when not on VC6.
if (IS_VS6_BUILD)
    include(cmake/stlport.cmake)
else()
    add_library(stlport INTERFACE)
endif()

include(cmake/config.cmake)
include(cmake/gamespy.cmake)
include(cmake/lzhl.cmake)

# GeneralsX @bugfix fbraz 10/03/2026 Include openal.cmake here (before Core subdirectory)
# so OpenAL::OpenAL target is available when Core/GameEngineDevice/CMakeLists.txt processes.
# openal.cmake is self-guarded with if(SAGE_USE_OPENAL), so this is always safe to include.
include(cmake/openal.cmake)

# GeneralsX @feature fbraz 11/06/2026 Include miniaudio.cmake here (before Core subdirectory)
# so miniaudio_lib target is available when Core/GameEngineDevice/CMakeLists.txt processes.
# miniaudio.cmake is self-guarded with if(SAGE_USE_MINIAUDIO), so this is always safe to include.
include(cmake/miniaudio.cmake)

# GeneralsX @feature BenderAI 21/04/2026 libcurl for in-game update checker (SAGE_UPDATE_CHECK builds)
# curl.cmake is self-guarded with if(SAGE_UPDATE_CHECK), so always safe to include.
include(cmake/curl.cmake)

# GeneralsX @feature fbraz 03/05/2026 Phase 4: Deterministic math library integration
# gamemath.cmake integrates fdlibm-based GameMath for cross-platform replay validation.
# Upstream reference: Okladnoj, PR #2670
# https://github.com/TheSuperHackers/GeneralsGameCode/pull/2670
# Self-guarded with if(SAGE_USE_DETERMINISTIC_MATH), so always safe to include.
include(cmake/gamemath.cmake)

if (IS_VS6_BUILD)
    # The original max sdk does not compile against a modern compiler.
    # If there is a desire to make this work, then a fixed max sdk needs to be created.
    add_subdirectory(Dependencies/MaxSDK)
endif()
add_subdirectory(Dependencies/Utility)

# To be removed when abandoning VC6
if (IS_VS6_BUILD)
    include_directories(Dependencies/Utility)
    add_compile_options(/FIUtility/CppMacros.h)
endif()

add_subdirectory(resources)

add_subdirectory(Core)

# GeneralsX @bugfix GitHubCopilot 14/04/2026 CompatLib must be configured on UNIX even when
# only Generals (not ZH) is built. It sets up d3d8lib INTERFACE includes for DXVK headers
# (windows_base.h) that core_wwcommon depends on transitively. When ZH is built,
# GeneralsMD/Code/CMakeLists.txt already includes CompatLib via add_subdirectory.
if(UNIX AND NOT RTS_BUILD_ZEROHOUR)
    add_subdirectory(GeneralsMD/Code/CompatLib)
endif()

# SagePatch (casual QoL extras: screenshot, cursor lock, brightness, etc.)
# Must be added before the game targets so sage_patch target exists when we
# declare the dependency below.
if(RTS_BUILD_OPTION_SAGE_PATCH)
    add_subdirectory(Patches/SagePatch)
endif()

# Add main build targets
if(RTS_BUILD_ZEROHOUR)
    add_subdirectory(GeneralsMD)
    if(TARGET sage_patch)
        add_dependencies(z_generals sage_patch)
    endif()
endif()

if(RTS_BUILD_GENERALS)
    add_subdirectory(Generals)
    if(TARGET sage_patch)
        add_dependencies(g_generals sage_patch)
    endif()
endif()

feature_summary(WHAT ENABLED_FEATURES DESCRIPTION "Enabled features:")
feature_summary(WHAT DISABLED_FEATURES DESCRIPTION "Disabled features:")
