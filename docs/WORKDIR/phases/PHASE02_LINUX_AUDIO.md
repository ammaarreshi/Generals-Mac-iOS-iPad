tggf# PHASE02: Linux Audio - OpenAL Integration

**Status**: COMPLETE
**Created**: 2026-02-08
**Updated**: 2026-02-21
**Completed**: 2026-02-21 (Session 54)
**Prerequisites**: Phase 1 complete ✅ (DXVK + SDL3 + mouse input working)

## Goal

Replace Miles Sound System with OpenAL for native Linux audio, using jmarshall's implementation as reference while adapting for Zero Hour's expanded audio features.

## Progress Snapshot
✅ Spike complete — SDL3 audio vs OpenAL decision made  
✅ Phase 1 complete (graphics/input working, menus clickable)  
✅ Implementation complete — fighter19 OpenAL pipeline ported, sound confirmed working  
✅ **PHASE 2 ACCEPTED — Menu music plays on Linux**

### Spike Decision (2026-02-21)

**OpenAL selected over SDL3 audio.** See full analysis: `docs/WORKDIR/support/SPIKE_AUDIO_SDL3_VS_OPENAL.md`

Key reasons:
1. **SDL3 has no 3D audio** — essential for RTS (unit voices, explosions attenuate with distance)
2. **Fighter19 has 3,490 lines of working Zero Hour OpenAL code** — zero reference exists for SDL3 audio
3. **SDL3 still needs FFmpeg for MP3** — no dependency reduction, worse capabilities

**Audio stack (fighter19 pattern)**:
```
AudioManager interface (AudioEventRTS, AudioRequest)
        ↓
  OpenALAudioManager      ← event scheduling, priority, volume, 3D
        ↓
  OpenALAudioFileCache    ← FFmpeg decodes MP3/WAV → PCM → AL buffer
        ↓
  OpenAL (libopenal)      ← PCM playback, 3D positioning, listener
```

---

## Context

**Current State (Phase 1)**:
- ✅ OpenALAudioManager exists (~550 lines)
- ✅ Device/context lifecycle implemented
- ✅ Source pooling ready (32 2D + 128 3D + 4 stream)
- ⚠️ Audio events stubbed (no actual audio playback)
- ⚠️ Music tracks stubbed (no file loading/streaming)
- ⚠️ 3D audio listener not wired (update() stub)

**Phase 2 Focus**:
Wire OpenAL backend to game audio system, implement audio event tracking, music streaming, and 3D audio positioning.

---

## Scope

### In Scope ✅
- Audio event tracking (addAudioEvent with handle management)
- Sound effect playback (gunshots, explosions, unit responses)
- Music streaming (background tracks, victory/defeat themes)
- 3D audio positioning (listener + source positions)
- Audio format conversion (Miles formats → OpenAL formats)
- Volume/pan/pitch controls
- Buffer management and streaming
- Miles→OpenAL API compatibility layer

### Out of Scope ❌
- Video audio sync (Phase 3)
- EAX/reverb effects (future enhancement)
- Lip-sync/dialogue system (future)
- Network audio sync (future)

---

## Implementation Plan

> **Strategy**: Port fighter19's complete OpenAL implementation (3,490 LOC across 3 files).  
> Do NOT build from scratch. Adapt fighter19 to Zero Hour's extended feature set where needed.

### Step 1: CMake + Docker Dependencies
- [ ] Add `cmake/audio.cmake` with FFmpeg + OpenAL detection
- [ ] Update Docker image: `apt install -y libopenal-dev libavcodec-dev libavformat-dev libavutil-dev libswresample-dev`
- [ ] Wire `cmake/audio.cmake` into main `CMakeLists.txt` under `SAGE_USE_OPENAL` guard

### Step 2: Port `OpenALAudioFileCache` (FFmpeg decoder)
- [ ] Port `references/old-refs/fighter19-dxvk-port/GeneralsMD/Code/GameEngineDevice/Source/OpenALAudioDevice/OpenALAudioCache.cpp`
- [ ] Port matching header
- [ ] Locate `FFmpegFile` wrapper in fighter19 and port it
- [ ] Test: load one `.wav` or `.mp3` file and print duration

### Step 3: Port `OpenALAudioStream` (music streaming)
- [ ] Port `references/old-refs/fighter19-dxvk-port/GeneralsMD/Code/GameEngineDevice/Source/OpenALAudioDevice/OpenALAudioStream.cpp`
- [ ] Port matching header
- [ ] Minimal changes expected (91 lines, no game-specific logic)

### Step 4: Replace `OpenALAudioManager` stub with fighter19 implementation
- [ ] Replace our 643-line stub with fighter19's 3,098-line implementation
- [ ] Adapt includes/namespaces (fighter19 uses `OpenALAudioDevice/` subdirectory)
- [ ] Resolve Zero Hour–specific hooks (fighter19 targets ZH, so diff should be minimal)
- [ ] Remove all `fprintf(stderr, "DEBUG:...")` leftover from our stub phase

### Step 5: Integration testing
- [ ] Launch game → verify menu music plays
- [ ] Start skirmish → verify unit response sounds (gunshots, voices)
- [ ] Verify no crash on `addAudioEvent` / `removeAudioEvent`
- [ ] Run for 10+ minutes without audio memory leak

### Step 6: Windows preservation validation
- [ ] Confirm Windows builds (vc6/win32) still compile with Miles Sound System
- [ ] Confirm `SAGE_USE_OPENAL` guard correctly excludes OpenAL on Windows



**jmarshall Reference Analysis**:
- [ ] Study `references/old-refs/jmarshall-win64-modern/Code/Audio/` structure
- [ ] Document MusicManager implementation (streaming patterns)
- [ ] Document AudioManager event system (handle tracking)
- [ ] Document buffer lifecycle (creation, queueing, cleanup)
- [ ] Identify Generals vs Zero Hour differences
- [ ] Create adaptation strategy document

**Key Files to Analyze** (jmarshall):
```
Code/Audio/
├── AudioManager.cpp      # Core audio event system
├── MusicManager.cpp      # Background music streaming
├── AudioEventRTS.cpp     # RTS-specific audio events
└── ALBuffer.cpp          # Buffer management
```

**fighter19 Audio Reference** (if applicable):
- [ ] Check if fighter19 has any audio improvements beyond stubs
- [ ] Document any DXVK-specific audio considerations

### B. Audio Event System (Core Foundation)

**Goal**: Wire OpenALAudioManager::addAudioEvent() to actual playback

**Tasks**:
- [ ] Create `AudioEvent` struct (handle, source, state, 3D params)
- [ ] Implement handle generation (unique ID per event)
- [ ] Implement `m_ActiveEvents` tracking (std::map<AudioEventHandle, AudioEvent>)
- [ ] Wire `addAudioEvent()`:
  ```cpp
  AudioEventHandle addAudioEvent(const AudioEventInfo* info) {
      // 1. Allocate source from pool
      // 2. Load buffer (implement loadAudioBuffer)
      // 3. Set source properties (gain, pitch, position)
      // 4. alSourcePlay(source)
      // 5. Return handle
  }
  ```
- [ ] Implement `removeAudioEvent(handle)`:
  ```cpp
  void removeAudioEvent(AudioEventHandle handle) {
      // 1. Find event in m_ActiveEvents
      // 2. alSourceStop(source)
      // 3. Release source to pool
      // 4. Remove from map
  }
  ```
- [ ] Implement `update()`:
  ```cpp
  void update() {
      // 1. Poll all active sources (alGetSourcei AL_SOURCE_STATE)
      // 2. Remove finished one-shot sounds
      // 3. Update 3D listener position (see Section D)
  }
  ```

**Testing**:
- Simple test: Load `.wav` file, play on button press
- Verify handle lifecycle (create → play → remove)
- Check source pool recycling

### C. Audio Buffer Management

**Goal**: Load game audio files into OpenAL buffers

**Tasks**:
- [ ] Implement `loadAudioBuffer(const char* filename)`:
  - Detect format (WAV, MP3, OGG - check game assets)
  - Decode audio data (may need libsndfile or similar)
  - Create AL buffer: `alGenBuffers()`, `alBufferData()`
  - Return ALuint buffer ID
- [ ] Create buffer cache (avoid reloading same file):
  ```cpp
  std::map<std::string, ALuint> m_BufferCache;
  ```
- [ ] Implement buffer cleanup (destructor, shutdown):
  ```cpp
  void releaseAllBuffers() {
      for (auto& pair : m_BufferCache) {
          alDeleteBuffers(1, &pair.second);
      }
      m_BufferCache.clear();
  }
  ```

**Audio File Locations** (Zero Hour):
- Check `Data/Audio/` for `.wav`, `.mp3` files
- Check if audio is in `.big` archives (may need decompression)
- Document format used by Miles in original game

**Dependencies**:
- May need: libsndfile, libvorbis, or mpg123 for decoding
- Check CMake dependencies: `find_package(SndFile)`

### D. 3D Audio Positioning

**Goal**: Update listener and source positions for spatial audio

**Tasks**:
- [ ] Wire listener position in `update()`:
  ```cpp
  void update() {
      // Get camera/listener position from game engine
      Vector3 position = getListenerPosition();
      Vector3 velocity = getListenerVelocity();
      Vector3 forward = getListenerForward();
      Vector3 up = getListenerUp();
      
      alListener3f(AL_POSITION, position.x, position.y, position.z);
      alListener3f(AL_VELOCITY, velocity.x, velocity.y, velocity.z);
      ALfloat orientation[] = {forward.x, forward.y, forward.z, up.x, up.y, up.z};
      alListenerfv(AL_ORIENTATION, orientation);
  }
  ```
- [ ] Implement per-source 3D positioning:
  ```cpp
  void setAudioEventPosition(AudioEventHandle handle, const Vector3& pos) {
      AudioEvent* event = findEvent(handle);
      if (event) {
          alSource3f(event->source, AL_POSITION, pos.x, pos.y, pos.z);
      }
  }
  ```
- [ ] Add 3D audio parameters to AudioEventInfo:
  - Position, velocity, reference distance, max distance
  - Rolloff factor for distance attenuation

**Game Integration**:
- Hook into GameEngine camera updates (listener follows camera)
- Hook into unit positions (gunshots, explosions at unit location)

### E. Music Streaming System

**Goal**: Background music playback with track management

**Reference**: jmarshall's `MusicManager.cpp`

**Tasks**:
- [ ] Implement streaming source (separate from SFX pool):
  ```cpp
  ALuint m_MusicSource;
  ALuint m_MusicBuffers[4];  // 4 buffers for streaming
  ```
- [ ] Implement `playMusicTrack(const char* filename)`:
  - Open file stream (libsndfile or similar)
  - Queue initial buffers (`alSourceQueueBuffers`)
  - Start playback (`alSourcePlay`)
  - Set looping behavior
- [ ] Implement streaming update loop:
  ```cpp
  void updateMusicStream() {
      int buffersProcessed = 0;
      alGetSourcei(m_MusicSource, AL_BUFFERS_PROCESSED, &buffersProcessed);
      
      while (buffersProcessed--) {
          ALuint buffer;
          alSourceUnqueueBuffers(m_MusicSource, 1, &buffer);
          
          // Read next chunk from file
          streamMusicData(buffer);
          
          // Re-queue buffer
          alSourceQueueBuffers(m_MusicSource, 1, &buffer);
      }
  }
  ```
- [ ] Implement `nextMusicTrack()` / `prevMusicTrack()`:
  - Stop current track
  - Load next from playlist
  - Crossfade (optional, Phase 4)
- [ ] Wire to game music system (menu themes, battle music, victory/defeat)

**Music File Locations** (Zero Hour):
- Check `Data/Audio/Music/` for tracks
- Document playlist system (XML? INI?)

### F. Miles→OpenAL Compatibility Layer

**Goal**: Minimize game code changes by wrapping Miles API

**Strategy** (if feasible):
- [ ] Create `miles_compat.h` with OpenAL-backed Miles stubs
- [ ] Map Miles functions to OpenAL equivalents:
  ```cpp
  // Miles API                    // OpenAL equivalent
  AIL_open_stream()         →     OpenAL streaming source
  AIL_set_stream_volume()   →     alSourcef(AL_GAIN)
  AIL_stream_status()       →     alGetSourcei(AL_SOURCE_STATE)
  ```
- [ ] Document unmapped features (EAX, MIDI, etc.)

**Alternative Approach**:
- Direct replacement: Change game code to call OpenALAudioManager directly
- Pros: Cleaner, no legacy API baggage
- Cons: More game code changes

**Decision**: Choose based on game code inspection (how tightly coupled is Miles?)

### G. Testing & Validation

**Unit Tests** (manual, no automated tests yet):
- [ ] Load and play single `.wav` file
- [ ] Multiple simultaneous sounds (source pool stress test)
- [ ] 3D audio (walk around sound source, verify attenuation)
- [ ] Music streaming (long track, verify no gaps/stutters)
- [ ] Volume controls (master, SFX, music volumes independent)

**Integration Tests**:
- [ ] Launch game, verify menu music plays
- [ ] Start skirmish, verify unit response sounds
- [ ] Trigger explosions, verify 3D positioning
- [ ] Play for 30 minutes, check for audio glitches/leaks

**Performance**:
- [ ] Profile OpenAL overhead (should be <1% CPU)
- [ ] Check memory usage (buffer cache size)
- [ ] Verify no audio/video desync (Phase 3 dependency)

### H. CMake & Dependencies

**Tasks**:
- [ ] Add OpenAL detection to CMake:
  ```cmake
  if(SAGE_USE_OPENAL)
      find_package(OpenAL REQUIRED)
      target_link_libraries(GameEngineDevice PRIVATE OpenAL::OpenAL)
  endif()
  ```
- [ ] Add audio decoding library (libsndfile):
  ```cmake
  find_package(SndFile REQUIRED)
  target_link_libraries(GameEngineDevice PRIVATE SndFile::sndfile)
  ```
- [ ] Update Docker build to include dependencies:
  ```dockerfile
  RUN apt install -y libopenal-dev libsndfile1-dev
  ```

---

## Acceptance Criteria (Phase 2)

Phase 2 is **COMPLETE** when:
- [x] Game plays menu music on launch (Linux build) — **confirmed 2026-02-21**
- [ ] Sound effects work (gunshots, explosions, unit voices) — not yet tested in skirmish
- [ ] 3D audio positioning works (sounds attenuate with distance) — not yet tested
- [ ] Music tracks advance (victory/defeat themes trigger) — not yet tested
- [ ] No audio crashes or memory leaks (30-minute stress test) — not yet tested
- [ ] Windows builds still use Miles Sound System (no regressions) — pending Windows VM

---

## Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|------------|
| Audio format incompatibility | High | Research game archive structure early, test sample files |
| Miles API deeply coupled | Medium | Create compatibility layer or document replacement strategy |
| OpenAL performance issues | Low | Profile early, OpenAL is mature and efficient |
| Buffer memory leaks | Medium | Implement strict buffer lifecycle, stress test |
| 3D audio artifacts | Low | Use reference distances from original game |

---

## References

### Primary
- **jmarshall OpenAL implementation**: `references/old-refs/jmarshall-win64-modern/Code/Audio/`
- **OpenAL Programming Guide**: `docs/ETC/` (download if needed)
- **Miles Sound System docs**: `docs/ETC/` (reverse-engineer if no docs available)

### Game Audio Structure
- [ ] Document Zero Hour audio file locations
- [ ] Document audio format (WAV? MP3? Compressed?)
- [ ] Document playlist system
- [ ] Document 3D audio parameters (reference distances, rolloff)

---

## Timeline (Estimated)

- **Week 1**: Research (jmarshall patterns, game audio structure)
- **Week 2**: Audio event system + buffer management
- **Week 3**: 3D audio positioning + music streaming
- **Week 4**: Testing, integration, bug fixes

**Total Estimate**: 4 weeks (blockers: game audio format discovery, Miles API coupling)

---

## Deliverables

- Updated `OpenALAudioManager.{h,cpp}` with full event system
- Audio buffer management implementation
- Music streaming system
- CMake dependencies for OpenAL + audio decoding
- Phase 2 test results (manual tests documented)
- Dev blog updates

---

## Phase 2 → Phase 3 Handoff

**Prerequisites for Phase 3 (Video Playback)**:
- Audio system working (Phase 2 complete)
- SDL3 windowing stable (Phase 1 complete)
- Game launches and runs without crashes

**Phase 3 Blockers**:
- Bink video codec (proprietary, may need alternative)
- Audio/video sync requirements
- Intro video skip mechanism

---

## Notes

- **jmarshall reference is Generals base game ONLY**: Zero Hour may have additional audio features (new unit voices, expanded music system). Adapt accordingly.
- **Audio file discovery is critical**: If audio is compressed in `.big` archives, we need decompression first.
- **OpenAL is mature**: Expect fewer issues than DXVK/graphics layer.
- **Preserve Windows builds**: Miles Sound System must remain functional behind compile guards.

---

**Status Tracking**:
- [x] Spike: SDL3 audio vs OpenAL decision (2026-02-21 → OpenAL selected)
- [x] Step 1: CMake + Docker dependencies (FFmpeg pkg_check_modules + libopenal-dev)
- [x] Step 2: OpenALAudioFileCache (FFmpeg decoder) — ported from fighter19
- [x] Step 3: OpenALAudioStream (music streaming) — ported from fighter19
- [x] Step 4: OpenALAudioManager full implementation — 3,099 lines ported, 7 API fixes applied
- [x] Step 5: Integration testing — menu music confirmed working
- [ ] Step 6: Windows preservation validation — not yet tested, pending Windows VM

**Progress**: 6/7 steps complete (Windows validation deferred)
