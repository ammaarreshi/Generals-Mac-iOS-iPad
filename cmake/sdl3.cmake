# SDL3 windowing/input library for Linux builds
# GeneralsX @build BenderAI 11/02/2026 - Session 26
# SDL3 provides cross-platform windowing, input events, and OS integration
# Used by SDL3GameEngine (replaces Win32 window management on Linux)
#
# Fighter19 pattern: Use find_package (expects system install)
# Our approach: Use FetchContent to download and compile SDL3 directly
# This avoids vcpkg issues with libsystemd and complex dependencies

if(SAGE_USE_SDL3)
    # GeneralsX @build BenderAI 22/02/2026 (updated)
    # Strategy: FetchContent to compile SDL3 + SDL3_image from source
    # Docker environment (ubuntu:24.04) has build dependencies pre-installed
    # This ensures local build compatibility (same glibc, same distro as developer machine)
    # Reference: https://github.com/libsdl-org/SDL/releases/download/release-3.4.2/SDL3-3.4.2.tar.gz
    
    message(STATUS "Configuring SDL3 (v3.4.2) with FetchContent (native build)...")
    
    include(FetchContent)
    
    # SDL3 - Core graphics, input, events, filesystem
    set(SDL3_VERSION "3.4.2")
    set(SDL3_URL "https://github.com/libsdl-org/SDL/releases/download/release-${SDL3_VERSION}/SDL3-${SDL3_VERSION}.tar.gz")
    set(SDL3_URL_HASH "SHA256=ef39a2e3f9a8a78296c40da701967dd1b0d0d6e267e483863ce70f8a03b4050c")
    
    # GeneralsX @bugfix nyanlintun 13/10/2026 iOS: Patches/sdl3-ios.patch derives the view
    # orientation from the window scene instead of the deprecated statusBarOrientation, which
    # on iPadOS 27 left the landscape-only game drawing into a portrait-sized Metal view. (#29)
    set(SDL3_PATCH_ARGS "")
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(SDL3_PATCH_ARGS PATCH_COMMAND ${CMAKE_COMMAND}
            -DPATCH_FILE=${CMAKE_SOURCE_DIR}/Patches/sdl3-ios.patch
            -P ${CMAKE_SOURCE_DIR}/cmake/apply_patch_once.cmake)
    endif()

    FetchContent_Declare(
        SDL3
        URL ${SDL3_URL}
        URL_HASH ${SDL3_URL_HASH}
        ${SDL3_PATCH_ARGS}
    )
    
    # Configure SDL3 build options
    set(SDL_SHARED ON CACHE BOOL "Build SDL3 as shared library" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "Don't build static library" FORCE)
    set(SDL_AUDIO ON CACHE BOOL "Enable audio subsystem" FORCE)
    set(SDL_TIMERS ON CACHE BOOL "Enable timers" FORCE)
    set(SDL_EVENTS ON CACHE BOOL "Enable events" FORCE)
    set(SDL_FILESYSTEM ON CACHE BOOL "Enable filesystem" FORCE)
    set(SDL_RENDER ON CACHE BOOL "Enable render subsystem" FORCE)
    set(SDL_VIDEO ON CACHE BOOL "Enable video subsystem" FORCE)
    
    # Platform support
    set(SDL_WAYLAND ON CACHE BOOL "Enable Wayland support (Linux)" FORCE)
    set(SDL_X11 ON CACHE BOOL "Enable X11 support (Linux)" FORCE)
    set(SDL_CAMERA OFF CACHE BOOL "Disable camera (unused)" FORCE)
    set(SDL_QSPI OFF CACHE BOOL "Disable QSPI (unused)" FORCE)
    
    FetchContent_MakeAvailable(SDL3)
    
    # GeneralsX @bugfix BenderAI 22/02/2026 (updated 24/02/2026 for macOS)
    # Before SDL3_image build: force PNG discovery to platform-specific libpng
    # Linux: System libpng16.so is dynamic shared library
    # macOS: Use Homebrew PNG or system framework
    if(NOT APPLE)
        # Find system shared libpng, bypassing vcpkg's static .a.
        # SDL3_image requires a shared .so but vcpkg only provides static libpng16.a.
        # NO_CMAKE_PATH + NO_CMAKE_FIND_ROOT_PATH skips all vcpkg-injected search paths,
        # so find_library uses only system paths (/usr/lib, /usr/lib64, multilib dirs).
        find_library(PNG_LIBRARY NAMES png16 png NO_CMAKE_PATH NO_CMAKE_FIND_ROOT_PATH)
        find_path(PNG_PNG_INCLUDE_DIR png.h PATH_SUFFIXES libpng16 NO_CMAKE_PATH NO_CMAKE_FIND_ROOT_PATH)
        find_package(PNG REQUIRED MODULE)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        # iOS: no shared libpng exists for the target and SDL3_image rejects a static
        # one. Disable its libpng backend entirely — PNG decoding still works through
        # the stb and Apple ImageIO backends that SDL3_image enables on Apple platforms.
        set(SDLIMAGE_PNG_LIBPNG OFF CACHE BOOL "No libpng on iOS; stb/ImageIO decode PNG" FORCE)
        set(SDLIMAGE_PNG_SHARED OFF CACHE BOOL "No shared libpng on iOS" FORCE)
    else()
        # macOS: Force Homebrew's dynamic libpng, bypassing vcpkg's static .a
        # GeneralsX @build BenderAI 24/02/2026 - Phase 5 macOS port
        # vcpkg provides static libpng16.a but SDL3_image requires dynamic .dylib
        # Homebrew installs libpng16.16.dylib at /usr/local/lib (Intel) or /opt/homebrew/lib (ARM)
        set(PNG_SHARED ON CACHE BOOL "Require PNG as shared library" FORCE)
        # GeneralsX @bugfix BenderAI 25/02/2026 Check Apple Silicon Homebrew (/opt/homebrew) FIRST.
        # A machine with both Homebrew installations (Intel Rosetta + native arm64) could have
        # /usr/local/lib/libpng16.dylib (x86_64) AND /opt/homebrew/lib/libpng16.dylib (arm64).
        # Linking the x86_64 dylib into an arm64 binary produces:
        #   ld: warning: ignoring file '...libpng16.dylib': found architecture 'x86_64', required 'arm64'
        # Always prefer /opt/homebrew (arm64) for arm64 builds.
        if(EXISTS "/opt/homebrew/lib/libpng16.dylib")
            # Apple Silicon Mac (Homebrew prefix: /opt/homebrew)
            set(PNG_INCLUDE_DIR "/opt/homebrew/include" CACHE PATH "PNG include dir (Homebrew)" FORCE)
            set(PNG_LIBRARY "/opt/homebrew/lib/libpng16.dylib" CACHE FILEPATH "PNG library (Homebrew .dylib)" FORCE)
            set(PNG_LIBRARY_DEBUG "/opt/homebrew/lib/libpng16.dylib" CACHE FILEPATH "" FORCE)
            set(PNG_LIBRARY_RELEASE "/opt/homebrew/lib/libpng16.dylib" CACHE FILEPATH "" FORCE)
        elseif(EXISTS "/usr/local/lib/libpng16.dylib")
            # Intel Mac fallback (Homebrew prefix: /usr/local)
            set(PNG_INCLUDE_DIR "/usr/local/include" CACHE PATH "PNG include dir (Homebrew)" FORCE)
            set(PNG_LIBRARY "/usr/local/lib/libpng16.dylib" CACHE FILEPATH "PNG library (Homebrew .dylib)" FORCE)
            set(PNG_LIBRARY_DEBUG "/usr/local/lib/libpng16.dylib" CACHE FILEPATH "" FORCE)
            set(PNG_LIBRARY_RELEASE "/usr/local/lib/libpng16.dylib" CACHE FILEPATH "" FORCE)
        else()
            message(FATAL_ERROR "libpng not found. Install: brew install libpng")
        endif()
        find_package(PNG REQUIRED MODULE)
    endif()
    
    # Tell CMake to find PNG - this should use our explicit system .so above, not vcpkg
    
    # SDL3_image - Image format support (PNG, JPG for cursor ANI loading)
    message(STATUS "Configuring SDL3_image (v3.4.0) with FetchContent (native build)...")
    
    set(SDL3_IMAGE_VERSION "3.4.0")
    set(SDL3_IMAGE_URL "https://github.com/libsdl-org/SDL_image/releases/download/release-${SDL3_IMAGE_VERSION}/SDL3_image-${SDL3_IMAGE_VERSION}.tar.gz")
    set(SDL3_IMAGE_URL_HASH "SHA256=2ceb75eab4235c2c7e93dafc3ef3268ad368ca5de40892bf8cffdd510f29d9d8")
    
    FetchContent_Declare(
        SDL3_image
        URL ${SDL3_IMAGE_URL}
        URL_HASH ${SDL3_IMAGE_URL_HASH}
    )
    
    # Configure SDL3_image build options
    # Note: PNG will use system libpng-dev (installed in Docker, no vcpkg conflicts)
    set(SDL3IMAGE_INSTALL ON CACHE BOOL "Install SDL3_image" FORCE)
    set(SDL3IMAGE_DEPS_SHARED ON CACHE BOOL "Use system shared dependencies" FORCE)
    set(SDL3IMAGE_JPG ON CACHE BOOL "Enable JPG support" FORCE)
    set(SDL3IMAGE_PNG ON CACHE BOOL "Enable PNG support (ANI cursor loading)" FORCE)
    set(SDL3IMAGE_TIF ON CACHE BOOL "Enable TIF support" FORCE)
    set(SDL3IMAGE_WEBP ON CACHE BOOL "Enable WebP support" FORCE)
    set(SDL3IMAGE_AVIF OFF CACHE BOOL "Disable AVIF (optional)" FORCE)
    set(SDL3IMAGE_XCUR ON CACHE BOOL "Enable X cursor support" FORCE)
    
    FetchContent_MakeAvailable(SDL3_image)
    
    # Create unified interface library for linking
    add_library(sdl3lib INTERFACE)
    target_link_libraries(sdl3lib INTERFACE SDL3::SDL3 SDL3_image::SDL3_image)
    
    # Expose include directories
    target_include_directories(sdl3lib INTERFACE 
        "${SDL3_SOURCE_DIR}/include"
        "${sdl3_image_SOURCE_DIR}/include"
    )
    
    message(STATUS "✓ SDL3 (${SDL3_VERSION}) + SDL3_image (${SDL3_IMAGE_VERSION}) configured")
    message(STATUS "  Build approach: Native FetchContent compilation")
    message(STATUS "  PNG support: System libpng-dev (dynamic linking)")
    
endif()
