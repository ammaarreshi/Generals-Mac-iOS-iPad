# GeneralsX @build BenderAI 13/02/2026 Disable Miles/Bink for Linux (Windows-only)
set(GAMEENGINEDEVICE_SRC
#    Include/MilesAudioDevice/MilesAudioManager.h
#    Include/VideoDevice/Bink/BinkVideoPlayer.h (see GeneralsMD/Code/GameEngineDevice/Include/W3DDevice/GameClient/W3DGameClient.h createVideoPlayer)
#    Include/W3DDevice/Common/W3DConvert.h
#    Include/W3DDevice/Common/W3DFunctionLexicon.h
#    Include/W3DDevice/Common/W3DModuleFactory.h
    Include/W3DDevice/Common/W3DRadar.h
#    Include/W3DDevice/Common/W3DThingFactory.h
    Include/W3DDevice/GameClient/BaseHeightMap.h
#    Include/W3DDevice/GameClient/camerashakesystem.h
    Include/W3DDevice/GameClient/FlatHeightMap.h
    Include/W3DDevice/GameClient/HeightMap.h
    Include/W3DDevice/GameClient/Module/W3DDebrisDraw.h
    Include/W3DDevice/GameClient/Module/W3DDefaultDraw.h
    Include/W3DDevice/GameClient/Module/W3DDependencyModelDraw.h
    Include/W3DDevice/GameClient/Module/W3DLaserDraw.h
    Include/W3DDevice/GameClient/Module/W3DModelDraw.h
    Include/W3DDevice/GameClient/Module/W3DOverlordAircraftDraw.h
    Include/W3DDevice/GameClient/Module/W3DOverlordTankDraw.h
    Include/W3DDevice/GameClient/Module/W3DOverlordTruckDraw.h
    Include/W3DDevice/GameClient/Module/W3DPoliceCarDraw.h
    Include/W3DDevice/GameClient/Module/W3DProjectileStreamDraw.h
    Include/W3DDevice/GameClient/Module/W3DPropDraw.h
    Include/W3DDevice/GameClient/Module/W3DRopeDraw.h
    Include/W3DDevice/GameClient/Module/W3DScienceModelDraw.h
    Include/W3DDevice/GameClient/Module/W3DSupplyDraw.h
    Include/W3DDevice/GameClient/Module/W3DTankDraw.h
    Include/W3DDevice/GameClient/Module/W3DTankTruckDraw.h
    Include/W3DDevice/GameClient/Module/W3DTracerDraw.h
    Include/W3DDevice/GameClient/Module/W3DTreeDraw.h
    Include/W3DDevice/GameClient/Module/W3DTruckDraw.h
    Include/W3DDevice/GameClient/TerrainTex.h
    Include/W3DDevice/GameClient/TileData.h
#    Include/W3DDevice/GameClient/W3DAssetManager.h
#    Include/W3DDevice/GameClient/W3DAssetManagerExposed.h
#    Include/W3DDevice/GameClient/W3DBibBuffer.h
#    Include/W3DDevice/GameClient/W3DBridgeBuffer.h
#    Include/W3DDevice/GameClient/W3DBufferManager.h
#    Include/W3DDevice/GameClient/W3DCustomEdging.h
#    Include/W3DDevice/GameClient/W3DCustomScene.h
#    Include/W3DDevice/GameClient/W3DDebugDisplay.h
#    Include/W3DDevice/GameClient/W3DDebugIcons.h
#    Include/W3DDevice/GameClient/W3DDisplay.h
#    Include/W3DDevice/GameClient/W3DDisplayString.h
#    Include/W3DDevice/GameClient/W3DDisplayStringManager.h
#    Include/W3DDevice/GameClient/W3DDynamicLight.h
#    Include/W3DDevice/GameClient/W3DFileSystem.h
    Include/W3DDevice/GameClient/W3DGadget.h
#    Include/W3DDevice/GameClient/W3DGameClient.h
#    Include/W3DDevice/GameClient/W3DGameFont.h
#    Include/W3DDevice/GameClient/W3DGameWindow.h
#    Include/W3DDevice/GameClient/W3DGameWindowManager.h
#    Include/W3DDevice/GameClient/W3DGUICallbacks.h
#    Include/W3DDevice/GameClient/W3DInGameUI.h
#    Include/W3DDevice/GameClient/W3DMirror.h
    Include/W3DDevice/GameClient/W3DMouse.h
#    Include/W3DDevice/GameClient/W3DParticleSys.h
#    Include/W3DDevice/GameClient/W3DPoly.h
    Include/W3DDevice/GameClient/W3DProfilerFrameCapture.h
#    Include/W3DDevice/GameClient/W3DProjectedShadow.h
    Include/W3DDevice/GameClient/W3DPropBuffer.h
#    Include/W3DDevice/GameClient/W3DRoadBuffer.h
#    Include/W3DDevice/GameClient/W3DScene.h
    Include/W3DDevice/GameClient/W3DShaderManager.h
#    Include/W3DDevice/GameClient/W3DShadow.h
#    Include/W3DDevice/GameClient/W3DShroud.h
    Include/W3DDevice/GameClient/W3DSmudge.h
    Include/W3DDevice/GameClient/W3DSnow.h
#    Include/W3DDevice/GameClient/W3DStatusCircle.h
    Include/W3DDevice/GameClient/W3DTerrainBackground.h
    Include/W3DDevice/GameClient/W3DTerrainTracks.h
    Include/W3DDevice/GameClient/W3DTerrainVisual.h
    Include/W3DDevice/GameClient/W3DTreeBuffer.h
    Include/W3DDevice/GameClient/W3DVideoBuffer.h
    Include/W3DDevice/GameClient/W3DView.h
#    Include/W3DDevice/GameClient/W3DVolumetricShadow.h
    Include/W3DDevice/GameClient/W3DWater.h
    Include/W3DDevice/GameClient/W3DWaterTracks.h
#    Include/W3DDevice/GameClient/W3DWaypointBuffer.h
#    Include/W3DDevice/GameClient/W3DWebBrowser.h
    Include/W3DDevice/GameClient/WorldHeightMap.h
#    Include/W3DDevice/GameLogic/W3DGameLogic.h
#    Include/W3DDevice/GameLogic/W3DGhostObject.h
#    Include/W3DDevice/GameLogic/W3DTerrainLogic.h
    Include/Win32Device/Common/Win32BIGFile.h
    Include/Win32Device/Common/Win32BIGFileSystem.h
#    Include/Win32Device/Common/Win32GameEngine.h
#    Include/Win32Device/GameClient/Win32DIKeyboard.h
#    Include/Win32Device/GameClient/Win32DIMouse.h
#    Include/Win32Device/GameClient/Win32Mouse.h
#    Source/MilesAudioDevice/MilesAudioManager.cpp
#    Source/VideoDevice/Bink/BinkVideoPlayer.cpp (Bink SDK Windows-only, using stubs)
#    Source/W3DDevice/Common/System/W3DFunctionLexicon.cpp
    Source/W3DDevice/Common/System/W3DRadar.cpp
#    Source/W3DDevice/Common/Thing/W3DModuleFactory.cpp
#    Source/W3DDevice/Common/Thing/W3DThingFactory.cpp
#    Source/W3DDevice/Common/W3DConvert.cpp
    Source/W3DDevice/GameClient/BaseHeightMap.cpp
    Source/W3DDevice/GameClient/CameraShakeSystem.cpp
    Source/W3DDevice/GameClient/Drawable/Draw/W3DDebrisDraw.cpp
    Source/W3DDevice/GameClient/Drawable/Draw/W3DDefaultDraw.cpp
    Source/W3DDevice/GameClient/Drawable/Draw/W3DDependencyModelDraw.cpp
    Source/W3DDevice/GameClient/Drawable/Draw/W3DLaserDraw.cpp
    Source/W3DDevice/GameClient/Drawable/Draw/W3DModelDraw.cpp
    Source/W3DDevice/GameClient/Drawable/Draw/W3DOverlordAircraftDraw.cpp
    Source/W3DDevice/GameClient/Drawable/Draw/W3DOverlordTankDraw.cpp
    Source/W3DDevice/GameClient/Drawable/Draw/W3DOverlordTruckDraw.cpp
    Source/W3DDevice/GameClient/Drawable/Draw/W3DPoliceCarDraw.cpp
    Source/W3DDevice/GameClient/Drawable/Draw/W3DProjectileStreamDraw.cpp
    Source/W3DDevice/GameClient/Drawable/Draw/W3DPropDraw.cpp
    Source/W3DDevice/GameClient/Drawable/Draw/W3DRopeDraw.cpp
    Source/W3DDevice/GameClient/Drawable/Draw/W3DScienceModelDraw.cpp
    Source/W3DDevice/GameClient/Drawable/Draw/W3DSupplyDraw.cpp
    Source/W3DDevice/GameClient/Drawable/Draw/W3DTankDraw.cpp
    Source/W3DDevice/GameClient/Drawable/Draw/W3DTankTruckDraw.cpp
    Source/W3DDevice/GameClient/Drawable/Draw/W3DTracerDraw.cpp
    Source/W3DDevice/GameClient/Drawable/Draw/W3DTreeDraw.cpp
    Source/W3DDevice/GameClient/Drawable/Draw/W3DTruckDraw.cpp
    Source/W3DDevice/GameClient/FlatHeightMap.cpp
    Source/W3DDevice/GameClient/GUI/Gadget/W3DCheckBox.cpp
    Source/W3DDevice/GameClient/GUI/Gadget/W3DComboBox.cpp
    Source/W3DDevice/GameClient/GUI/Gadget/W3DHorizontalSlider.cpp
    Source/W3DDevice/GameClient/GUI/Gadget/W3DListBox.cpp
    Source/W3DDevice/GameClient/GUI/Gadget/W3DProgressBar.cpp
    Source/W3DDevice/GameClient/GUI/Gadget/W3DPushButton.cpp
    Source/W3DDevice/GameClient/GUI/Gadget/W3DRadioButton.cpp
    Source/W3DDevice/GameClient/GUI/Gadget/W3DStaticText.cpp
    Source/W3DDevice/GameClient/GUI/Gadget/W3DTabControl.cpp
    Source/W3DDevice/GameClient/GUI/Gadget/W3DTextEntry.cpp
    Source/W3DDevice/GameClient/GUI/Gadget/W3DVerticalSlider.cpp
    Source/W3DDevice/GameClient/GUI/GUICallbacks/W3DControlBar.cpp
#    Source/W3DDevice/GameClient/GUI/GUICallbacks/W3DMainMenu.cpp
#    Source/W3DDevice/GameClient/GUI/GUICallbacks/W3DMOTD.cpp
#    Source/W3DDevice/GameClient/GUI/W3DGameFont.cpp
#    Source/W3DDevice/GameClient/GUI/W3DGameWindow.cpp
#    Source/W3DDevice/GameClient/GUI/W3DGameWindowManager.cpp
    Source/W3DDevice/GameClient/HeightMap.cpp
#    Source/W3DDevice/GameClient/Shadow/W3DBufferManager.cpp
#    Source/W3DDevice/GameClient/Shadow/W3DProjectedShadow.cpp
#    Source/W3DDevice/GameClient/Shadow/W3DShadow.cpp
#    Source/W3DDevice/GameClient/Shadow/W3DVolumetricShadow.cpp
    Source/W3DDevice/GameClient/TerrainTex.cpp
    Source/W3DDevice/GameClient/TileData.cpp
#    Source/W3DDevice/GameClient/W3DAssetManager.cpp
#    Source/W3DDevice/GameClient/W3DAssetManagerExposed.cpp
#    Source/W3DDevice/GameClient/W3DBibBuffer.cpp
#    Source/W3DDevice/GameClient/W3DBridgeBuffer.cpp
#    Source/W3DDevice/GameClient/W3DCustomEdging.cpp
#    Source/W3DDevice/GameClient/W3DDebugDisplay.cpp
#    Source/W3DDevice/GameClient/W3DDebugIcons.cpp
#    Source/W3DDevice/GameClient/W3DDisplay.cpp
#    Source/W3DDevice/GameClient/W3DDisplayString.cpp
#    Source/W3DDevice/GameClient/W3DDisplayStringManager.cpp
#    Source/W3DDevice/GameClient/W3DDynamicLight.cpp
#    Source/W3DDevice/GameClient/W3DFileSystem.cpp
#    Source/W3DDevice/GameClient/W3DGameClient.cpp
#    Source/W3DDevice/GameClient/W3DInGameUI.cpp
    Source/W3DDevice/GameClient/W3DMouse.cpp
#    Source/W3DDevice/GameClient/W3DParticleSys.cpp
#    Source/W3DDevice/GameClient/W3DPoly.cpp
    Source/W3DDevice/GameClient/W3DProfilerFrameCapture.cpp
    Source/W3DDevice/GameClient/W3DPropBuffer.cpp
#    Source/W3DDevice/GameClient/W3DRoadBuffer.cpp
#    Source/W3DDevice/GameClient/W3DScene.cpp
    Source/W3DDevice/GameClient/W3DShaderManager.cpp
#    Source/W3DDevice/GameClient/W3DShroud.cpp
    Source/W3DDevice/GameClient/W3DSmudge.cpp
    Source/W3DDevice/GameClient/W3DSnow.cpp
#    Source/W3DDevice/GameClient/W3DStatusCircle.cpp
    Source/W3DDevice/GameClient/W3DTerrainBackground.cpp
    Source/W3DDevice/GameClient/W3DTerrainTracks.cpp
    Source/W3DDevice/GameClient/W3DTerrainVisual.cpp
    Source/W3DDevice/GameClient/W3DTreeBuffer.cpp
    Source/W3DDevice/GameClient/W3DVideoBuffer.cpp
    Source/W3DDevice/GameClient/W3DView.cpp
#    Source/W3DDevice/GameClient/W3dWaypointBuffer.cpp
#    Source/W3DDevice/GameClient/W3DWebBrowser.cpp
    Source/W3DDevice/GameClient/Water/W3DWater.cpp
    Source/W3DDevice/GameClient/Water/W3DWaterTracks.cpp
    Source/W3DDevice/GameClient/WorldHeightMap.cpp
#    Source/W3DDevice/GameLogic/W3DGameLogic.cpp
#    Source/W3DDevice/GameLogic/W3DGhostObject.cpp
#    Source/W3DDevice/GameLogic/W3DTerrainLogic.cpp
    Source/Win32Device/Common/Win32BIGFile.cpp
    Source/Win32Device/Common/Win32BIGFileSystem.cpp
#    Source/Win32Device/Common/Win32GameEngine.cpp
#    Source/Win32Device/GameClient/Win32DIMouse.cpp
#    Source/Win32Device/GameClient/Win32Mouse.cpp
)

# GeneralsX @build BenderAI 13/02/2026 Platform-specific file implementations
if(WIN32)
    # Windows-specific files (use Windows API for filesystem)
    list(APPEND GAMEENGINEDEVICE_SRC
        Include/Win32Device/Common/Win32LocalFile.h
        Include/Win32Device/Common/Win32LocalFileSystem.h
        Source/Win32Device/Common/Win32LocalFile.cpp
        Source/Win32Device/Common/Win32LocalFileSystem.cpp
#        Source/Win32Device/Common/Win32OSDisplay.cpp
#        Source/Win32Device/GameClient/Win32DIKeyboard.cpp
    )
endif()

# Add C++ 17 FileSystem implementation for non-VS6 builds
if(NOT IS_VS6_BUILD)
    list(APPEND GAMEENGINEDEVICE_SRC
        Include/StdDevice/Common/StdBIGFile.h
        Include/StdDevice/Common/StdBIGFileSystem.h
        Include/StdDevice/Common/StdLocalFile.h
        Include/StdDevice/Common/StdLocalFileSystem.h
        Source/StdDevice/Common/StdBIGFile.cpp
        Source/StdDevice/Common/StdBIGFileSystem.cpp
        Source/StdDevice/Common/StdLocalFile.cpp
        Source/StdDevice/Common/StdLocalFileSystem.cpp
    )
endif()

add_library(corei_gameenginedevice_private INTERFACE)
add_library(corei_gameenginedevice_public INTERFACE)

target_sources(corei_gameenginedevice_private INTERFACE ${GAMEENGINEDEVICE_SRC})

target_include_directories(corei_gameenginedevice_public INTERFACE
    Include
)

target_link_libraries(corei_gameenginedevice_private INTERFACE
    corei_always
    corei_main
)

# GeneralsX @build BenderAI 13/02/2026 Platform-specific multimedia stubs
target_link_libraries(corei_gameenginedevice_public INTERFACE
    corei_gameengine_public
)

if(WIN32)
    target_link_libraries(corei_gameenginedevice_public INTERFACE
        binkstub
        d3d8lib
        milesstub
    )
endif()

# GeneralsX @build BenderAI 13/02/2026 OpenAL audio library for Linux
# GeneralsX @bugfix fbraz 10/03/2026 Use FetchContent target when available, fallback to find_package
# GeneralsX @refactor fbraz3 31/03/2026 OpenALAudioDevice moved from GeneralsMD to Core (shared by Generals and Zero Hour)
if(SAGE_USE_OPENAL)
    if(TARGET OpenAL::OpenAL)
        # FetchContent-provided openal-soft: interface includes propagated via target
        target_link_libraries(corei_gameenginedevice_public INTERFACE OpenAL::OpenAL)
    else()
        # System install of OpenAL (Linux with libopenal-dev, etc.)
        find_package(OpenAL REQUIRED)
        target_include_directories(corei_gameenginedevice_public INTERFACE ${OPENAL_INCLUDE_DIR})
        target_link_libraries(corei_gameenginedevice_public INTERFACE ${OPENAL_LIBRARY})
    endif()

    target_sources(corei_gameenginedevice_private INTERFACE
        Include/OpenALAudioDevice/OpenALAudioManager.h
        Include/OpenALAudioDevice/OpenALAudioStream.h
        Source/OpenALAudioDevice/OpenALAudioManager.cpp
        Source/OpenALAudioDevice/OpenALAudioStream.cpp
        Source/OpenALAudioDevice/OpenALAudioCache.cpp
    )
    # OpenALAudioCache.h is a private header co-located with its source
    target_include_directories(corei_gameenginedevice_private INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/OpenALAudioDevice
    )
    # FFmpegFile.cpp is required by OpenALAudioCache for audio decoding;
    # add it here when RTS_BUILD_OPTION_FFMPEG has not already provided it.
    if(NOT RTS_BUILD_OPTION_FFMPEG)
        target_sources(corei_gameenginedevice_private INTERFACE
            Source/VideoDevice/FFmpeg/FFmpegFile.cpp
        )
    endif()
endif()

# GeneralsX @feature fbraz 11/06/2026 MiniAudio audio library (alternative to OpenAL)
# miniaudio is a single-header C library that replaces OpenAL + FFmpeg for audio decoding/playback.
# Reference: https://miniaud.io/
# Reference: https://github.com/feliwir/CnC_Generals_Zero_Hour (MiniAudio backend by Stephan Vedder)
if(SAGE_USE_MINIAUDIO)
    if(NOT TARGET miniaudio_lib)
        message(FATAL_ERROR "SAGE_USE_MINIAUDIO is ON but miniaudio_lib target not found. Include cmake/miniaudio.cmake first.")
    endif()

    target_link_libraries(corei_gameenginedevice_public INTERFACE miniaudio_lib)

    target_sources(corei_gameenginedevice_private INTERFACE
        Include/MiniAudioDevice/MiniAudioCompat.h
        Include/MiniAudioDevice/MiniAudioManager.h
        Include/MiniAudioDevice/MiniAudioStream.h
        Source/MiniAudioDevice/MiniAudioManager.cpp
        Source/MiniAudioDevice/MiniAudioStream.cpp
    )
endif()

if(RTS_BUILD_OPTION_FFMPEG)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavcodec libavformat libavutil libswscale)

    if(FFMPEG_FOUND)
        target_sources(corei_gameenginedevice_private INTERFACE
            Include/VideoDevice/FFmpeg/FFmpegFile.h
            Include/VideoDevice/FFmpeg/FFmpegVideoPlayer.h
            Source/VideoDevice/FFmpeg/FFmpegFile.cpp
            Source/VideoDevice/FFmpeg/FFmpegVideoPlayer.cpp
        )
        target_include_directories(corei_gameenginedevice_private INTERFACE ${FFMPEG_INCLUDE_DIRS})

        include(${CMAKE_SOURCE_DIR}/cmake/ffmpeg_framework_fix.cmake)
        sage_fix_ffmpeg_framework_flags()

        target_link_libraries(corei_gameenginedevice_private INTERFACE PkgConfig::FFMPEG)
        target_compile_definitions(corei_gameenginedevice_public INTERFACE RTS_HAS_FFMPEG)
    endif()
endif()
