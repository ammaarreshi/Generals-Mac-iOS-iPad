# GeneralsX Linux Port - Setup Summary

**Date**: 2026-02-07
**Agent**: Bender (in top form)
**Status**: Phase 0 setup complete, ready to begin analysis
**Location**: `docs/WORKDIR/planning/SETUP_SUMMARY.md`

---

## What Was Accomplished

### 1. 📋 New Instructions Document
**File**: `.github/instructions/generalsx.instructions.md`

Completely rewrote project instructions with focus on Linux port via:
- **Phase 0**: Deep analysis & planning (CURRENT)
- **Phase 1**: Linux graphics (DXVK integration from fighter19)
- **Phase 2**: Linux audio (OpenAL from jmarshall)
- **Phase 3**: Video playback investigation (spike)
- **Phase 4**: Polish & hardening

**Key changes**:
- ✅ Preserves Windows builds (VC6 + Win32) as non-negotiable
- ✅ Defines Docker-based Linux build strategy (native ELF compilation)
- ✅ Clear acceptance criteria for each phase
- ✅ Platform isolation patterns documented
- ✅ Reference repository consultation workflow

**Backup**: Original saved to `.github/instructions/generalsx.instructions.md.backup`

### 2. 📁 Documentation Structure
Created proper documentation hierarchy following `docs.instructions.md`:

```
docs/
├── DEV_BLOG/
│   └── 2026-02-DIARY.md               # Update before every commit
├── WORKDIR/                            # Active work
│   ├── phases/
│   │   └── PHASE00_ANALYSIS_PLANNING.md
│   ├── planning/                      # Strategic docs
│   ├── reports/                       # Session summaries
│   ├── support/                       # Technical discoveries
│   ├── audit/                         # Audit records
│   └── lessons/                       # Lessons learned
└── ETC/
    └── COMMAND_LINE_PARAMETERS.md     # Game parameters reference
```

### 3. 🔧 VS Code Tasks (Linux/macOS)
**File**: `.vscode/tasks.json`

Configured tasks for Docker-based builds (no toolchain installs needed!):
- `Configure (Linux Docker, linux64-deploy)` - Set up Linux build in container
- `Build GeneralsXZH (Linux Docker)` - Build Zero Hour for Linux (native ELF)
- `Build GeneralsX (Linux Docker)` - Build Generals for Linux (native ELF)
- `Build Windows (MinGW Docker)` - Build Zero Hour for Windows (via MinGW)
- `Pipeline: Docker Build ZH (Full)` - Full Docker Linux pipeline
- `Validate: Check Docker Prerequisites` - Verify Docker installation
- `Docs: Update Dev Blog` - Quick diary access

**All tasks**:
- Create `logs/` directory automatically
- Capture output with `tee` to log files
- Use bash/zsh (not PowerShell) for cross-platform compatibility

### 4. 📖 Documentation Files Created

| File | Purpose |
|------|---------|
| `docs/DEV_BLOG/2026-02-DIARY.md` | Development diary template |
| `docs/WORKDIR/phases/PHASE00_ANALYSIS_PLANNING.md` | Phase 0 progress tracking |
| `docs/ETC/COMMAND_LINE_PARAMETERS.md` | Game launch parameters |
| `docs/NEXT_STEPS.md` | You are here - what to do next |

---

## What You Need To Do Now

### Prerequisites (Install First)

```bash
# Install Docker on macOS (if not already installed)
brew install --cask docker
# Or download from: https://www.docker.com/products/docker-desktop

# Verify Docker is working
docker --version
docker run hello-world

# Pull Ubuntu base image (used for Linux AND Windows MinGW builds)
docker pull ubuntu:22.04

# That's it! No need to install MinGW, GCC, or any toolchains via brew!
# Docker containers handle everything.
```

### Phase 0: Begin Analysis

Your mission (should you choose to accept it):

1. **Understand Current Architecture**:
   - How does DX8 initialize?
   - Where's device management?
   - What are the render pipelines?

2. **Study fighter19's DXVK Port**:
   - Query with DeepWiki: `Fighter19/CnC_Generals_Zero_Hour`
   - Compare files: `diff -r GeneralsMD/ references/old-refs/fighter19-dxvk-port/GeneralsMD/`
   - Document what applies to Zero Hour
   - **Important**: fighter19 compiles NATIVE Linux ELF binaries (not Windows PE)
   - Uses GCC/Clang on Linux (not MinGW)

3. **Study jmarshall's OpenAL**:
   - Query with DeepWiki: `jmarshall2323/CnC_Generals_Zero_Hour`
   - Note: Generals-only, needs Zero Hour adaptation
   - Document audio pipeline and Miles→OpenAL mapping

4. **Create Analysis Documents**:
   Create these files in `docs/WORKDIR/support/`:
   - `phase0-renderer-architecture.md`
   - `phase0-fighter19-analysis.md`
   - `phase0-jmarshall-analysis.md`
   - `phase0-platform-abstraction.md`
   - `phase0-build-system.md`
   - `phase0-risk-assessment.md`

5. **Update Dev Blog**:
   Before every commit, update `docs/DEV_BLOG/2026-02-DIARY.md`

### Reference Repositories Setup (Optional but Recommended)

```bash
# Set up git remotes for easy diffing
cd references/old-refs/fighter19-dxvk-port
git remote add upstream https://github.com/Fighter19/CnC_Generals_Zero_Hour.git

cd ../jmarshall-win64-modern
git remote add upstream https://github.com/jmarshall2323/CnC_Generals_Zero_Hour.git

cd ../thesuperhackers-main
git remote add upstream https://github.com/TheSuperHackers/GeneralsGameCode.git

# Now you can easily fetch and compare
git fetch upstream
git diff upstream/main -- path/to/file
```

---

## Key Principles (Don't Forget)

1. **Windows compatibility is non-negotiable**: VC6 and Win32 builds must work
2. **Research first, code later**: Phase 0 is analysis-heavy
3. **Isolate platform code**: Linux changes stay in platform/backend layers
4. **Update dev blog before commits**: Document your thinking
5. **Use VS Code tasks**: They handle logs and environment setup

---

## Phase 0 Completion Criteria

Phase 0 is done when:
- [ ] All 6 analysis documents written
- [ ] MinGW preset configured and tested
- [ ] Windows build validation working
- [ ] Phase 1 implementation plan written
- [ ] All acceptance criteria in `PHASE00_ANALYSIS_PLANNING.md` checked

---

## Quick Commands Reference

```bash
# Check Docker installation
docker --version
docker run hello-world

# Build Linux (native ELF) in Docker
docker run --rm -v "$PWD:/work" -w /work ubuntu:22.04 bash -c "
  apt update && apt install -y build-essential cmake ninja-build && 
  cmake --preset linux64-deploy && 
  cmake --build build/linux64-deploy --target z_generals
"

# Build Windows (MinGW in Docker - no brew install!)
docker run --rm -v "$PWD:/work" -w /work ubuntu:22.04 bash -c "
  apt update && apt install -y build-essential cmake ninja-build mingw-w64 && 
  cmake --preset mingw-w64-i686 && 
  cmake --build build/mingw-w64-i686 --target z_generals
"

# Find DX8 device initialization
grep -r "IDirect3D8" --include="*.cpp" --include="*.h"

# Compare with fighter19's changes
diff -r Core/GameEngineDevice/ references/old-refs/fighter19-dxvk-port/Core/GameEngineDevice/

# Open dev blog
code docs/DEV_BLOG/2026-02-DIARY.md
```

---

## Remember

> "I'm so full of luck, it's shooting out like luck diarrhea!" - Bender

But seriously, **don't rush Phase 0**. Understanding the codebase now prevents painful refactoring later.

Good luck, meatbag. **Bite my shiny metal ass!** 🤖
