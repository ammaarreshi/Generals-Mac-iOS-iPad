ewsdesw# MiniAudio Implementation - Session Notes (2026-06-15)

## Overview

Implemented miniaudio (miniaud.io) as a single-header audio backend replacement for OpenAL + FFmpeg in GeneralsX (Zero Hour + Generals base). miniaudio handles both audio playback and decoding (WAV/MP3/FLAC natively), reducing two dependencies (OpenAL + FFmpeg for audio) to one.

Based on [feliwir's fork](https://github.com/feliwir/CnC_Generals_Zero_Hour) by Stephan Vedder (June 2026).

---

## Architecture

### Before (OpenAL + FFmpeg)
```
Game sounds (.wav) → FFmpeg decodes → OpenAL plays via buffers
Music (.mp3)       → FFmpeg decodes → OpenAL plays via streams
Video frames       → FFmpeg decodes → display
Video audio        → FFmpeg extracts → OpenALAudioStream plays
```

### After (MiniAudio + FFmpeg for video only)
```
Game sounds (.wav) → FFmpeg decodes → ma_audio_buffer_init_copy → miniaudio plays
Music (.mp3)       → FFmpeg decodes → ma_audio_buffer_init_copy → miniaudio plays
Video frames       → FFmpeg decodes → display
Video audio        → FFmpeg extracts → MiniAudioStream accumulates → miniaudio plays
```

**Note**: miniaudio's built-in MP3 decoder hangs when streaming through the game's VFS. We use FFmpeg for all audio decoding (same as OpenAL path) and miniaudio only for playback.

---

## Files Created

| File | Description |
|---|---|
| `cmake/miniaudio.cmake` | FetchContent dependency (single-header, v0.11.25) |
| `Core/GameEngineDevice/Include/MiniAudioDevice/MiniAudioManager.h` | Header (derived from feliwir's fork) |
| `Core/GameEngineDevice/Source/MiniAudioDevice/MiniAudioManager.cpp` | Full AudioManager implementation (~1100 lines) |
| `Core/GameEngineDevice/Include/MiniAudioDevice/MiniAudioStream.h` | Video audio stream header |
| `Core/GameEngineDevice/Source/MiniAudioDevice/MiniAudioStream.cpp` | Video audio stream implementation |

## Files Modified

| File | Change |
|---|---|
| `CMakeLists.txt` (root) | Added `include(cmake/miniaudio.cmake)` |
| `CMakePresets.json` | `linux64-deploy`/`macos-vulkan` → MiniAudio default; added `linux64-openal`/`macos-openal` legacy presets |
| `cmake/config-build.cmake` | Added `SAGE_USE_MINIAUDIO` propagation via `core_config` |
| `Core/GameEngineDevice/CMakeLists.txt` | Added `SAGE_USE_MINIAUDIO` block with sources and link |
| `GeneralsMD/Code/GameEngineDevice/CMakeLists.txt` | Added `miniaudio_lib` link + `SKIP_PRECOMPILE_HEADERS` |
| `Generals/Code/GameEngineDevice/CMakeLists.txt` | Added `miniaudio_lib` link + `SKIP_PRECOMPILE_HEADERS` |
| `GeneralsMD/.../SDL3GameEngine.cpp` | Added `#ifdef SAGE_USE_MINIAUDIO` instantiation |
| `Generals/.../SDL3GameEngine.cpp` | Added `#ifdef SAGE_USE_MINIAUDIO` instantiation |
| `GeneralsMD/.../FFmpegVideoPlayer.cpp` | Added `#ifdef SAGE_USE_MINIAUDIO` blocks for video audio |
| `Core/.../WWAudio/WWAudio.h` | Fixed MilesStub guard for `SAGE_USE_MINIAUDIO` |
| `Core/.../WWAudio/Utils.h` | Fixed MilesStub guard |
| `Core/.../WWAudio/SoundBuffer.h` | Fixed MilesStub guard |
| `Core/.../WWAudio/AudibleSound.h` | Fixed MilesStub guard |
| `Core/.../WWAudio/MilesStub.h` | Updated guard to include `SAGE_USE_MINIAUDIO` |
| `Core/Libraries/Include/Lib/BaseTypeCore.h` | `Byte` typedef: `unsigned char` on macOS, `char` elsewhere |

---

## Bugs Found and Fixed

### 1. `m_fadingAudio` and `m_stoppedAudio` not declared
**Symptom**: Compilation error — undeclared identifiers
**Root cause**: OpenAL implementation uses separate lists (`m_playingSounds`, `m_playing3DSounds`, `m_playingStreams`, `m_fadingAudio`, `m_stoppedAudio`). MiniAudio uses a single `m_playingSounds` list but still references `m_fadingAudio`/`m_stoppedAudio` from the base class pattern.
**Fix**: Added both lists to `MiniAudioManager.h`.

### 2. VFS `tell()` / `seek()` using wrong API
**Symptom**: Compilation error — `handle->file->tell()` and `FILE_SEEK_BEGIN` undefined
**Root cause**: Game's `File` class uses `position()` (not `tell()`) and `File::START`/`File::CURRENT`/`File::END` (not `FILE_SEEK_*`)
**Fix**: Updated VFS callbacks to use correct API.

### 3. `has3DSensitiveStreamsPlaying` logic inverted
**Symptom**: Returns TRUE when it shouldn't
**Root cause**: Checked `m_soundType == AT_Music` then returned TRUE for non-"Game_" events. Should check `!= AT_Music` first.
**Fix**: Corrected logic to match OpenAL implementation.

### 4. `adjustPlayingVolume` / `adjustVolumeOfPlayingAudio` ignoring volume type
**Symptom**: All sounds use same volume multiplier
**Root cause**: Used generic `desiredVolume` without multiplying by type-specific volume (`m_soundVolume`, `m_musicVolume`, etc.)
**Fix**: Added type-based volume selection matching OpenAL pattern.

### 5. `stopAllAudioImmediately` not clearing all lists
**Symptom**: Fading/stopped audio leaked
**Root cause**: Only cleared `m_playingSounds`, not `m_fadingAudio`/`m_stoppedAudio`
**Fix**: Added loops for all three lists.

### 6. `playAudioEvent` initial volume ignoring type
**Symptom**: Music plays at wrong volume initially
**Root cause**: Used `event->getVolume()` without multiplying by type volume
**Fix**: Added type-based volume calculation before `ma_sound_start`.

### 7. `startNextLoop` volume ignoring type
**Symptom**: Looping sounds play at wrong volume
**Same root cause as #6**.
**Fix**: Added type-based volume calculation.

### 8. `SAGE_USE_MINIAUDIO` not propagated to Core libraries
**Symptom**: WW3D2 and other Core libraries compile without `SAGE_USE_MINIAUDIO` defined → `mss.h` not found
**Root cause**: `SAGE_USE_OPENAL` is propagated via `core_config` in `cmake/config-build.cmake`, but `SAGE_USE_MINIAUDIO` was not.
**Fix**: Added `target_compile_definitions(core_config INTERFACE SAGE_USE_MINIAUDIO)` in `config-build.cmake`.

### 9. `Byte` typedef conflict with macOS `MacTypes.h`
**Symptom**: `typedef redefinition with different types ('UInt8' vs 'char')` when miniaudio includes CoreAudio
**Root cause**: Game defines `typedef char Byte` in `BaseTypeCore.h`. macOS defines `typedef UInt8 Byte` in `MacTypes.h`. miniaudio's CoreAudio backend includes `MacTypes.h`.
**Fix**: Changed `BaseTypeCore.h` to use `#ifdef __APPLE__` → `typedef unsigned char Byte` (matches macOS), else `typedef char Byte`.

### 10. miniaudio CMake node libraries linking failure
**Symptom**: `libminiaudio_reverb_node.dylib` etc. fail to link (undefined symbols)
**Root cause**: miniaudio's own CMakeLists.txt builds optional node libraries as shared libs without proper linking to core miniaudio.
**Fix**: Changed `cmake/miniaudio.cmake` to use `FetchContent_Populate` only (not `FetchContent_MakeAvailable`), creating a simple interface target without building miniaudio's CMake project.

### 11. `m_engine.channels`/`sampleRate` not in `ma_engine`
**Symptom**: Compilation error on CI
**Root cause**: These fields don't exist in miniaudio 0.11.x `ma_engine` struct.
**Fix**: Removed from diagnostic log line.

### 12. Orphaned `#ifdef INTENSIVE_AUDIO_DEBUG` without matching `#endif`
**Symptom**: `unterminated conditional directive`
**Root cause**: Edit left an orphaned `#ifdef` when refactoring `playAudioEvent`.
**Fix**: Removed orphaned directive.

### 13. `getUninterruptable()` misspelling
**Symptom**: `no member named 'getUninterruptable' in 'AudioEventRTS'`
**Root cause**: Method is `getUninterruptible()` (with 'i').
**Fix**: Updated all occurrences.

### 14. `UnsignedIntPtr` undefined
**Symptom**: `unknown type name 'UnsignedIntPtr'`
**Root cause**: Base class uses `UnsignedInt` for `notifyOfAudioCompletion`/`findPlayingAudioFrom`, not `UnsignedIntPtr`.
**Fix**: Changed signatures to use `UnsignedInt`.

### 15. `ma_decoder_config` has no `pVFS` field
**Symptom**: `no member named 'pVFS' in 'ma_decoder_config'`
**Root cause**: VFS is passed via `ma_decoder_init_vfs()`, not via config.
**Fix**: Changed `getFileLengthMS` to use `ma_decoder_init_vfs()`.

### 16. `ma_audio_buffer_init` does NOT copy data
**Symptom**: SIGSEGV on CoreAudio IOThread — `ma_copy_pcm_frames` reads freed memory
**Root cause**: `ma_audio_buffer_init()` only references external data. When the `pcmData` vector goes out of scope, audio callback reads freed memory.
**Fix**: Changed to `ma_audio_buffer_init_copy()` which allocates and copies data into internal storage.

### 17. Video audio format mismatch (float32 vs PCM16)
**Symptom**: Video audio plays at extreme volume with heavy distortion
**Root cause**: After float32→PCM16 conversion, `bytesPerSample` was still 4 (from original format). `maFormat` was set to `ma_format_f32` instead of `ma_format_s16`. miniaudio interpreted PCM16 data as float32.
**Fix**: Track `outputBitsPerSample` separately and use it for format detection after conversion.

### 18. `rebuildSound()` called every frame causing audio restart
**Symptom**: Video audio restarts from beginning every frame
**Root cause**: `update()` called `rebuildSound()` when buffer grew, creating a new sound each frame.
**Fix**: Deferred sound creation — `play()` sets flag, `update()` creates sound once when buffer stops growing (2 stable frames).

### 19. `FFmpegVideoStream::update()` not calling `MiniAudioStream::update()`
**Symptom**: Video sound never created (MiniAudioStream `update()` never called)
**Root cause**: `FFmpegVideoStream::update()` had a no-op for miniaudio.
**Fix**: Added `audioStream->update()` call.

### 20. `stopAllAudioImmediately` / `processFadingList` iterator invalidation
**Symptom**: Potential crash when `playing == NULL` and `continue` is called without incrementing iterator
**Root cause**: Loop uses `it = erase(it)` but has `continue` paths that skip increment.
**Fix**: Always erase-then-advance, or advance-then-release pattern.

### 21. `processPlayingList` not respecting `m_requestStop`
**Symptom**: Sounds marked to stop never actually stop
**Root cause**: Only checked `ma_sound_is_playing()`, not `m_requestStop` flag.
**Fix**: Added `m_requestStop` check before the `is_playing` check.

### 22. `releasePlayingAudio` notifying for PAT_INVALID
**Symptom**: SoundManager gets spurious completion notifications
**Root cause**: `notifyOf2DSampleCompletion` called even for PAT_INVALID type.
**Fix**: Added `else if (release->m_type == PAT_3DSample)` guard.

### 23. `closeDevice` destroying groups while sounds still playing
**Symptom**: Potential use-after-free in audio callback
**Root cause**: Sound groups destroyed before sounds were stopped.
**Fix**: Call `stopAllAudioImmediately()` before destroying any miniaudio objects.

### 24. `playAudioEvent` leaking `ffmpegFile` on success path
**Symptom**: Memory leak for every sound played
**Root cause**: `delete ffmpegFile` only called on error paths.
**Fix**: Always delete after decode loop.

### 25. `playAudioEvent` indentation/scope bug in switch
**Symptom**: `audio->m_sound = sound` and `audio->m_audioBuffer = audioBuffer` assigned inside switch case, not before
**Root cause**: Copy-paste error from OpenAL code which used separate lists.
**Fix**: Assign before switch so it's always set regardless of case.

### 26. MiniAudioStream `play()` not getting engine reference
**Symptom**: `m_engine == NULL` when `play()` is called
**Root cause**: Stream called `TheAudio->getDevice()` which returned the engine pointer, but this was fragile and could fail if timing was wrong.
**Fix**: `getHandleForBink()` now injects engine via `setEngine()` at construction time.

---

## Build Configuration

### Presets (default: MiniAudio)
| Preset | Audio Backend |
|---|---|
| `linux64-deploy` | MiniAudio (default) |
| `macos-vulkan` | MiniAudio (default) |
| `linux64-openal` | OpenAL (legacy) |
| `macos-openal` | OpenAL (legacy) |

### CMake flags
```
SAGE_USE_MINIAUDIO=ON   # Enable miniaudio backend
SAGE_USE_OPENAL=OFF      # Disable OpenAL backend
```

### Copyright Attribution
- `MiniAudioManager.h/cpp`: Copyright 2026 Stephan Vedder + Copyright 2025 Electronic Arts Inc.
- `MiniAudioStream.h/cpp`: Copyright 2025 Electronic Arts Inc.
- Reference: https://github.com/feliwir/CnC_Generals_Zero_Hour

---

## Known Limitations / Future Work

1. **miniaudio's built-in MP3 decoder hangs on VFS** — We use FFmpeg for all audio decoding. This is a known issue with miniaudio + custom VFS for MP3 streaming.

2. **Video audio accumulates all data before playback** — Short intro videos work fine. Long videos (>30s) may have delayed audio start. Could be improved with proper ring-buffer streaming.

3. **`closeAnySamplesUsingFile` is a no-op** — miniaudio manages file lifecycle internally after decoding.

4. **`hasMusicTrackCompleted` always returns FALSE** — Not yet implemented.

5. **`pauseAudioEvent` is empty** — Individual sound pause not implemented.

6. **macOS default audio device routing** — Some users report audio only on headphones, not speakers. Likely a macOS audio routing configuration issue, not a miniaudio bug.

---

## Commit History

```
ed7bcec30 refactor(audio): comprehensive MiniAudio implementation review
8b6b95a70 fix(audio): call MiniAudioStream::update() from FFmpegVideoStream
d441eb052 fix(audio): defer video sound creation until data accumulates
23d64281b fix(audio): fix video audio format detection after float→s16 conversion
10d03292b debug(audio): add diagnostic logging to playAudioEvent
7cb464846 fix(audio): rewrite MiniAudioStream with proper video audio playback
9b8fcc983 fix(audio): use ma_audio_buffer_init_copy to properly copy PCM data
74e72c718 fix(audio): remove m_engine.channels/sampleRate from log line
c3b866c10 fix(audio): resolve Byte typedef conflict with CoreAudio on macOS
b869316e5 fix(audio): remove orphaned ifdef causing unterminated conditional directive
d78c3a1c7 fix(audio): use FFmpeg for audio decoding instead of miniaudio decoders
cd20d30cf debug(audio): add instrumentation to pinpoint hang location in playAudioEvent
66615630b fix(audio): use ma_audio_buffer instead of ma_decoder for PCM data
c20a9a187 fix(audio): remove orphaned ifdef causing unterminated conditional directive
32a7c1cbe fix(audio): resolve compilation errors in MiniAudio implementation
0375b8d27 fix(audio): propagate SAGE_USE_MINIAUDIO via core_config to all targets
19829bdce feat(audio): add MiniAudio backend as alternative to OpenAL+FFmpeg
```
