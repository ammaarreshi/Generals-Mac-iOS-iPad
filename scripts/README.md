# Scripts Directory

This folder is organized by function for easier maintenance and discovery.

## Directory Structure

### `build/` - Build & Deployment Scripts per Platform

#### `build/linux/` - Linux & Docker Build
Scripts for Linux native and Docker-based builds:
- `build-linux-appimage-generals.sh` - Package GeneralsX as AppImage (portable single-file Linux distribution)
- `docker-configure-linux.sh` - Configure CMake for Linux build
- `docker-build-linux-zh.sh` - Build GeneralsXZH (Zero Hour) for Linux
- `docker-build-linux-generals.sh` - Build GeneralsX (base game) for Linux
- `docker-build-mingw-zh.sh` - Cross-compile Windows .exe via MinGW in Docker
- `build-linux-appimage-zh.sh` - Package GeneralsXZH as AppImage (portable single-file Linux distribution)
- `bundle-linux-zh.sh` - Bundle compiled binaries
- `deploy-linux-zh.sh` - Deploy to runtime directory
- `run-linux-zh.sh` - Launch the game windowed

#### `build/macos/` - macOS Build
- `build-macos-zh.sh` - Configure + build GeneralsXZH
- `build-macos-generals.sh` - Configure + build GeneralsX
- `bundle-macos-zh.sh` - Bundle app
- `bundle-macos-generals.sh` - Bundle app
- `deploy-macos-zh.sh` - Deploy binaries
- `deploy-macos-generals.sh` - Deploy binaries
- `run-macos-zh.sh` - Launch the game

#### `build/windows/` - Windows Build (Pending)
Reserved for modern Windows toolchain (VS2022 + SDL3 + DXVK + MiniAudio)

### `env/` - Environment Setup

#### `env/docker/` - Docker Configuration
- `docker-build-images.sh` - Build pre-configured Docker images (Linux + MinGW)
- `docker-install.sh` - Docker environment validation

#### `env/cache/` - Compiler Cache
- `setup_ccache.sh` - Configure ccache (GCC/Clang)
- `test_ccache.sh` - Test ccache functionality
- `setup_sccache.ps1` - Configure sccache (Windows)
- `test_sccache.ps1` - Test sccache functionality

### `tooling/` - Code Analysis & Utilities

#### `tooling/clang-tidy/` - Custom clang-tidy Plugin
- `plugin/` - Custom clang-tidy checks source (C++ checks for AsciiString, Singleton patterns)
- `run.py` - Unified clang-tidy runner with batch processing and quiet output

#### `tooling/cpp/maintenance/` - C++ Code Maintenance
Utilities for large-scale code refactoring and fixes:
- `fix_*.py` - Targeted fixes (matrix conversions, water rendering, noise, debug logging, Windows API)
- `monitor-dxvk-build.py` - DXVK build monitoring tool
- `*_refactor_*.py` - Code transformation scripts (string classes, etc.)
- `remove_*.py` / `replace_*.py` - Include guard and pragma cleanup
- `unify_move_files.py` - Move files between Generals/GeneralsMD/Core with CMakeLists.txt updates

### `qa/` - Quality Assurance & Testing

#### `qa/smoke/` - Smoke Tests
- `docker-smoke-test-zh.sh` - Quick startup validation (expects crash, checks init output)
- `run-bundled-game.sh` - Test bundled binary after deployment
- `collect-flatpak-vulkan-wsi-report.sh` - Collect reproducible Flatpak Vulkan/XCB diagnostics for upstream runtime issues

### `legacy/` - Deprecated & Compatibility

#### `legacy/compat/` - Old Scripts
- `docker-build.sh` / `dockerbuild.sh` - Deprecated Docker wrappers
- `apply-patch-13-manual.sh` - Historical patch utility
- `promote-linux-attempt-to-main.sh` - Promotion helper (legacy)

### Deprecated: `cpp/` and `clang-tidy-plugin/`
Backward-compatibility wrappers. See their README files for migration info.

---

## Quick Start

### Docker Prerequisites

```bash
# Check Docker installation
docker --version

# macOS: Install Docker Desktop
brew install --cask docker
```

### First Linux Build

```bash
# 1. Build Docker images (one-time, ~5-10 min)
./scripts/env/docker/docker-build-images.sh all

# 2. Configure
./scripts/build/linux/docker-configure-linux.sh

# 3. Build
./scripts/build/linux/docker-build-linux-zh.sh

# 4. Smoke test (will crash; check logs)
./scripts/qa/smoke/docker-smoke-test-zh.sh

# 5. Deploy
./scripts/build/linux/deploy-linux-zh.sh

# 6. Run
./scripts/build/linux/run-linux-zh.sh -win

# 7. Optional: build AppImage package
./scripts/build/linux/build-linux-appimage-zh.sh linux64-deploy

# 7b. Optional: build AppImage package for base Generals
./scripts/build/linux/build-linux-appimage-generals.sh linux64-deploy

# 8. Optional: run AppImage with explicit asset paths
CNC_GENERALS_ZH_PATH="/path/to/GeneralsZH_or_GeneralsMD" \
CNC_GENERALS_PATH="/path/to/Generals" \
./build/GeneralsXZH-linux64-deploy-x86_64.AppImage -win
```

### macOS Build

```bash
# All-in-one (configure + build + deploy + run)
./scripts/build/macos/build-macos-zh.sh

# Or step-by-step:
./scripts/build/macos/build-macos-zh.sh --build-only
./scripts/build/macos/deploy-macos-zh.sh
./scripts/build/macos/run-macos-zh.sh -win
```

### Windows Cross-Compile (from Linux/macOS)

```bash
# Build Windows .exe via MinGW in Docker
./scripts/build/linux/docker-build-mingw-zh.sh

# Output: build/mingw-w64-i686/GeneralsMD/GeneralsXZH.exe
# Test in Windows VM or Wine
```

---

## Docker Workflow

### Image Management

**`docker-build-images.sh [linux|mingw|all]`**

Builds pre-configured Docker images with all dependencies pre-installed (vcpkg, CMake, toolchains).

```bash
# Build both images (one-time setup, ~5-10 minutes)
./scripts/env/docker/docker-build-images.sh all

# Build specific image
./scripts/env/docker/docker-build-images.sh linux
./scripts/env/docker/docker-build-images.sh mingw
```

**Benefits**:
- ✅ 40-50% faster builds (no package installation per build)
- ✅ vcpkg shared volume (`~/.generalsx/vcpkg`)
- ✅ Image sizes: ~90MB (Linux), ~660MB (MinGW)
- ✅ Auto-detection (all build scripts check/build images if missing)

### Environment Variables

Scripts support customization:

```bash
# Custom Docker image base
export DOCKER_IMAGE="ubuntu:26.04"
./scripts/build/linux/docker-build-linux-zh.sh

# Flatpak PoC: inject newer libxcb/X11 libs from an external directory
export LIBXCB_POC_DIR="$PWD/flatpak/poc-libxcb"
./scripts/build/linux/build-linux-flatpak.sh linux64-deploy GeneralsMD

# Custom log directory
export LOG_DIR="my-logs"
./scripts/build/linux/docker-build-linux-zh.sh

# Verbose output
export VERBOSE=1
```

---

## VS Code Tasks

All scripts are integrated into VS Code tasks (Cmd+Shift+P → "Tasks: Run Task"):

| Task | Script | Description |
|------|--------|-------------|
| [Linux] Configure (Docker) | `build/linux/docker-configure-linux.sh` | Configure CMake |
| [Linux] Build GeneralsXZH | `build/linux/docker-build-linux-zh.sh` | Build Zero Hour |
| [Linux] Build GeneralsX | `build/linux/docker-build-linux-generals.sh` | Build base game |
| [Linux] Deploy GeneralsXZH | `build/linux/deploy-linux-zh.sh` | Deploy binaries |
| [Linux] Run GeneralsXZH | `build/linux/run-linux-zh.sh -win` | Launch game |
| [macOS] Build GeneralsXZH | `build/macos/build-macos-zh.sh` | Build + deploy + run |
| [macOS] Build GeneralsX | `build/macos/build-macos-generals.sh` | Build base game |
| [macOS] Deploy GeneralsXZH | `build/macos/deploy-macos-zh.sh` | Deploy binaries |
| [macOS] Deploy GeneralsX | `build/macos/deploy-macos-generals.sh` | Deploy binaries |
| [macOS] Bundle GeneralsXZH | `build/macos/bundle-macos-zh.sh` | Bundle app (.app + zip) |
| [macOS] Bundle GeneralsX | `build/macos/bundle-macos-generals.sh` | Bundle app (.app + zip) |
| Validate: Check Docker Prerequisites | Verify Docker works | Pre-flight check |

---

## Troubleshooting

### "Docker not found"
```bash
# macOS
brew install --cask docker

# Start Docker Desktop and verify
docker --version
```

### "Permission denied" on scripts
```bash
# Make all scripts executable
find . -name "*.sh" -exec chmod +x {} \;
```

### "Preset not found"
```bash
# List available CMake presets
grep '"name"' CMakePresets.json
```

### Build errors or lingering state
```bash
# Check most recent build log
cat logs/build_zh_*.log | tail -50

# Clean build (remove cached objects)
rm -rf build/linux64-deploy
./scripts/build/linux/docker-configure-linux.sh
./scripts/build/linux/docker-build-linux-zh.sh
```

### Docker image issues
```bash
# List Docker images
docker images | grep generalsx

# Remove and rebuild images
docker rmi generalsx/linux-builder:latest generalsx/mingw-builder:latest
./scripts/env/docker/docker-build-images.sh all
```

---

## Notes

- **No brew/apt installs** during build: Docker containers pre-install all dependencies
- **Logs auto-created**: `logs/` directory created automatically with descriptive names
- **Verbose output**: All commands echo to terminal + log file
- **Error exit**: Scripts use `set -e` (stop immediately on first error)
- **Backward compatibility**: Old script paths (`scripts/run-clang-tidy.py`, `scripts/cpp/`) still work; forward compatibility maintained
- **macOS bundle dylibs**: macOS bundle scripts include non-system linked dylibs discovered via `otool -L` (including Homebrew paths when linked)
- **macOS bundle toggle**: set `GX_BUNDLE_INCLUDE_EXTERNAL_DYLIBS=0` to disable external dylib scanning when producing smaller local test artifacts

---

## For More Information

- **Build System**: See [CMakePresets.json](../CMakePresets.json)
- **Phase 1 Details**: See [docs/WORKDIR/phases/PHASE01_IMPLEMENTATION_PLAN.md](../docs/WORKDIR/phases/PHASE01_IMPLEMENTATION_PLAN.md)
- **Docker Workflow**: See [docs/WORKDIR/support/DOCKER_WORKFLOW.md](../docs/WORKDIR/support/DOCKER_WORKFLOW.md)
- **Instructions**: See [.github/instructions/scripts.instructions.md](../.github/instructions/scripts.instructions.md)
