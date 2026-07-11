# Phase 0: jmarshall OpenAL Implementation Analysis

**Created**: 2026-02-07
**Status**: In Progress
**Reference**: `references/old-refs/jmarshall-win64-modern/`

## Objective

Understand jmarshall's OpenAL audio implementation and confirm the hypothesis:
- **Abstraction layer created, core code untouched**
- Miles Sound System API translated to OpenAL
- Audio pipeline preserved
- Windows Miles path still available

## Key Hypothesis to Confirm

> jmarshall did NOT modify core audio code. Instead, created compatibility layer to translate Miles API calls to OpenAL.

If true, we can adapt this pattern for Zero Hour.

## Research Questions

1. **Where is the OpenAL abstraction layer?**
   - Expected: New audio backend in `Core/GameEngine/Audio/` or similar
   - Check for Miles→OpenAL API mapping
   - Check for compile-time switches

2. **How are Miles calls intercepted?**
   - Header-level redirection?
   - Link-time substitution?
   - Runtime polymorphism?

3. **What audio formats are handled?**
   - WAV, MP3, OGG support
   - Streaming vs buffered playback
   - 3D positional audio

4. **What are the Zero Hour differences?**
   - **CRITICAL**: jmarshall only ported Generals base game
   - Zero Hour may have additional audio features
   - Need to identify gaps and adaptation strategy

## Investigation Plan

### Step 1: Survey OpenAL Implementation
- [ ] Use `cognitionai/deepwiki` to query jmarshall repo
- [ ] Ask: "Where is OpenAL implementation located?"
- [ ] Ask: "How does Miles→OpenAL API translation work?"
- [ ] Ask: "What files were modified for audio system?"

### Step 2: Analyze Audio Abstraction
- [ ] Document Miles API surface used by game
- [ ] Document OpenAL equivalents
- [ ] Identify format conversion requirements

### Step 3: Analyze Audio Pipeline
- [ ] Music playback system
- [ ] Sound effects system
- [ ] Voice/dialogue system
- [ ] 3D audio positioning

### Step 4: Identify Zero Hour Gaps
- [ ] List Generals-only assumptions
- [ ] Identify Zero Hour expansion features
- [ ] Document required adaptations

## Findings

### OpenAL Abstraction Architecture ✅ CONFIRMED

**Abstraction Pattern**: Inheritance-based audio manager
- **Base Class**: `AudioManager` (abstract interface)
- **Implementations**: `OpenALAudioManager`, `MilesAudioManager` (concrete)
- **Location**: `Code/GameEngineDevice/OpenALAudioDevice/`
- **Core Isolation**: Game logic calls `AudioManager` interface, implementation swappable

**Key Components**:
- `OpenALAudioManager.h/cpp` - OpenAL implementation of AudioManager
  - `init()` - Initializes OpenAL device/context
  - `playAudioEvent()` - Plays audio via OpenAL
  - `openDevice()` - Sets up OpenAL context
- `OpenALDriver.cpp` - Low-level OpenAL operations
  - Play 2D/3D samples (`alGenSources`, `alSourcePlay`)
  - Device management (`alcOpenDevice`)
  - Listener updates (`alListener3f`)
- `OpenALAudioLoader.h` - Audio file loading/decoding (WAV, MP3)

**Design Philosophy**: NOT a "Miles API wrapper" - instead, parallel implementations of common interface

### Miles→OpenAL API Mapping

**Clarification**: NO direct API translation layer exists.
Instead: Both backends implement same `AudioManager` interface.

**Equivalent Operations**:
- `MilesAudioManager::openDevice()` → `OpenALAudioManager::openDevice()`
- `MilesAudioManager::playAudioEvent()` → `OpenALAudioManager::playAudioEvent()`
- Miles init → OpenAL `alcOpenDevice()` + `alcCreateContext()`
- Miles sample play → OpenAL `alGenSources()` + `alSourcePlay()`

**Key Files**:
- `GameEngineDevice/Include/OpenALAudioDevice/OpenALAudioManager.h`
- `GameEngineDevice/Source/OpenALAudioDevice/OpenALAudioManager.cpp`
- `GameEngineDevice/Source/OpenALAudioDevice/OpenALDriver.cpp`
- `GameEngineDevice/Include/OpenALAudioDevice/OpenALAudioLoader.h`

### Audio Format Support
- **WAV**: Direct support via `OpenALAudioLoader`
- **MP3**: Decoding support via `OpenALAudioLoader`
- **OGG**: To be investigated
- **3D Audio**: Supported (listener position updates in `OpenALDriver.cpp`)

### Changed Files List
**Audio System**:
- `GameEngineDevice/OpenALAudioDevice/*` - Complete OpenAL backend

**Build System**:
- `CMakeLists.txt` - Conditional compilation (Miles vs OpenAL)

**Zero Hour Adaptation Notes**:
⚠️ **CRITICAL**: jmarshall only ported **Generals base game**.
- Must verify Zero Hour expansion audio features
- May need additional audio event types
- May need expansion-specific music handling
- Voice lines and faction audio need testing

## Suspected Abstraction Pattern

Expected pattern (to confirm):
```cpp
// Core/GameEngine/Audio/Include/milesaudio.h
#ifdef USE_OPENAL
    #include "openal_adapter.h"
    // Translate Miles calls to OpenAL
#else
    #include <mss.h> // Real Miles headers
#endif
```

## Zero Hour Specific Audio Features

Known expansion features to verify:
- Additional faction music
- Expansion-specific sound effects
- New voice lines
- Enhanced audio cues

Need to confirm: Does jmarshall's implementation handle these?
Answer: **NO** - Generals only. Must adapt for Zero Hour.

## Notes

- jmarshall's audio WORKS on Windows (modern build)
- Only covers Generals base game
- Zero Hour adaptation is OUR responsibility
- This is reference for patterns, NOT copy-paste

## Next Steps

1. Query jmarshall repo via DeepWiki
2. Confirm abstraction layer approach
3. Document Miles→OpenAL mapping
4. List Zero Hour-specific audio requirements
5. Create adaptation strategy document
