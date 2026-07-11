# Phase 1: Linux Graphics - DXVK Integration

**Status**: ⚠️ SUPERSEDED by PHASE01_IMPLEMENTATION_PLAN.md
**Created**: 2026-02-07 (Session 5)
**Superseded**: 2026-02-08 (Session 6)

> **NOTE**: This document was replaced by the more detailed [PHASE01_IMPLEMENTATION_PLAN.md](./PHASE01_IMPLEMENTATION_PLAN.md).  
> Kept for historical reference only. See the implementation plan for current progress.

---

## Objective

Port fighter19's DXVK graphics layer to GeneralsXZH, enabling native Linux rendering via Vulkan translation. This phase focuses ONLY on graphics - audio will be silent/stubbed.

## Scope

### In Scope ✅
- DXVK DirectX8→Vulkan wrapper integration
- SDL3 windowing/input layer (replace Win32)
- MinGW build preset for Linux (64-bit x86_64)
- Platform-specific graphics in `Core/GameEngineDevice/`
- Compile-time platform selection (`#ifdef BUILD_WITH_DXVK`)

### Out of Scope ❌
- Audio (silent/stubbed - Phase 2)
- Video playback (defer to Phase 3)
- Multiplayer/network (future)
- Mods (test after Phase 1)

## Implementation Plan

### Step 1: Prepare Build System
- [ ] Add `linux64-deploy` preset to CMakePresets.json
- [ ] Create toolchain file if needed
- [ ] Configure DXVK/SDL3 dependencies
- [ ] Test Docker build workflow
- [ ] Confirm Windows builds still work

### Step 2: DXVK Wrapper Integration
- [ ] Create DXVK adapter headers in `Core/GameEngineDevice/Include/`
- [ ] Implement DXVK device wrapper
- [ ] Add DX8→DXVK function mapping
- [ ] Use `#ifdef BUILD_WITH_DXVK` guards
- [ ] Keep Windows DX8 path intact

### Step 3: SDL3 Windowing Layer
- [ ] Replace Win32 window creation with SDL3
- [ ] Implement SDL3 event loop
- [ ] Map SDL3 input to game input system
- [ ] Add `#ifdef USE_SDL3` guards
- [ ] Keep Win32 windowing intact

### Step 4: Platform File Layer
- [ ] Implement POSIX file APIs where needed
- [ ] Add case-insensitive file lookup (Linux)
- [ ] Handle path separator differences
- [ ] Add `#ifdef __linux__` guards
- [ ] Keep Win32 file APIs intact

### Step 5: Stub Audio for Phase 1
- [ ] Create no-op audio backend
- [ ] Ensure game doesn't crash without audio
- [ ] Log audio calls for Phase 2 reference
- [ ] Document Miles API usage

### Step 6: Build and Test
- [ ] Build linux64-deploy in Docker
- [ ] Verify native ELF binary created
- [ ] Test in Linux VM or container
- [ ] Validate Windows builds unchanged
- [ ] Test basic gameplay (skirmish map)

## Acceptance Criteria

Phase 1 is **COMPLETE** when:
- [ ] GeneralsXZH builds with `linux64-deploy` preset
- [ ] Native Linux ELF binary launches
- [ ] Main menu renders correctly
- [ ] Can load and play skirmish map
- [ ] Graphics are functional (no audio yet)
- [ ] No crashes during 10-minute gameplay test
- [ ] Windows VC6 build compiles and runs
- [ ] Windows Win32 build compiles and runs
- [ ] Platform code properly isolated

## Files to Modify

### Build System
- `CMakePresets.json` - Add linux64-deploy
- `cmake/config-build.cmake` - Add BUILD_WITH_DXVK option
- `cmake/mingw.cmake` - Linux toolchain config

### Graphics Device Layer
- `Core/GameEngineDevice/Include/w3dgraphicsdevice.h`
- `Core/GameEngineDevice/Source/w3dgraphicsdevice.cpp`
- `Core/GameEngineDevice/Include/dxvk_adapter.h` (NEW)
- `Core/GameEngineDevice/Source/dxvk_adapter.cpp` (NEW)

### Windowing Layer
- `Core/Libraries/Include/rts/window.h`
- `Core/Libraries/Source/Platform/sdl3_window.cpp` (NEW)
- `GeneralsMD/Code/Main/WinMain.cpp` (minimal changes)

### Platform Layer
- `Core/Libraries/Source/Platform/posix_filesystem.cpp` (NEW)
- `Core/Libraries/Include/rts/filesystem.h`

### Audio Stubs (Phase 1 only)
- `Core/GameEngine/Audio/stub_audio.cpp` (NEW)

## Testing Checklist

### Build Tests
- [ ] Docker build succeeds (linux64-deploy)
- [ ] Windows VC6 build succeeds
- [ ] Windows Win32 build succeeds
- [ ] No warnings introduced

### Functional Tests
- [ ] Linux: Launch game
- [ ] Linux: Main menu visible
- [ ] Linux: Load skirmish map
- [ ] Linux: Play for 10 minutes without crash
- [ ] Windows: All of the above still work

### Regression Tests
- [ ] Windows: Replay compatibility (VC6)
- [ ] Windows: No visual regressions
- [ ] Windows: Same functionality as before

## Reference Materials

- fighter19 DXVK integration: `references/old-refs/fighter19-dxvk-port/`
- Phase 0 analysis: `docs/WORKDIR/support/phase0-fighter19-analysis.md`
- Platform abstraction design: `docs/WORKDIR/support/phase0-platform-abstraction.md`

## Known Risks

1. **DXVK compatibility**: Some DX8 features may not translate perfectly
   - **Mitigation**: Test early, document issues
2. **SDL3 input lag**: SDL event loop may introduce latency
   - **Mitigation**: Compare with Win32 responsiveness
3. **File path issues**: Case sensitivity on Linux
   - **Mitigation**: Implement case-insensitive lookup

## Notes

- Phase 1 is graphics ONLY - no audio
- Game will be silent but playable
- Focus on getting it working, optimize later
- Keep Windows builds intact - NON-NEGOTIABLE

## Next Phase

After Phase 1 complete:
- Phase 2: Linux Audio - OpenAL Integration
- Add working audio to Linux build
- Adapt jmarshall's OpenAL for Zero Hour

## Progress Log

See `docs/DEV_BLOG/2026-02-DIARY.md` for daily updates.
