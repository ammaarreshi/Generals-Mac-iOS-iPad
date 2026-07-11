# Do we want to build extra SDK stuff or just the game binary?
option(RTS_BUILD_CORE_TOOLS "Build core tools" ON)
option(RTS_BUILD_CORE_EXTRAS "Build core extra tools/tests" OFF)
option(RTS_BUILD_ZEROHOUR "Build Zero Hour code." ON)
option(RTS_BUILD_GENERALS "Build Generals code." ON)
option(RTS_BUILD_OPTION_PROFILE "Build code with the \"Profile\" configuration." OFF)
option(RTS_BUILD_OPTION_PROFILE_TRACY "Build code with Tracy profiling enabled." OFF)
option(RTS_BUILD_OPTION_DEBUG "Build code with the \"Debug\" configuration." OFF)
option(RTS_BUILD_OPTION_ASAN "Build code with Address Sanitizer." OFF)
option(RTS_BUILD_OPTION_VC6_FULL_DEBUG "Build VC6 with full debug info." OFF)
option(RTS_BUILD_OPTION_FFMPEG "Enable FFmpeg support" OFF)

# Linux/SDL3 and OpenAL options (Phase 1 Linux port)
option(SAGE_USE_SDL3 "Use SDL3 for windowing/input (Linux/macOS)" OFF)
option(SAGE_USE_OPENAL "Use OpenAL for audio backend (Linux/macOS)" OFF)
option(SAGE_USE_MINIAUDIO "Use MiniAudio for audio backend (Linux/macOS)" OFF)

# GeneralsX @feature BenderAI 21/04/2026 In-game update checker via GitHub Releases API (SDL3+libcurl builds only)
# Default ON when SDL3 is enabled, but only if the user has not explicitly set SAGE_UPDATE_CHECK.
# An explicit -DSAGE_UPDATE_CHECK=OFF on the cmake command line is always respected.
if(NOT DEFINED CACHE{SAGE_UPDATE_CHECK})
    set(SAGE_UPDATE_CHECK "${SAGE_USE_SDL3}" CACHE BOOL "Enable in-game update check via GitHub Releases API")
endif()

# macOS port option (Phase 5)
option(SAGE_USE_MOLTENVK "Use MoltenVK for Vulkan on macOS (Phase 5 macOS port)" OFF)

# SagePatch — optional QoL features for casual play (screenshot, cursor lock,
# brightness, camera/scroll INI overrides). Compiles to a separate shared lib
# that is loaded via DYLD_INSERT_LIBRARIES (macOS) / LD_PRELOAD (Linux) at runtime.
option(RTS_BUILD_OPTION_SAGE_PATCH "Build SagePatch QoL extras (macOS/Linux, requires SDL3)" ON)

if(NOT RTS_BUILD_ZEROHOUR AND NOT RTS_BUILD_GENERALS)
    set(RTS_BUILD_ZEROHOUR TRUE)
    message("You must select one project to build, building Zero Hour by default.")
endif()

add_feature_info(CoreTools RTS_BUILD_CORE_TOOLS "Build Core Mod Tools")
add_feature_info(CoreExtras RTS_BUILD_CORE_EXTRAS "Build Core Extra Tools/Tests")
add_feature_info(ZeroHourStuff RTS_BUILD_ZEROHOUR "Build Zero Hour code")
add_feature_info(GeneralsStuff RTS_BUILD_GENERALS "Build Generals code")
add_feature_info(ProfileBuild RTS_BUILD_OPTION_PROFILE "Building as a \"Profile\" build")
add_feature_info(DebugBuild RTS_BUILD_OPTION_DEBUG "Building as a \"Debug\" build")
add_feature_info(AddressSanitizer RTS_BUILD_OPTION_ASAN "Building with address sanitizer")
add_feature_info(Vc6FullDebug RTS_BUILD_OPTION_VC6_FULL_DEBUG "Building VC6 with full debug info")
add_feature_info(FFmpegSupport RTS_BUILD_OPTION_FFMPEG "Building with FFmpeg support")
add_feature_info(SDL3Windowing SAGE_USE_SDL3 "Using SDL3 for windowing (Linux)")
add_feature_info(OpenALAudio SAGE_USE_OPENAL "Using OpenAL for audio (Linux)")
add_feature_info(UpdateCheck SAGE_UPDATE_CHECK "In-game update check via GitHub Releases API")
add_feature_info(SagePatch RTS_BUILD_OPTION_SAGE_PATCH "Build SagePatch QoL extras (macOS)")

set(RTS_BUILD_OUTPUT_SUFFIX "" CACHE STRING "Suffix appended to output names of installable targets")

if(RTS_BUILD_ZEROHOUR)
    option(RTS_BUILD_ZEROHOUR_TOOLS "Build tools for Zero Hour" ON)
    option(RTS_BUILD_ZEROHOUR_EXTRAS "Build extra tools/tests for Zero Hour" OFF)
    option(RTS_BUILD_ZEROHOUR_DOCS "Build documentation for Zero Hour" OFF)

    add_feature_info(ZeroHourTools RTS_BUILD_ZEROHOUR_TOOLS "Build Zero Hour Mod Tools")
    add_feature_info(ZeroHourExtras RTS_BUILD_ZEROHOUR_EXTRAS "Build Zero Hour Extra Tools/Tests")
    add_feature_info(ZeroHourDocs RTS_BUILD_ZEROHOUR_DOCS "Build Zero Hour Documentation")
endif()

if(RTS_BUILD_GENERALS)
    option(RTS_BUILD_GENERALS_TOOLS "Build tools for Generals" ON)
    option(RTS_BUILD_GENERALS_EXTRAS "Build extra tools/tests for Generals" OFF)
    option(RTS_BUILD_GENERALS_DOCS "Build documentation for Generals" OFF)

    add_feature_info(GeneralsTools RTS_BUILD_GENERALS_TOOLS "Build Generals Mod Tools")
    add_feature_info(GeneralsExtras RTS_BUILD_GENERALS_EXTRAS "Build Generals Extra Tools/Tests")
    add_feature_info(GeneralsDocs RTS_BUILD_GENERALS_DOCS "Build Generals Documentation")
endif()

if(NOT IS_VS6_BUILD)
    # Because we set CMAKE_CXX_STANDARD_REQUIRED and CMAKE_CXX_EXTENSIONS in the compilers.cmake this should be enforced.
    target_compile_features(core_config INTERFACE cxx_std_20)
endif()

if(IS_VS6_BUILD AND RTS_BUILD_OPTION_VC6_FULL_DEBUG)
    target_compile_options(core_config INTERFACE ${RTS_FLAGS} /Zi)
else()
    target_compile_options(core_config INTERFACE ${RTS_FLAGS})
endif()

# This disables a lot of warnings steering developers to use windows only functions/function names.
if(MSVC)
    target_compile_definitions(core_config INTERFACE _CRT_NONSTDC_NO_WARNINGS _CRT_SECURE_NO_WARNINGS $<$<CONFIG:DEBUG>:_DEBUG_CRT>)
endif()

if(UNIX)
    target_compile_definitions(core_config INTERFACE _UNIX)
    # Ubuntu 24.04+ and macOS have strlcpy/strlcat/wcslcpy/wcslcat in libc
    # GeneralsX @TheSuperHackers @build BenderAI 11/02/2026 Added guards for glibc 2.38+
    target_compile_definitions(core_config INTERFACE 
        HAVE_STRLCPY HAVE_STRLCAT HAVE_WCSLCPY HAVE_WCSLCAT)
endif()

if(RTS_BUILD_OPTION_DEBUG)
    target_compile_definitions(core_config INTERFACE RTS_DEBUG WWDEBUG DEBUG)
else()
    target_compile_definitions(core_config INTERFACE RTS_RELEASE NDEBUG)
endif()

if(RTS_BUILD_OPTION_PROFILE)
    target_compile_definitions(core_config INTERFACE RTS_PROFILE_LEGACY)
endif()

# Define a dummy Tracy target when the build option is disabled.
if(RTS_BUILD_OPTION_PROFILE_TRACY)
    include(cmake/tracy.cmake)
else()
    add_library(core_profile_tracy INTERFACE)
endif()
# Linux port options (Phase 1)
if(SAGE_USE_SDL3)
    target_compile_definitions(core_config INTERFACE SAGE_USE_SDL3)
    message(STATUS "SDL3 windowing backend enabled")
endif()

if(SAGE_USE_OPENAL)
    target_compile_definitions(core_config INTERFACE SAGE_USE_OPENAL)
    message(STATUS "OpenAL audio backend enabled")
endif()

# GeneralsX @feature fbraz 11/06/2026 MiniAudio audio backend (alternative to OpenAL)
if(SAGE_USE_MINIAUDIO)
    target_compile_definitions(core_config INTERFACE SAGE_USE_MINIAUDIO)
    message(STATUS "MiniAudio audio backend enabled")
endif()

# GeneralsX @feature BenderAI 21/04/2026 Update check compile definition
if(SAGE_UPDATE_CHECK)
    target_compile_definitions(core_config INTERFACE SAGE_UPDATE_CHECK)
    message(STATUS "In-game update checker enabled")
endif()

if(SAGE_USE_GLM)
    target_compile_definitions(core_config INTERFACE SAGE_USE_GLM)
    message(STATUS "GLM math library enabled (DirectX 8 replacement)")
endif()

# macOS MoltenVK detection (Phase 5)
# GeneralsX @build BenderAI 24/02/2026 - Phase 5 macOS port
if(APPLE AND SAGE_USE_MOLTENVK)
    find_package(Vulkan REQUIRED COMPONENTS MoltenVK)
    if(NOT Vulkan_FOUND)
        message(FATAL_ERROR "MoltenVK not found. Install from https://vulkan.lunarg.com/sdk/home#mac (LunarG installer to ~/VulkanSDK/<version>/macOS)")
    endif()
    
    target_compile_definitions(core_config INTERFACE SAGE_USE_MOLTENVK)
    target_link_libraries(core_config INTERFACE Vulkan::Vulkan)
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        # On iOS, MoltenVK links as a STATIC library, so its framework dependencies
        # must be linked by the consumer (the macOS dylib resolved these itself).
        target_link_libraries(core_config INTERFACE
            "-framework Metal"
            "-framework IOSurface"
            "-framework CoreGraphics"
            "-framework QuartzCore"
            "-framework Foundation"
            "-framework UIKit"
        )
    endif()
    message(STATUS "MoltenVK (Vulkan on macOS) detected and enabled")
    message(STATUS "  Vulkan SDK: ${Vulkan_INCLUDE_DIRS}")
    message(STATUS "  MoltenVK: Will translate Vulkan to Metal")
endif()