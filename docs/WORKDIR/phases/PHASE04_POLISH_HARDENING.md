# PHASE04: Polish & Hardening (Linux)

**Status**: 📋 Not Started (Blocked by Phase 1-3)
**Type**: Bug fixing, optimization, QA
**Created**: 2026-02-08
**Updated**: 2026-02-08 (Scoped to Linux, macOS moved to Phase 5)
**Prerequisites**: Phase 1, 2, and 3 (or 3 deferred) complete

## Goal

Address Linux-specific bugs, performance issues, and compatibility edge cases. Polish the Linux port to production-ready state. Prepare codebase for Phase 5 (macOS port).

## Progress Snapshot
⏳ Blocked by Phase 1-3  
🎯 Focus on stability, performance, and user experience  
🐛 Bug bash + optimization pass

---

## Context

**Current State (End of Phase 3)**:
- ✅ Graphics working (DXVK + SDL3)
- ✅ Audio working (OpenAL)
- ⚠️ Videos working, stubbed, or deferred
- ⚠️ Unknown bugs from integration
- ⚠️ Performance not profiled
- ⚠️ Edge cases not tested

**Phase 4 Focus**:
Stabilize, optimize, and prepare for community release.

---

## Scope

### In Scope ✅
- Linux-specific bug fixes (crashes, hangs, rendering glitches)
- Performance profiling and optimization (Linux)
- Filesystem case sensitivity edge cases
- Input handling edge cases (SDL3 quirks)
- Memory leak detection and fixes
- Compatibility testing (multiple Linux distros, GPUs, drivers)
- User experience polish (error messages, graceful degradation)
- Documentation (installation, troubleshooting for Linux)
- Packaging (AppImage, Flatpak, or tarball for Linux)
- **Code organization for Phase 5**: Ensure SDL3/Vulkan/OpenAL code is portable to macOS

### Out of Scope ❌
- New features (audio effects, shaders, gameplay changes)
- Multiplayer/network (future phase)
- Mod compatibility (future phase)
- **macOS port** (moved to Phase 5, after Linux is stable)
- Mobile ports (out of scope)

---

## Implementation Plan

### A. Bug Bash (Testing & Issue Collection)

**Goal**: Discover and catalog all Linux-specific bugs

**Tasks**:
- [ ] **Smoke tests** (comprehensive):
  - Launch game (windowed, fullscreen, different resolutions)
  - Main menu navigation (all buttons, settings)
  - Single player skirmish (all factions, all maps)
  - Campaign mode (if videos working/stubbed gracefully)
  - Replay system (load `.rep` files, ensure compatibility)
  - Load saved games
  - Exit game gracefully (no crashes on shutdown)

- [ ] **Stress tests**:
  - 2-hour gameplay session (check for memory leaks, performance degradation)
  - Large battles (8-player FFA, max units, check FPS)
  - Rapid alt+tab (window focus handling)
  - Audio spam (100+ simultaneous explosions, check source pool limits)

- [ ] **Edge case tests**:
  - Case-sensitive filenames (Linux filesystem)
  - UTF-8 paths (special characters in home directory)
  - Missing assets (game handles gracefully?)
  - Corrupted save files
  - Invalid command-line arguments

- [ ] **Compatibility tests** (multiple environments):
  - Ubuntu 22.04, 26.04 (LTS targets)
  - Fedora 40+ (latest kernel/drivers)
  - Arch Linux (rolling release, bleeding edge)
  - Steam Deck (Proton comparison: native vs. Wine)
  - Multiple GPUs: NVIDIA (proprietary), AMD (Mesa), Intel (integrated)
  - Multiple Vulkan drivers: Mesa RADV, AMDVLK, NVIDIA proprietary

- [ ] **Create bug tracker** (GitHub Issues or local markdown):
  - Template: Title, Steps to Reproduce, Expected, Actual, Logs, Environment
  - Priority labels: P0 (crash), P1 (major), P2 (minor), P3 (polish)
  - Assign estimates and track fixes

**Deliverable**: `docs/WORKDIR/audit/phase4-bugs.md` (issue list)

### B. Performance Profiling & Optimization

**Goal**: Ensure Linux performance matches or exceeds Windows

**Profiling Tools**:
- [ ] **gprof** or **perf** (CPU profiling):
  ```bash
  perf record -g ./GeneralsXZH -win -noshellmap
  perf report
  ```
  - Identify hot paths (rendering, audio, game logic)
  - Optimize if >10% overhead vs. Windows

- [ ] **Valgrind** (memory profiling):
  ```bash
  valgrind --leak-check=full --track-origins=yes ./GeneralsXZH -win
  ```
  - Check for memory leaks (OpenAL buffers, SDL surfaces, DXVK resources)
  - Fix all "definitely lost" leaks

- [ ] **RenderDoc** (GPU profiling):
  - Capture frame, analyze draw calls
  - Compare DXVK overhead vs. native DX8 on Windows
  - Optimize if >20% overhead

- [ ] **FPS benchmarks** (compare Windows vs. Linux):
  - Same map, same battle scenario, measure average FPS
  - Target: <10% FPS difference
  - If worse: Profile and optimize DXVK settings, Vulkan driver

**Optimization Candidates**:
- [ ] Filesystem I/O (case-insensitive lookup caching)
- [ ] OpenAL buffer management (reduce allocations)
- [ ] DXVK state changes (batch render state changes if possible)
- [ ] SDL3 event polling (reduce polling frequency if needed)

**Deliverable**: `docs/WORKDIR/reports/phase4-performance.md` (benchmark results)

### C. Filesystem Case Sensitivity Fixes

**Goal**: Handle Linux case-sensitive filesystem gracefully

**Problem**:
- Windows: `Data/Audio/Music.mp3` == `data/audio/music.mp3`
- Linux: Case matters, file not found if case mismatch

**Solution** (already planned in Phase 1, now implement):
- [ ] Implement `CaseInsensitiveFileSystem` wrapper:
  ```cpp
  class CaseInsensitiveFileSystem {
      std::map<std::string, std::string> m_PathCache;  // lowercase → actual
      
      std::string findFile(const char* path) {
          std::string lower = toLowerCase(path);
          if (m_PathCache.count(lower)) {
              return m_PathCache[lower];
          }
          // Scan directory, build cache
          return scanAndCache(path);
      }
  };
  ```
- [ ] Hook into game file loading:
  - Wrap `fopen()` calls (or `LocalFileSystem::openFile()`)
  - Log mismatches: "Warning: Requested 'Data/Audio/Music.mp3', found 'Data/audio/music.mp3'"
- [ ] Test with intentionally mismatched case (rename files, verify game finds them)

**Deliverable**: Case-insensitive file lookup working, no "file not found" errors

### D. Input Handling Edge Cases

**Goal**: Ensure SDL3 input is robust and responsive

**Tasks**:
- [ ] Test keyboard edge cases:
  - Rapid key spam (queue overflow?)
  - Modifier keys (Shift, Ctrl, Alt + other keys)
  - International keyboards (AZERTY, QWERTZ)
  - Key repeat rate (SDL vs. game input system)
  
- [ ] Test mouse edge cases:
  - High DPI mice (sensitivity issues?)
  - Mouse grab/ungrab (alt+tab behavior)
  - Mouse wheel scrolling (map zoom)
  - Multi-monitor setups (cursor confined to game window?)

- [ ] Test window management:
  - Fullscreen toggle (Alt+Enter)
  - Minimize/restore
  - Resize window (aspect ratio handling)
  - Focus loss during gameplay (auto-pause?)

- [ ] Test controller support (if applicable):
  - SDL3 gamepad API (future enhancement)
  - Note: Original game is keyboard/mouse only, low priority

**Deliverable**: Input handling polished, no dropped inputs or lag

### E. Error Handling & Graceful Degradation

**Goal**: Game fails gracefully, not with cryptic crashes

**Tasks**:
- [ ] Add error messages for common failures:
  - "OpenAL device not found: Install libopenal1"
  - "Vulkan not supported: Update GPU drivers"
  - "Asset file missing: Verify game installation"

- [ ] Implement fallback behaviors:
  - If OpenAL fails: Warn user, continue with silent audio
  - If video fails: Skip video, continue to next scene
  - If shader compilation fails: Fall back to simpler shader

- [ ] Add debug logging:
  - Environment variables: `GENERALSX_DEBUG=1` for verbose logs
  - Log file: `~/.local/share/Generals/debug.log`
  - Log DXVK output: `DXVK_LOG_LEVEL=info`

- [ ] Test error paths:
  - Unplug audio device mid-game
  - Kill Vulkan driver
  - Delete asset files
  - Verify game doesn't crash

**Deliverable**: User-friendly error messages, no silent crashes

### F. Multi-Distro Compatibility Testing

**Goal**: Ensure game works on major Linux distributions

**Target Distros**:
- [ ] Ubuntu 22.04 LTS (stable, large user base)
- [ ] Ubuntu 26.04 LTS (latest LTS)
- [ ] Fedora 40+ (cutting edge, latest Mesa)
- [ ] Arch Linux (rolling release, community favorite)
- [ ] Debian 12 (stable, server-oriented but some gamers)
- [ ] Steam Deck (SteamOS 3.x, unique use case)

**Test Matrix**:
| Distro | Vulkan Driver | GPU | Status | Notes |
|--------|---------------|-----|--------|-------|
| Ubuntu 22.04 | Mesa RADV | AMD RX 6800 | ❓ | Test here |
| Ubuntu 26.04 | Mesa RADV | AMD RX 6800 | ❓ | Test here |
| Fedora 40 | Mesa RADV | AMD RX 7900 | ❓ | Test here |
| Arch | Mesa RADV | AMD RX 580 | ❓ | Test here |
| Ubuntu 22.04 | NVIDIA 550 | RTX 3080 | ❓ | Test here |
| Steam Deck | Mesa RADV | AMD Van Gogh | ❓ | Test here |

**Dependency Verification**:
- [ ] Document required packages:
  ```bash
  # Ubuntu/Debian
  sudo apt install libopenal1 libsdl3-0 libvulkan1
  
  # Fedora
  sudo dnf install openal-soft SDL3 vulkan-loader
  
  # Arch
  sudo pacman -S openal sdl3 vulkan-icd-loader
  ```
- [ ] Create dependency check script:
  ```bash
  scripts/check_linux_deps.sh
  ```

**Deliverable**: Compatibility matrix, installation docs for each distro

### G. Packaging & Distribution

**Goal**: Easy installation for end users

**Option 1: AppImage** (recommended for broad compatibility):
- [ ] Create `GeneralsXZH.AppImage`:
  - Bundle game binary, DXVK libs, SDL3, OpenAL
  - Single file, no installation required
  - Works on all distros (Ubuntu 20.04+)
- [ ] Test AppImage on all target distros
- [ ] Document: "Download GeneralsXZH.AppImage, chmod +x, run"

**Option 2: Flatpak** (sandboxed, modern):
- [ ] Create `com.ea.GeneralsZeroHour.yml` manifest
- [ ] Submit to Flathub (requires legal approval from EA)
- [ ] Pro: Automatic updates, sandboxed
- [ ] Con: Complex setup, may not allow original game assets

**Option 3: Native packages** (distro-specific):
- [ ] Create `.deb` (Ubuntu/Debian):
  ```bash
  scripts/build_deb.sh
  ```
- [ ] Create `.rpm` (Fedora/RHEL):
  ```bash
  scripts/build_rpm.sh
  ```
- [ ] Create `PKGBUILD` (Arch AUR):
  ```bash
  # Community-maintained, submit to AUR
  ```
- [ ] Pro: Native integration with package manager
- [ ] Con: Maintenance burden (separate build for each distro)

**Option 4: Tarball** (manual installation):
- [ ] Create `GeneralsXZH-linux-x64.tar.xz`:
  - Binary + README + install script
  - User extracts to `/opt/` or `~/Games/`
- [ ] Simple but less polished

**Recommendation**: Start with AppImage (easiest for users), consider Flatpak later

**Deliverable**: Packaged release, installation instructions

### H. Documentation & User Guides

**Goal**: Users can install and troubleshoot without developer help

**Documents to Create**:
- [ ] `INSTALL_LINUX.md`:
  - System requirements (CPU, GPU, RAM, distro)
  - Installation steps (AppImage, native packages)
  - First launch troubleshooting
  - Asset installation (original game required)

- [ ] `TROUBLESHOOTING_LINUX.md`:
  - "Game won't launch" → Check Vulkan, drivers
  - "No audio" → Check OpenAL, PulseAudio config
  - "Black screen" → DXVK logs, Mesa version
  - "Crash on startup" → Debug logs, environment variables
  - "Performance issues" → DXVK_HUD, Vulkan settings

- [ ] `BUILDING_LINUX.md`:
  - Build from source instructions
  - CMake presets, dependencies
  - Docker build workflow
  - For developers and contributors

- [ ] `COMMAND_LINE_PARAMETERS.md` (expand existing):
  - Linux-specific flags
  - DXVK environment variables
  - SDL3 environment variables

**Deliverable**: Complete user-facing documentation

### I. Community Feedback & Iteration

**Goal**: Incorporate user feedback before 1.0 release

**Tasks**:
- [ ] **Beta release** (GitHub Releases, Discord, Reddit):
  - Tag: `v1.0-beta1-linux`
  - Announce: "Linux port beta, testers wanted"
  - Provide AppImage, source tarball
  - Request feedback: Bug reports, performance, compatibility

- [ ] **Collect feedback**:
  - GitHub Issues (bugs, feature requests)
  - Community Discord (real-time feedback)
  - Reddit threads (broader audience)

- [ ] **Triage and prioritize**:
  - P0: Crashes, game-breaking bugs → Fix immediately
  - P1: Major bugs, common issues → Fix for 1.0
  - P2: Minor bugs, polish → Fix for 1.1
  - P3: Enhancement requests → Future consideration

- [ ] **Iterate**:
  - Release `v1.0-beta2`, `beta3`, etc.
  - Each beta fixes reported issues
  - Goal: Stable 1.0 release with <10 open P0/P1 bugs

**Deliverable**: Community-tested release, issue tracker with backlog

---

## Acceptance Criteria (Phase 4)

Phase 4 is **COMPLETE** when:
- [ ] No P0 (crash) bugs in issue tracker
- [ ] <5 P1 (major) bugs remaining (documented, low impact)
- [ ] Linux performance within 10% of Windows (FPS benchmark)
- [ ] No memory leaks (Valgrind clean)
- [ ] Case-insensitive filesystem working (no "file not found" errors)
- [ ] Passes 2-hour stress test (no crashes, minimal performance drop)
- [ ] Works on Ubuntu 22.04, 26.04, Fedora 40, Arch (verified)
- [ ] AppImage or Flatpak release available
- [ ] Documentation complete (`INSTALL_LINUX.md`, `TROUBLESHOOTING_LINUX.md`)
- [ ] Community beta tested (>10 testers, feedback incorporated)

---

## Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|------------|
| Unknown bugs (black swan) | High | Comprehensive testing, beta releases |
| Performance regressions | Medium | Profile early, compare to Windows baseline |
| Driver incompatibilities | Medium | Test multiple GPUs, document known issues |
| Community rejection | Low | Engage early, incorporate feedback |
| Legal issues (packaging) | Low | Require users own original game, no copyrighted assets in repo |

---

## Timeline (Estimated)

- **Week 1-2**: Bug bash, smoke tests, stress tests
- **Week 3**: Performance profiling, optimization
- **Week 4**: Filesystem case sensitivity, input edge cases
- **Week 5**: Multi-distro testing, packaging
- **Week 6**: Documentation, beta release preparation
- **Week 7-8**: Community beta, feedback iteration
- **Week 9**: Release candidate, final polish
- **Week 10**: 1.0 release

**Total Estimate**: 10 weeks (2.5 months)

---

## Deliverables

- `docs/WORKDIR/audit/phase4-bugs.md` (bug tracker)
- `docs/WORKDIR/reports/phase4-performance.md` (benchmark results)
- `docs/INSTALL_LINUX.md` (user installation guide)
- `docs/TROUBLESHOOTING_LINUX.md` (user troubleshooting)
- `docs/BUILDING_LINUX.md` (developer build guide)
- `scripts/check_linux_deps.sh` (dependency checker)
- `GeneralsXZH.AppImage` (packaged release)
- GitHub Issues triaged and prioritized
- Community beta feedback incorporated

---
Next Steps

**Phase 5: macOS Port** (Now prioritized, see PHASE05_MACOS_PORT.md):
- Port Linux build to macOS using MoltenVK
- Reuse SDL3, OpenAL, and Vulkan code (70-80% compatible)
- Estimated effort: 20-40 hours
- Performance: ~80-90 FPS (10-20% overhead vs Windows)

**Future Phases (6+)**:
- Multiplayer/network testing (LAN, online, replay compatibility)
- Mod compatibility (community mods, map packs)
- Steam Deck optimizations (controller support, Big Picture modet, Big Picture mode)
- macOS native build (separate phase, complex)
- Wayland support (currently X11 via XWayland)
- High DPI scaling improvements
- Localization (non-English languages)
- Achievement system (Steam integration?)
- Community content hub (mod browser, map sharing)

---

## Appendix A: Future Architecture Proposals

### A1. SDL3 Audio Migration (Phase 4 Strategic Priority)

**Proposal**: Migrate from OpenAL to SDL3 Audio backend.

**Strategic Significance** ⭐⭐⭐:
This migration represents the **final liberation** of a 23-year-old proprietary game engine:
- **Original (2003)**: Miles Sound System + Bink Video (RAD Game Tools proprietary), DirectX (Microsoft proprietary)
- **Phase 2 (Current)**: OpenAL (open) + FFmpeg (open) + Vulkan (open) - mostly free but still 4 libraries
- **Phase 4 Goal**: **100% open source**, unified under single SDL3 library (graphics + audio)
- **Outcome**: Fully auditable, zero proprietary tech, community-maintainable indefinitely

**Technical Rationale**:
- Unify graphics (SDL3) + audio (SDL3) under single library ecosystem
- Reduce external dependencies (4 libs → 3 libs, no `libopenal`)
- Automatic audio/video sync (FFmpeg integration in Phase 3)
- Benefit Phase 5 (macOS port) with unified SDL3 system

**When to Consider**:
✅ After Phase 2 (OpenAL) is stable  
✅ Audio quality validated with user feedback  
✅ Strategic goal matters to community (\"fully open source\" narrative)  
✅ Not blocking critical Path-to-Release  
❌ NOT during Phase 2 development (too risky)

**Effort**: ~24-35 hours (3-4 developer-days)
- Phase 2 Design: 2-3 hours
- Phase 3 Prototype: 8-12 hours
- Integration: 8-12 hours
- Optimization: 4-8 hours

**Full Proposal**: See [future-sdl3-audio-migration.md](../support/future-sdl3-audio-migration.md)

**Decision Criteria** (collect End of Phase 2):
1. Is audio quality acceptable? (target >90% positive)
2. Is FPS impact acceptable? (target <5% overhead)
3. Does strategic goal (\"100% open source\") matter to community/stakeholders?

If all "YES", SDL3 Audio becomes **HIGH PRIORITY** Phase 4 slot. Otherwise, defer (keep OpenAL).

---

## Notes

- **Phase 4 is where the port becomes "production-ready"**: Don't rush, quality matters
- **Community involvement is critical**: Beta testers find bugs developers miss
- **Performance parity is a must**: If Linux is slower, adoption will suffer
- **Documentation is as important as code**: Users can't enjoy what they can't install
- **Packaging matters**: AppImage or Flatpak > tarball > "build from source"
- **Future architectures**: See Appendix A for long-term enhancement ideas (SDL3 Audio, etc.)

---

**Status Tracking**:
- [ ] A. Bug Bash
- [ ] B. Performance Profiling
- [ ] C. Filesystem Case Sensitivity
- [ ] D. Input Handling Edge Cases
- [ ] E. Error Handling & Graceful Degradation
- [ ] F. Multi-Distro Compatibility
- [ ] G. Packaging & Distribution
- [ ] H. Documentation & User Guides
- [ ] I. Community Feedback & Iteration

**Progress**: 0/9 sections complete
