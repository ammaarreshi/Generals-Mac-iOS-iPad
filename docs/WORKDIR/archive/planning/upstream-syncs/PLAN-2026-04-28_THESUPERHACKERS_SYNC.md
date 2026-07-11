# PLAN-2026-04-28 THESUPERHACKERS UPSTREAM SYNC

## Objective

Perform an upstream sync from `thesuperhackers/main` into branch `thesuperhackers-sync-04-28-2026` while preserving GeneralsX cross-platform architecture (SDL3 + DXVK + OpenAL + FFmpeg pathing) and importing useful upstream fixes.

## Merge State Snapshot

- Merge base action: `git merge thesuperhackers/main`
- Current explicit conflicts:
  - `Core/GameEngine/Source/Common/FrameRateLimit.cpp`
  - `GeneralsMD/Code/Main/CMakeLists.txt`
  - `README.md`
- High-risk auto-merged surface includes:
  - Core profiling/tracing infrastructure (`Core/Libraries/Source/profile/*`, `cmake/tracy.cmake`, `CMakePresets.json`)
  - Rendering/device wiring (`Core/GameEngineDevice/*`, `Generals*/Code/GameEngineDevice/*`)
  - Runtime launch paths (`Generals*/Code/Main/WinMain.cpp`, `GeneralsMD/Code/Main/*`)
  - Gameplay/system changes in `Core/GameEngine`, `Generals`, `GeneralsMD`
  - INI and load-path related files (`INICommandButton.cpp`, shared game client/system files)

## Reference Inputs Consulted

- Local instructions and project strategy documents under `.github/instructions/`
- Existing project lessons in `docs/WORKDIR/lessons/2026-04-LESSONS.md`
- Local reference snapshot under `references/old-refs/thesuperhackers-main/`
- Direct upstream file state from `thesuperhackers/main` for each conflicted file

## Conflict Resolution Plan

### 1) `Core/GameEngine/Source/Common/FrameRateLimit.cpp`

#### Conflict Summary
- Upstream added `PROFILER_SECTION` in `FrameRateLimit::wait`.
- GeneralsX added non-Windows timing/sleep implementation (`clock_gettime`, `nanosleep`) to keep the frame limiter functional on Linux/macOS.

#### Resolution Strategy
- Preserve GeneralsX cross-platform timing implementation for non-Windows.
- Keep upstream profiler hook (`PROFILER_SECTION`) at the top of `wait` so profiling remains available.
- Confirm macro availability via merged profile headers/CMake and verify no compile break.

#### Risk and Mitigation
- Risk: profiler macro undefined in some presets.
  - Mitigation: run configure + build validation on macOS and Linux flows.
- Risk: frame pacing unit mismatch causing jitter or speed-up.
  - Mitigation: smoke-run both `GeneralsXZH` and `GeneralsX` main loop entry/exit.

### 2) `GeneralsMD/Code/Main/CMakeLists.txt`

#### Conflict Summary
- Upstream switched `core_profile` to `core_profile_legacy` and kept full Windows-oriented link list in common section.
- GeneralsX split Windows-only libs (`d3d8`, `dinput8`, etc.) behind `if(WIN32)` and kept cross-platform common link set.

#### Resolution Strategy
- Preserve GeneralsX cross-platform link partitioning (`if(WIN32)` for Win32-only libs).
- Adopt upstream `core_profile_legacy` where profile library naming changed, to remain compatible with merged profiling infrastructure.
- Keep SDL3 non-Windows sources (`SDL3Main.cpp`, `LinuxStubs.cpp`) and non-Windows output naming (`GeneralsXZH`) intact.

#### Risk and Mitigation
- Risk: wrong profile target name can break link on one platform.
  - Mitigation: validate macOS and Linux configure/build.
- Risk: accidental Windows-only dependency leaking into non-Windows link.
  - Mitigation: inspect final target link block and run platform builds.

### 3) `README.md`

#### Conflict Summary
- Conflict is in sponsor section, where upstream inserted profiling/contributing section while GeneralsX has project-specific support messaging.

#### Resolution Strategy
- Keep GeneralsX README identity and support section.
- Re-integrate useful upstream profiling note in a neutral way only if it does not regress GeneralsX messaging.
- Ensure docs links remain valid for this repo structure.

#### Risk and Mitigation
- Risk: mixed branding or confusing instructions between repositories.
  - Mitigation: retain GeneralsX-centric wording and clearly scope profiling note.

## Execution Order

1. Resolve explicit conflicts in the three files above.
2. Stage resolved files and verify no conflict markers remain repository-wide.
3. Perform targeted review of high-risk auto-merged files:
   - `cmake/config-build.cmake`, `cmake/tracy.cmake`, `Core/Libraries/Source/profile/*`
   - `GeneralsMD/Code/Main/WinMain.cpp`, `Generals/Code/Main/WinMain.cpp`
   - `Core/GameEngineDevice/*` key changes
4. Validate configure/build and smoke run sequentially:
   - macOS configure/build first
   - Linux configure/build second
   - Runtime smoke for both products on validated runnable platform
5. Remove generated artifacts/caches from working tree.
6. Commit merge with detailed message.
7. Push branch `thesuperhackers-sync-04-28-2026` to `origin`.

## Special Constraints

- No blanket keep-ours/keep-theirs strategy.
- Preserve cross-platform architecture and platform isolation.
- Avoid hacks/stubs and do root-cause resolution only.
- Keep determinism-sensitive behavior untouched unless explicitly required.