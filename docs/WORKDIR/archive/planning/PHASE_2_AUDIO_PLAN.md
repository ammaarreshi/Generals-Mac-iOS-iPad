# Phase 2: Linux Audio - OpenAL Integration

**Status**: 📋 PLANNING  
**Priority**: 🔴 HIGH (after Phase 1 graphics)  
**Est. Duration**: 2-3 sessions  
**Reference**: `references/old-refs/jmarshall-win64-modern/Code/Audio/` (adapt for Zero Hour)

---

## Objective

Port Miles Sound System (Windows-only audio SDK) to OpenAL on Linux while maintaining Windows compatibility. Current state: **SILENT** (audio stubs in place).

### Goals
- ✅ Make Linux build produce audio (sound effects, music, voices)
- ✅ Maintain Windows Miles Sound System for VC6/Win32 builds
- ✅ Match original audio quality and timing
- ✅ No gameplay impact (determinism preserved)

---

## What We Have Now

### Current Audio State
**File**: `GeneralsMD/Code/GameEngineDevice/Source/`
- Windows: `DX8GraphicsDevice` + Miles Sound System SDK
- Linux: Stubs (no-op implementations)

### Build System
**File**: `cmake/miles.cmake`
```cmake
# Conditions: /usr/include/mss.h (Windows Miles, must-have)
# On Linux: Falls back gracefully (no audio)
```

### Player Audio Flow
```
Game Logic (GameEngine)
  ↓
Audio Manager (Core/GameEngine/Source/Audio/*)
  ↓
Audio Device (Windows: MilesAudioDevice, Linux: StubAudioDevice)
  ↓
Platform (Windows: Miles SDK, Linux: OpenAL)
```

---

## Reference Architecture - jmarshall's OpenAL Port

### Key Discovery Points (Generals-only, adapt for Zero Hour!)

**1. File**: `references/old-refs/jmarshall-win64-modern/Code/Audio/AudioManager.cpp`
- Audio system initialization flow
- How Miles functions map to OpenAL calls
- Format conversion (Miles → PCM)

**2. File**: `references/old-refs/jmarshall-win64-modern/Code/Audio/MusicManager.cpp`
- Background music management
- Audio format detection
- Cross-platform stream handling

**3. File**: `references/old-refs/jmarshall-win64-modern/Code/Audio/OpenALAudioDevice.cpp`
- Complete OpenAL device setup
- Buffer/source management
- Distance attenuation (3D audio)

**4. INI Parsing**: `jmarshall/Code/Lib/INI/INISection.cpp`
- Handles audio configuration files
- May be useful for custom audio settings

### Key Patterns to Adopt
```cpp
// Pattern 1: Miles → OpenAL function mapping
AnyAudio_3D_Create_Stream() → alGenSources() + alBufferData()
AnyAudio_Set_Volume() → alSourcef(..., AL_GAIN, ...)
AnyAudio_Set_Position() → alSource3f(..., AL_POSITION, ...)

// Pattern 2: Platform abstraction
#ifdef BUILD_WITH_MILES
    MilesAudioDevice device;  // Windows
#else
    OpenALAudioDevice device;   // Linux
#endif

// Pattern 3: Safe fallback
if (!OpenAL_Init()) {
    fprintf(stderr, "WARNING: OpenAL failed, using silent audio");
    use_silent_audio = true;  // Continue without crash
}
```

---

## Implementation Roadmap

### Phase 2.1: Audio System Abstraction (Session TBD)
**Goal**: Create platform-agnostic audio layer

**Deliverables**:
- [ ] New file: `Core/GameEngineDevice/Include/AudioDevice.h` (abstract interface)
- [ ] Implement: `AudioDevice::PlaySound(filename, volume, 3D_position)`
- [ ] Implement: `AudioDevice::PlayMusic(filename, loop)`
- [ ] Implement: `AudioDevice::SetVolume(master_vol)`
- [ ] Document: Miles → OpenAL method mapping table

**Key Methods** (study in jmarshall ref):
```cpp
class AudioDevice
{
public:
    virtual void Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void Update() = 0;
    
    virtual void PlaySound(const char* filename, float volume, bool loop, Coord3D* pos = nullptr) = 0;
    virtual void PlayMusic(const char* filename, bool loop) = 0;
    virtual void StopSound(SoundHandle handle) = 0;
    virtual void SetVolume(float master, float music, float sfx) = 0;
};
```

**Windows Path** (existing):
- Delegates to Miles SDK (existing code unchanged)

**Linux Path** (new):
- Delegates to OpenAL device

---

### Phase 2.2: OpenAL Device Implementation (Session TBD+1)
**Goal**: Implement complete OpenAL backend

**Deliverables**:
- [ ] New file: `Core/GameEngineDevice/Source/OpenALAudioDevice.cpp`
- [ ] New file: `Core/GameEngineDevice/Source/OpenALAudioDevice.h`
- [ ] CMake: Add OpenAL dependency detection
- [ ] CMake: Create `cmake/openal.cmake` preset loader

**Build Dependencies** (add to vcpkg.json):
```json
{
    "name": "openal-soft",
    "version>=" : "1.23.0"
}
```

**Key Implementation** (from jmarshall reference):
```cpp
class OpenALAudioDevice : public AudioDevice
{
private:
    ALCdevice* device;
    ALCcontext* context;
    std::map<SoundHandle, ALuint> sources;  // OpenAL source IDs
    
public:
    void Initialize()
    {
        device = alcOpenDevice(nullptr);  // Default audio device
        context = alcCreateContext(device, nullptr);
        alcMakeContextCurrent(context);
    }
    
    void PlaySound(const char* filename, float volume, bool loop, Coord3D* pos)
    {
        ALuint source = 0;
        alGenSources(1, &source);
        
        // Load audio file (WAV, OGG, etc.)
        ALuint buffer = LoadAudioFile(filename);
        alSourcei(source, AL_BUFFER, buffer);
        alSourcef(source, AL_GAIN, volume);
        
        // 3D positioning if provided
        if (pos) {
            alSource3f(source, AL_POSITION, pos->x, pos->y, pos->z);
        }
        
        alSourcePlay(source);
        sources[handle] = source;
    }
};
```

**Acceptance Criteria**:
- [ ] OpenAL initializes without crash
- [ ] Sound files (WAV/OGG) load successfully
- [ ] Volume control works (0.0 → 1.0 range)
- [ ] 3D positional audio works (if game uses it)
- [ ] Music streams loop properly

---

### Phase 2.3: Miles → OpenAL Wrapper (Session TBD+2)
**Goal**: Create compatibility layer so existing game code works unchanged

**Deliverables**:
- [ ] New file: `Core/GameEngineDevice/Include/MilesCompat.h`
- [ ] Document: Complete Miles → OpenAL function mapping
- [ ] Blanket wrapper: Redirect Miles calls to OpenAL device

**Key Mappings**:
| Miles API | OpenAL Equivalent | Notes |
|-----------|------------------|-------|
| `AnyAudio_Init()` | `alut_Init()` or manual OpenAL init | Initialize audio system |
| `AnyAudio_Close()` | `alut_Shutdown()` | Shutdown audio system |
| `AnyAudio_3D_Create_Stream()` | `alGenSources() + alBufferData()` | Create sound source |
| `AnyAudio_Play_Sound()` | `alSourcePlay()` | Play sound |
| `AnyAudio_Stop_Sound()` | `alSourceStop()` | Stop sound |
| `AnyAudio_Set_Volume()` | `alSourcef(source, AL_GAIN, vol)` | Set volume |
| `AnyAudio_Set_Position()` | `alSource3f(source, AL_POSITION, x, y, z)` | Set 3D position |

---

### Phase 2.4: Testing & Validation (Session TBD+3)
**Goal**: Verify audio works correctly

**Test Scenarios**:
- [ ] **Smoke Test**: Launch GeneralsXZH in Docker, hear menu music
- [ ] **Gameplay Test**: Skirmish mode 1min → verify unit sounds, explosions
- [ ] **Music Test**: Campaign mission → verify background music (loop doesn't stutter)
- [ ] **3D Audio Test** (if used): Unit position changes → verify pan/distance attenuation
- [ ] **Stress Test**: 10+ units shooting → verify no crashes, audio queues properly
- [ ] **Windows VC6 Test**: Build Windows binary → verify Miles SDK still works (no regression)

**Success Criteria**:
- ✅ Audio quality comparable to original (no pops, clicks, distortion)
- ✅ Music doesn't cut out abruptly
- ✅ Sound effects trigger at correct moments
- ✅ Volume levels reasonable (not too quiet/loud)
- ✅ No undefined references or linker errors
- ✅ Determinism preserved (gameplay identical to audio-off build)

---

## Risk Assessment

### High Risk 🔴
- **Audio Format Support**: Miles handles proprietary formats; OpenAL expects standard (WAV, OGG, FLAC)
  - **Mitigation**: Check what formats game actually uses; convert if needed
  
- **Synchronization**: Miles events may be tightly coupled to gameplay logic
  - **Mitigation**: Study Audio Manager flow in jmarshall reference first

### Medium Risk 🟡
- **Performance**: OpenAL buffers/sources may behave differently than Miles
  - **Mitigation**: Profile and optimize buffer management

- **Compatibility**: Some Linux systems may not have working audio drivers
  - **Mitigation**: Graceful fallback (silent audio, continue game)

### Low Risk 🟢
- **Windows Regression**: Miles SDK path isolated behind `#ifdef BUILD_WITH_MILES`
  - **Mitigation**: Test Windows build after each checkpoint

---

## Critical Questions (Answer Before Starting)

1. **Which audio formats does game use?**
   - Grep for file extensions in audio code
   - Check: `.wav`, `.ogg`, `.mp3`, `.aud` (Miles format?)

2. **Is 3D Surround audio used?**
   - Search for: `alut_3D`, `3D_Create_Stream`, `AL_POSITION`
   - If used: Need full OpenAL 3D model

3. **Audio callback pattern?**
   - Does game push audio data or pull?
   - Miles: Push (streaming)? Or pull (immediate)?

4. **Music vs SFX separation?**
   - Separate manager instances?
   - Different initialization flags?

---

## Session Checkpoint Plan

| Session | Focus | Deliverable |
|---------|-------|-------------|
| Next | Phase 2.1 Analysis | AudioDevice interface + mapping table |
| Next+1 | Phase 2.2 OpenAL | Full OpenALAudioDevice implementation |
| Next+2 | Phase 2.3 Wrapper | Miles compatibility layer |
| Next+3 | Phase 2.4 Testing | Audio validation + Windows regression test |

---

## When to Start Phase 2

**Prerequisites**:
- [ ] Phase 1 smoke test passed (binary launches)
- [ ] Windows VC6 build validates (no regressions from Phase 1)
- [ ] jmarshall reference studied and understood
- [ ] Audio format requirements documented

**Decision Point**: After runtime smoke test of Phase 1 binary → Phase 2 kickoff

---

## Documentation References

See also:
- [SESSION_38_BUILD_SUCCESS.md](../WORKDIR/support/SESSION_38_BUILD_SUCCESS.md) - Phase 1 technical details
- [SIDEQUEST_CD_REMOVAL.md](../WORKDIR/reports/SIDEQUEST_CD_REMOVAL.md) - Portable gameplay context
- `references/old-refs/jmarshall-win64-modern/Code/Audio/` - Detailed OpenAL implementation
- `docs/DEV_BLOG/2026-02-DIARY.md` - Session history

