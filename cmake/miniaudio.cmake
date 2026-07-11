# GeneralsX @feature fbraz 11/06/2026 MiniAudio audio library via FetchContent
# miniaudio - Single file audio playback and capture library (miniaud.io)
#
# Replaces OpenAL + FFmpeg for audio decoding/playback with a single dependency.
# FFmpeg is still needed for video decoding (FFmpegVideoPlayer).
#
# Reference: https://miniaud.io/
# Reference: https://github.com/feliwir/CnC_Generals_Zero_Hour (MiniAudio backend by Stephan Vedder)

if(SAGE_USE_MINIAUDIO)
    message(STATUS "Configuring miniaudio via FetchContent...")

    include(FetchContent)

    # GeneralsX @build Codex 10/07/2026 Pin the reviewed backend source for reproducible builds.
    FetchContent_Declare(
        miniaudio
        URL "https://github.com/mackron/miniaudio/archive/refs/tags/0.11.25.tar.gz"
        URL_HASH SHA256=b900edcffe979816e2560a0580b9b1216d674b4f17fbadeca8f777a7f8ab0274
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    # miniaudio is a single-header library; we just need the source directory
    # Use FETCHCONTENT_UPDATES_DISCONNECTED to avoid re-downloading
    FetchContent_GetProperties(miniaudio)
    if(NOT miniaudio_POPULATED)
        FetchContent_Populate(miniaudio)
    endif()

    # Create an interface target for miniaudio (single-header library)
    # We don't use miniaudio's CMakeLists.txt because it builds optional node
    # libraries with incorrect linking. Instead, we use the single-header approach.
    add_library(miniaudio_lib INTERFACE)
    target_include_directories(miniaudio_lib INTERFACE ${miniaudio_SOURCE_DIR})
    target_compile_definitions(miniaudio_lib INTERFACE SAGE_USE_MINIAUDIO)

    # Platform-specific audio backend configuration
    if(APPLE)
        # Use CoreAudio on macOS
        target_compile_definitions(miniaudio_lib INTERFACE
            MA_NO_WASAPI MA_NO_DSOUND MA_NO_WINMM)
    elseif(UNIX)
        # Use ALSA on Linux (PulseAudio/PipeWire via ALSA compat)
        target_compile_definitions(miniaudio_lib INTERFACE
            MA_NO_WASAPI MA_NO_DSOUND MA_NO_WINMM MA_NO_COREAUDIO)
    endif()

    message(STATUS "miniaudio configured: target miniaudio_lib available")
endif()
