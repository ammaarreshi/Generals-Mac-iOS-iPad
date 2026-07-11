# GeneralsX - Linux Build Instructions

This guide provides step-by-step instructions for building GeneralsX on Linux 64-bit systems using **Docker** (recommended) or native builds.

## ⚡ Quick Start (Docker - Recommended)

**Docker builds are the primary method** - they provide consistent, isolated builds without polluting your system.

```bash
# Clone repository
git clone https://github.com/fbraz3/GeneralsX.git
cd GeneralsX

# Build Zero Hour (GeneralsXZH) with Docker
./scripts/docker-build-linux-zh.sh linux64-deploy

# Binary location: build/linux64-deploy/GeneralsMD/GeneralsXZH
```

**Result**: Native Linux ELF binary (~177 MB) with DXVK (DirectX → Vulkan) and SDL3.

---

## 🔧 Technology Stack

- **Graphics**: DXVK (DirectX 8 → Vulkan translation)
- **Windowing/Input**: SDL3
- **Build System**: CMake + Ninja
- **Compiler**: GCC/Clang with C++20 support
- **Preset**: `linux64-deploy` (64-bit native ELF)

**No Wine required** - This is a native Linux port!

## Prerequisites

### Docker Method (Recommended)

**Easiest and cleanest approach** - no system dependencies required!

```bash
# Install Docker
# Ubuntu/Debian:
sudo apt update
sudo apt install docker.io
sudo systemctl start docker
sudo usermod -aG docker $USER  # Log out and back in after this

# Fedora:
sudo dnf install docker
sudo systemctl start docker
sudo usermod -aG docker $USER

# Arch Linux:
sudo pacman -S docker
sudo systemctl start docker
sudo usermod -aG docker $USER

# Verify Docker
docker --version
docker run --rm hello-world
```

### Native Build Method (Advanced)

### Essential Tools
- **GCC** or **Clang** compiler (C++20 support required)
- **CMake** 3.20 or higher
- **Ninja** build system (recommended)
- **Git** for repository management

### Distribution-Specific Installation (Native Builds)

**Only needed if NOT using Docker.**

#### Ubuntu/Debian
```bash
# Install build tools
sudo apt update
sudo apt install build-essential cmake ninja-build git

# Install DXVK/Vulkan dependencies
sudo apt install libvulkan-dev vulkan-tools mesa-vulkan-drivers

# Install SDL3 dependencies
sudo apt install libsdl3-dev

# Install additional dependencies
sudo apt install libgl1-mesa-dev libglu1-mesa-dev libasound2-dev
```

#### Fedora
```bash
# Install build tools
sudo dnf install gcc-c++ cmake ninja-build git

# Install DXVK/Vulkan dependencies
sudo dnf install vulkan-loader vulkan-tools mesa-vulkan-drivers

# Install SDL3 dependencies
sudo dnf install SDL3-devel

# Install additional dependencies
sudo dnf install mesa-libGL-devel mesa-libGLU-devel alsa-lib-devel
```

#### Arch Linux
```bash
# Install build tools
sudo pacman -S base-devel cmake ninja git

# Install DXVK/Vulkan dependencies
sudo pacman -S vulkan-icd-loader vulkan-tools mesa

# Install SDL3 dependencies
sudo pacman -S sdl3

# Install additional dependencies
sudo pacman -S mesa glu alsa-lib
```

## Build Configuration

### Docker Method (Recommended)

```bash
# 1. Configure build (creates CMake cache inside Docker)
./scripts/docker-configure-linux.sh linux64-deploy

# 2. Build Zero Hour
./scripts/docker-build-linux-zh.sh linux64-deploy

# 3. Build Generals (optional)
./scripts/docker-build-linux-generals.sh linux64-deploy

# 4. Smoke test (optional)
./scripts/docker-smoke-test-zh.sh linux64-deploy
```

**Available Presets**:
- `linux64-deploy` - 64-bit release build (recommended)
- `linux64-testing` - 64-bit debug build

### Native Method (Advanced)

### 1. Clone Repository
```bash
git clone https://github.com/fbraz3/GeneralsX.git
cd GeneralsX
```

### 2. Configure Build (Native)
```bash
# Configure using linux64-deploy preset for 64-bit Linux
cmake --preset linux64-deploy
```

## Build Targets

### Docker Method (Recommended)

```bash
# Primary: Zero Hour (GeneralsXZH)
./scripts/docker-build-linux-zh.sh linux64-deploy
# Output: build/linux64-deploy/GeneralsMD/GeneralsXZH

# Secondary: Generals (GeneralsX)
./scripts/docker-build-linux-generals.sh linux64-deploy
# Output: build/linux64-deploy/Generals/GeneralsX
```

### Native Method

### Primary Target: Zero Hour (GeneralsXZH) - Native
```bash
# Build the main Zero Hour executable
cmake --build build/linux64-deploy --target z_generals -j 4

# Executable location: build/linux64-deploy/GeneralsMD/GeneralsXZH
```

### Secondary Target: Original Generals (GeneralsX) - Native
```bash
# Build the original Generals executable
cmake --build build/linux64-deploy --target g_generals -j 4

# Executable location: build/linux64-deploy/Generals/GeneralsX
```

### Core Libraries (Optional Testing) - Native
```bash
# Build core libraries independently
cmake --build build/linux64-deploy --target ww3d2 wwlib wwmath -j 4
```

### Build with Dynamic Core Allocation - Native
```bash
# Use all cores except one to avoid system overload
cmake --build build/linux64-deploy --target z_generals -j $(nproc --ignore=1)
```

## Debug Build Configurations

### Debug Build (Native)
```bash
cmake --preset linux64-testing
cmake --build build/linux64-testing --target z_generals -j 4
```

### Release Build (Default, Native)
```bash
cmake --preset linux64-deploy
cmake --build build/linux64-deploy --target z_generals -j 4
```

## Build Cleanup

```bash
# Clean previous build if needed
rm -rf build/linux64-deploy

# Docker: Reconfigure
./scripts/docker-configure-linux.sh linux64-deploy

# Native: Reconfigure
cmake --preset linux64-deploy
```

## Troubleshooting

### CMake can't find dependencies
```bash
# Update package manager and reinstall cmake
# Ubuntu/Debian:
sudo apt update && sudo apt upgrade cmake

# Fedora:
sudo dnf update cmake

# Arch Linux:
sudo pacman -Syu cmake
```

### Compiler version issues
```bash
# Ensure C++20 support
gcc --version  # Should be 10+ or later
clang --version  # Should be 13+ or later

# Install newer compiler if needed
# Ubuntu/Debian:
sudo apt install gcc-11 g++-11

# Fedora:
sudo dnf install gcc

# Arch Linux:
sudo pacman -S gcc
```

### Linking errors
```bash
# Clean and rebuild
rm -rf build/linux64-deploy

# Docker:
./scripts/docker-configure-linux.sh linux64-deploy
./scripts/docker-build-linux-zh.sh linux64-deploy

# Native:
cmake --preset linux64-deploy
cmake --build build/linux64-deploy --target z_generals -j 4
```

### Missing graphics libraries (DXVK/Vulkan)
```bash
# Ubuntu/Debian:
sudo apt install libvulkan-dev vulkan-tools mesa-vulkan-drivers

# Fedora:
sudo dnf install vulkan-loader vulkan-tools mesa-vulkan-drivers

# Arch Linux:
sudo pacman -S vulkan-icd-loader vulkan-tools mesa

# Verify Vulkan support:
vulkaninfo | head -20
```

### Missing audio libraries
```bash
# Ubuntu/Debian:
sudo apt install libasound2-dev

# Fedora:
sudo dnf install alsa-lib-devel

# Arch Linux:
sudo pacman -S alsa-lib
```

### Verify Build
```bash
# Verify 64-bit ELF executable
file build/linux64-deploy/GeneralsMD/GeneralsXZH
# Should show: ELF 64-bit LSB executable, x86-64

# Check DXVK dependencies
ldd build/linux64-deploy/GeneralsMD/GeneralsXZH | grep -i vulkan

# Check size (~177 MB expected)
ls -lh build/linux64-deploy/GeneralsMD/GeneralsXZH
```

## Port Status

### Current Status (February 2026)
The Linux port **Phase 1 (Graphics) is COMPLETE** ✅

**Working**:
- ✅ Native Linux ELF builds via Docker
- ✅ DXVK integration (DirectX 8 → Vulkan)
- ✅ SDL3 windowing and input
- ✅ Build system (CMake + Docker)
- ✅ Binary compilation (~177 MB native executable)

**In Progress**:
- 🔄 Phase 2 (Audio) - OpenAL integration
- 🔄 Runtime smoke testing

**Planned**:
- 📋 Phase 3 (Video) - Bink video replacement
- 📋 Phase 4+ (Polish) - Optimizations and hardening

### Implementation Timeline
- **Phase 0** (Analysis): ✅ Complete (December 2025 - January 2026)
- **Phase 1** (Graphics/DXVK): ✅ Complete (January 2026 - February 2026)
- **Phase 2** (Audio/OpenAL): 🔄 Next (February 2026 - March 2026)
- **Phase 3** (Video): 📋 Planned (Q2 2026)
- **Phase 4+** (Polish): 📋 Future (Q2-Q3 2026)

## Additional Resources

- **Development Diary**: See [../DEV_BLOG/README.md](../DEV_BLOG/README.md)
- **Phase Documentation**: See [../WORKDIR/phases/](../WORKDIR/phases/)
- **Docker Scripts**: See [../../scripts/README_DOCKER_SCRIPTS.md](../../scripts/README_DOCKER_SCRIPTS.md)
- **Reference Repos**: See [../../references/old-refs/fighter19-dxvk-port/](../../references/old-refs/fighter19-dxvk-port/)
- **DXVK Architecture**: See [../WORKDIR/support/](../WORKDIR/support/)

## Support

For Linux build-specific issues, check [Issues](https://github.com/fbraz3/GeneralsX/issues) or open a new one with the `linux` label.

---
**Last updated**: February 18, 2026
**Target Architecture**: Linux 64-bit (x86_64) native ELF
**Status**: Phase 1 (Graphics) In progress 🔄
**Technology**: DXVK (DirectX → Vulkan) + SDL3 + Docker