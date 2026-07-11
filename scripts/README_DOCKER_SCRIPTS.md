# DEPRECATED: Docker Build Scripts

⚠️ **This file has been consolidated into `README.md`.**

For all Docker build documentation, quick start guides, troubleshooting, and usage examples, see:
- **[scripts/README.md](README.md)** - Complete and current documentation

For detailed Docker workflow insights, see:
- **[docs/WORKDIR/support/DOCKER_WORKFLOW.md](../docs/WORKDIR/support/DOCKER_WORKFLOW.md)**

---

## Quick Navigation (Legacy Content Below)

The content below is kept for reference but please use the resources above for current information.

## Prerequisites

- **Docker** installed and running
- **macOS/Linux host** (scripts use bash)
- Check Docker: `docker --version` or run VS Code task "Validate: Check Docker Prerequisites"

## Quick Start

### First Time Setup

```bash
# Build Docker images (one-time, ~5-10 minutes)
./scripts/docker-build-images.sh all

# Or let build scripts auto-build images on first run
./scripts/docker-build-linux-zh.sh  # Will auto-build if missing
```

### Subsequent Builds

```bash
# Fast builds (no package installation!)
./scripts/docker-build-linux-zh.sh       # Zero Hour (Linux)
./scripts/docker-build-linux-generals.sh # Generals (Linux)
./scripts/docker-build-mingw-zh.sh       # Zero Hour (Windows .exe)
```

**Key Benefits**:
- ✅ No package installation per build (saved in image)
- ✅ No vcpkg bootstrap per build (saved in image)
- ✅ 40-50% faster build times
- ✅ Automatic image management (scripts check/build if missing)

## Scripts

### Image Management

**`docker-build-images.sh [linux|mingw|all]`** 🆕
- **Purpose**: Build pre-configured Docker images with all dependencies
- **When**: First time setup, or when updating toolchain/CMake/vcpkg
- **Images**:
  - `generalsx/linux-builder:latest` - Native Linux builds (GCC, CMake, vcpkg)
  - `generalsx/mingw-builder:latest` - MinGW cross-compilation (MinGW-w64, CMake)
- **Time**: ~5-10 minutes (one-time)

```bash
# Build all images
./scripts/docker-build-images.sh all

# Build specific image
./scripts/docker-build-images.sh linux
./scripts/docker-build-images.sh mingw

# Rebuild when toolchain changes
./scripts/docker-build-images.sh all  # Force rebuild
```

**Note**: All other scripts auto-detect and build images if missing.

## Scripts

### Configure

**`docker-configure-linux.sh [preset]`**
- Configures CMake for Linux build
- Default preset: `linux64-deploy`
- Output: `build/linux64-deploy/` directory
- Log: `logs/configure_linux64-deploy_docker.log`

```bash
./scripts/docker-configure-linux.sh
./scripts/docker-configure-linux.sh linux64-testing  # Debug variant
```

### Build Linux (Native ELF)

**`docker-build-linux-zh.sh [preset]`**
- Builds GeneralsXZH (Zero Hour) for Linux
- Native ELF binary (not Windows .exe)
- Output: `build/linux64-deploy/GeneralsMD/GeneralsXZH`
- Log: `logs/build_zh_linux64-deploy_docker.log`

```bash
./scripts/docker-build-linux-zh.sh
```

**`docker-build-linux-generals.sh [preset]`**
- Builds GeneralsX (base game) for Linux
- Output: `build/linux64-deploy/Generals/GeneralsX`
- Log: `logs/build_generals_linux64-deploy_docker.log`

```bash
./scripts/docker-build-linux-generals.sh
```

### Build Windows (MinGW Cross-Compile)

**`docker-build-mingw-zh.sh [preset]`**
- Cross-compiles Windows .exe using MinGW in Docker
- Output: `build/mingw-w64-i686/GeneralsMD/GeneralsXZH.exe`
- Log: `logs/build_zh_mingw-w64-i686_docker.log`
- **Testing**: Run in Windows VM or Wine

```bash
./scripts/docker-build-mingw-zh.sh
```

### Smoke Test

**`docker-smoke-test-zh.sh [preset]`**
- Attempts to launch Linux binary (expects crash)
- **Goal**: Check initialization output, not full run
- Useful for debugging missing libraries, initialization errors
- Log: `logs/smoke_test_zh_linux64-deploy.log`

```bash
./scripts/docker-smoke-test-zh.sh
```

## VS Code Tasks

All scripts are integrated into VS Code tasks (Cmd+Shift+P → "Tasks: Run Task"):

| Task | Script | Description |
|------|--------|-------------|
| Configure (Linux Docker) | `docker-configure-linux.sh` | Configure CMake |
| Build GeneralsXZH (Linux Docker) | `docker-build-linux-zh.sh` | Build Zero Hour (default) |
| Build GeneralsX (Linux Docker) | `docker-build-linux-generals.sh` | Build base game |
| Build Windows (MinGW Docker) | `docker-build-mingw-zh.sh` | Cross-compile Windows .exe |
| Test: Smoke Test GeneralsXZH | `docker-smoke-test-zh.sh` | Launch test (expects crash) |

## Workflow

### First Build (Phase 1)

```bash
# 1. Configure
./scripts/docker-configure-linux.sh

# 2. Build
./scripts/docker-build-linux-zh.sh

# 3. Smoke test (will crash, check logs)
./scripts/docker-smoke-test-zh.sh
```

**Expected issues** (first build):
- Missing headers (SDL3, MiniAudio, DXVK)
- Undefined symbols
- Linking errors

**Fix**: Iterate (fix errors → rebuild → fix → rebuild...)

### Windows Cross-Compile Test

```bash
# Build Windows .exe on macOS/Linux using MinGW
./scripts/docker-build-mingw-zh.sh

# Result: build/mingw-w64-i686/GeneralsMD/GeneralsXZH.exe
# Test in Windows VM or Wine
```

## Environment Variables

Scripts support environment variables:

```bash
# Custom Docker image
export DOCKER_IMAGE="ubuntu:26.04"
./scripts/docker-build-linux-zh.sh

# Custom log directory
export LOG_DIR="my-logs"
./scripts/docker-build-linux-zh.sh
```

## Troubleshooting

### "Docker not found"
```bash
# macOS
brew install --cask docker

# Open Docker Desktop and wait for it to start
docker --version
```

### "Permission denied"
```bash
# Make scripts executable
chmod +x scripts/docker-*.sh
```

### "Preset not found"
```bash
# Check CMakePresets.json for available presets
grep '"name"' CMakePresets.json
```

### Build errors
```bash
# Check logs
cat logs/build_zh_linux64-deploy_docker.log

# Clean build
rm -rf build/linux64-deploy
./scripts/docker-configure-linux.sh
./scripts/docker-build-linux-zh.sh
```

## Notes

- **No brew installs needed**: Docker containers have everything
- **macOS clean**: All builds happen in containers (no MinGW on your Mac)
- **Logs auto-created**: `logs/` directory created automatically
- **Verbose output**: All commands echo to terminal + log file
- **Exit on error**: Scripts use `set -e` (stop on first error)

## Phase 1 Status

**Current**: Building Phase 1 (DXVK + SDL3 + MiniAudio)
**Status**: 70% complete (code written, NOT yet compiled)
**Next**: First Docker build test

See: `docs/WORKDIR/phases/PHASE01_IMPLEMENTATION_PLAN.md`
