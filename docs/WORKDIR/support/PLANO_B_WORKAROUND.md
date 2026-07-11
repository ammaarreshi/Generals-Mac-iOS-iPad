# Plan B — Use fighter19 reference as a workaround while CMake runs

## Context
If `cmake --preset macos-vulkan` takes more than 20 minutes, we can use the fighter19 build as a base.

## Steps

### 1. Check if fighter19 has DXVK compiled
```bash
ls -la references/old-refs/fighter19-dxvk-port/GeneralsMD/Run/
# Should have libdxvk_d3d8.0.dylib + libdxvk_d3d9.0.dylib
```

### 2. If yes, copy to our build
```bash
mkdir -p build/macos-vulkan-backup
cp -r build/macos-vulkan build/macos-vulkan-backup

# copy DXVK dylibs
cp references/old-refs/fighter19-dxvk-port/GeneralsMD/Run/libdxvk_*.d*.llib \
   build/macos-vulkan/_run/ 2>/dev/null || echo "Not available"
```

### 3. If fighter19 has no build, clone and compile
```bash
cd references/old-refs/fighter19-dxvk-port
cmake --preset linux64-deploy  # For native builds
# OR
cmake --preset macos-vulkan    # If it has a macOS preset
cmake --build build/macos-vulkan --target z_generals
```

### 4. If everything fails, use a previous build
```bash
git log --oneline build/macos-vulkan/ 2>/dev/null | head -5
# Try resetting to a previous commit that had DXVK compiled
```

## Quick Verification
```bash
file build/macos-vulkan/_deps/dxvk-src/src/dxso/dxso_compiler.cpp 2>/dev/null && \
  echo "DXVK source fetched" || echo "DXVK not yet ready"
```
