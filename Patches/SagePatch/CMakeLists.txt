# SagePatch — cross-platform QoL extras for GeneralsX.
#
# Layout: shared C++ in src/common/, platform-specific implementations in
# src/{macos,linux,windows}/. The common code calls a single sagepatch::
# namespace; per-platform sources provide screenshot, brightness, and the
# event-hook mechanism (DYLD __interpose on macOS, LD_PRELOAD + dlsym on Linux).
#
# Engine-side INI overrides (camera height, scroll speed, camera pitch) are
# picked up automatically by the engine — see resources/Override.ini.

if(NOT SAGE_USE_SDL3)
    message(FATAL_ERROR "SagePatch requires SAGE_USE_SDL3=ON.")
endif()

set(SAGE_PATCH_SOURCES
    src/common/Init.cpp
    src/common/KeyHandler.cpp
    src/common/CursorLock.cpp
    src/common/WindowPosition.cpp
)

if(APPLE)
    list(APPEND SAGE_PATCH_SOURCES
        src/macos/interposers_macos.cpp
        src/macos/Screenshot_macos.cpp
        src/macos/Brightness_macos.cpp
    )
elseif(UNIX AND NOT APPLE)
    list(APPEND SAGE_PATCH_SOURCES
        src/linux/interposers_linux.cpp
        src/linux/Screenshot_linux.cpp
        src/linux/Brightness_linux.cpp
    )
elseif(WIN32)
    message(WARNING "SagePatch on Windows: no preload mechanism; the dylib will build but is a no-op until a proxy DLL pattern is implemented.")
    list(APPEND SAGE_PATCH_SOURCES
        src/windows/Stubs_windows.cpp
    )
else()
    message(FATAL_ERROR "SagePatch: unsupported platform.")
endif()

add_library(sage_patch SHARED ${SAGE_PATCH_SOURCES})

# SagePatch sources use modern C++ (scoped enums, brace-init). The legacy engine
# builds at C++98, and without pinning a standard here the target inherits it,
# so a standards-strict compiler (e.g. AppleClang) fails to build sage_patch with
# "scoped enumerations are a C++11 extension". Pin this target to C++17.
set_target_properties(sage_patch PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

target_include_directories(sage_patch PRIVATE
    include
    ${CMAKE_BINARY_DIR}/_deps/sdl3-src/include
)

# SDL3 symbols resolve at load time from the host process — do not link the
# dylib at build time so a wrong RPATH cannot pull in a stale SDL3.
if(APPLE)
    target_link_libraries(sage_patch PRIVATE
        "-undefined dynamic_lookup"
        "-framework CoreGraphics"
        "-framework ApplicationServices"
        "-framework Foundation"
    )
elseif(UNIX)
    target_link_libraries(sage_patch PRIVATE
        ${CMAKE_DL_LIBS}
        SDL3::SDL3
    )
endif()

set_target_properties(sage_patch PROPERTIES
    OUTPUT_NAME "sage_patch"
    PREFIX "lib"
)
if(APPLE)
    set_target_properties(sage_patch PROPERTIES
        SUFFIX ".dylib"
        INSTALL_NAME_DIR "@rpath"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()

target_compile_options(sage_patch PRIVATE
    -fvisibility=hidden
    -fno-exceptions
    -Wall
    -Wextra
    -Wno-unused-parameter
)

target_compile_definitions(sage_patch PRIVATE
    SAGE_PATCH_VERSION="0.1.0-alpha"
)

add_feature_info(SagePatch ON "QoL patch (screenshot, cursor lock, brightness, window snaps, camera/scroll INI overrides)")
