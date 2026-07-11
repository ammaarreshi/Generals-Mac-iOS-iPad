# Building GeneralsX on macOS and iOS

This guide builds Command & Conquer: Generals - Zero Hour for Apple Silicon
macOS and arm64 iPhone/iPad devices. Commands assume the repository is at:

```text
~/local/src/GeneralsX
```

The build does not include game data. Use assets from a copy of Zero Hour that
you own.

## Working directory layout

The validated local layout is:

```text
~/local/src/
  GeneralsX/       # this repository
  vcpkg/           # vcpkg checkout
  vcpkg-cache/     # reusable binary package cache
```

Set shared environment variables before building:

```sh
cd ~/local/src/GeneralsX

export GX_ROOT="$PWD"
export DEVELOPER_DIR=/Applications/Xcode-26.6.0.app/Contents/Developer
export VCPKG_ROOT="$HOME/local/src/vcpkg"
export VCPKG_DEFAULT_BINARY_CACHE="$HOME/local/src/vcpkg-cache"
export VCPKG_DISABLE_METRICS=1

mkdir -p "$VCPKG_DEFAULT_BINARY_CACHE"
```

`DEVELOPER_DIR` avoids changing the system-wide Xcode selection. Adjust the
path after an Xcode upgrade.

## One-time prerequisites

Install the command-line tools:

```sh
brew install cmake ninja meson pkgconf glslang vulkan-tools molten-vk ffmpeg xcodegen
```

Xcode must be installed, opened once, and signed into an Apple ID before an iOS
app can be provisioned. Verify the selected toolchain:

```sh
xcodebuild -version
xcrun --sdk iphoneos --show-sdk-path
```

Initialize the repository dependencies:

```sh
cd ~/local/src/GeneralsX
git submodule update --init --recursive
```

Install vcpkg if `~/local/src/vcpkg` does not exist:

```sh
git clone https://github.com/microsoft/vcpkg.git ~/local/src/vcpkg
~/local/src/vcpkg/bootstrap-vcpkg.sh
```

## Stage game assets

This machine already has the Steam assets under GameHub. Expose them to the
repository without copying them:

```sh
cd ~/local/src/GeneralsX
mkdir -p .local
ln -sfn "/Users/andy/GameHub/steamapps/common/Command & Conquer Generals - Zero Hour" \
  .local/GeneralsZH
```

Confirm that the source contains `.big` archives:

```sh
find .local/GeneralsZH -maxdepth 1 -iname '*.big' -print | head
```

Use `GX_GAME_DATA=/another/path` during iOS packaging if the assets live
somewhere else.

## Stage MoltenVK and fonts

The iOS package needs a dynamic MoltenVK framework, while the iOS engine link
needs the static MoltenVK xcframework. The pinned fetch script contains both:

```sh
cd ~/local/src/GeneralsX

GX_MOLTENVK_ROOT="$PWD/.local/MoltenVK" \
  ./scripts/build/ios/fetch-moltenvk.sh

GX_FONTS="$PWD/.local/ios-staging/fonts" \
  ./scripts/build/ios/stage-fonts.sh

mkdir -p .local/vulkan-sdk/lib
ln -sfn ../MoltenVK/MoltenVK/MoltenVK/include \
  .local/vulkan-sdk/include
ln -sfn ../../MoltenVK/MoltenVK/MoltenVK/static/MoltenVK.xcframework \
  .local/vulkan-sdk/lib/MoltenVK.xcframework
```

If macOS blocks a downloaded framework after its checksum has been verified by
the fetch script, clear its downloaded-file metadata:

```sh
xattr -cr .local/MoltenVK
```

## Create the local macOS Vulkan SDK shim

The macOS scripts accept a LunarG SDK, but this setup uses Homebrew's Vulkan
loader and MoltenVK through a local SDK-shaped directory:

```sh
cd ~/local/src/GeneralsX

mkdir -p \
  .local/vulkan-sdk/macOS/bin \
  .local/vulkan-sdk/macOS/include \
  .local/vulkan-sdk/macOS/lib/pkgconfig \
  .local/vulkan-sdk/macOS/share/vulkan/icd.d

ln -sfn "$(brew --prefix glslang)/bin/glslangValidator" \
  .local/vulkan-sdk/macOS/bin/glslangValidator
ln -sfn "$(brew --prefix vulkan-headers)/include/vulkan" \
  .local/vulkan-sdk/macOS/include/vulkan
ln -sfn "$(brew --prefix vulkan-loader)/lib/libvulkan.dylib" \
  .local/vulkan-sdk/macOS/lib/libvulkan.dylib
ln -sfn "$(brew --prefix vulkan-loader)/lib/libvulkan.1.dylib" \
  .local/vulkan-sdk/macOS/lib/libvulkan.1.dylib
ln -sfn "$(brew --prefix molten-vk)/lib/libMoltenVK.dylib" \
  .local/vulkan-sdk/macOS/lib/libMoltenVK.dylib
ln -sfn "$(brew --prefix vulkan-loader)/lib/pkgconfig/vulkan.pc" \
  .local/vulkan-sdk/macOS/lib/pkgconfig/vulkan.pc
ln -sfn "$(brew --prefix molten-vk)/etc/vulkan/icd.d/MoltenVK_icd.json" \
  .local/vulkan-sdk/macOS/share/vulkan/icd.d/MoltenVK_icd.json
```

## Build and install iOS/iPadOS

The iOS CMake preset expects the SDK root that contains `include/` and `lib/`:

```sh
cd ~/local/src/GeneralsX
export VULKAN_SDK="$PWD/.local/vulkan-sdk"

cmake --preset ios-vulkan
cmake --build build/ios-vulkan --target z_generals
```

The engine binary is produced at:

```text
build/ios-vulkan/GeneralsMD/GeneralsXZH.app/GeneralsXZH
```

Find the connected device identifier:

```sh
xcrun devicectl list devices
```

Package, sign, and install the full-assets app. These are the signing values
used by the current local setup; update them if the Xcode account changes:

```sh
cd ~/local/src/GeneralsX

export GX_TEAM_ID=CT4TV38B7S
export GX_BUNDLE_ID=com.dezhifang.generalszh
export GX_DEVICE_ID=<identifier-from-devicectl>

GX_DERIVED_DATA="$PWD/build/ios-derived" \
GX_IOS_PACKAGE_DIR="$PWD/build/ios-package" \
GX_MOLTENVK="$PWD/.local/MoltenVK/MoltenVK/MoltenVK/dynamic/MoltenVK.xcframework/ios-arm64/MoltenVK.framework" \
GX_GAME_DATA="$PWD/.local/GeneralsZH" \
GX_FONTS="$PWD/.local/ios-staging/fonts" \
  ./scripts/build/ios/package-ios-zh.sh --install
```

The signed application is left at:

```text
build/ios-package/GeneralsXZH.app
```

Launch it from the terminal when needed:

```sh
xcrun devicectl device process launch \
  --device "$GX_DEVICE_ID" "$GX_BUNDLE_ID"
```

For a code-only package, add `--dev`. That omits the approximately 2.7 GB asset
copy and only works when the device's app Documents directory already contains
usable game data. Normal device installs should use the full package.

### Incremental iOS rebuild

After changing C++ code:

```sh
cmake --build build/ios-vulkan --target z_generals

GX_DERIVED_DATA="$PWD/build/ios-derived" \
GX_IOS_PACKAGE_DIR="$PWD/build/ios-package" \
GX_MOLTENVK="$PWD/.local/MoltenVK/MoltenVK/MoltenVK/dynamic/MoltenVK.xcframework/ios-arm64/MoltenVK.framework" \
GX_GAME_DATA="$PWD/.local/GeneralsZH" \
GX_FONTS="$PWD/.local/ios-staging/fonts" \
  ./scripts/build/ios/package-ios-zh.sh --install
```

## Build and run macOS

The macOS scripts expect the platform-specific Vulkan SDK shim:

```sh
cd ~/local/src/GeneralsX
export VULKAN_SDK="$PWD/.local/vulkan-sdk/macOS"

./scripts/build/macos/build-macos-zh.sh
```

The engine binary is produced at:

```text
build/macos-vulkan/GeneralsMD/GeneralsXZH
```

Create a local runtime whose game assets are symlinks. This keeps generated
binaries and dylibs out of the Steam/GameHub installation:

```sh
cd ~/local/src/GeneralsX
mkdir -p build/macos-runtime/GeneralsZH

for item in "$PWD/.local/GeneralsZH"/*; do
  ln -sfn "$item" "$PWD/build/macos-runtime/GeneralsZH/$(basename "$item")"
done
```

Deploy the executable and runtime libraries, then launch windowed:

```sh
export GX_RUNTIME_DIR="$PWD/build/macos-runtime/GeneralsZH"

./scripts/build/macos/deploy-macos-zh.sh
./scripts/build/macos/run-macos-zh.sh -win
```

For subsequent C++ changes, skip CMake configuration:

```sh
./scripts/build/macos/build-macos-zh.sh --build-only
./scripts/build/macos/deploy-macos-zh.sh
```

## Package the macOS app

Create the ad-hoc-signed application archive:

```sh
cd ~/local/src/GeneralsX
export VULKAN_SDK="$PWD/.local/vulkan-sdk/macOS"
export GX_BUNDLE_DEFAULT_CNC_GENERALS_ZH_PATH="$PWD/build/macos-runtime/GeneralsZH"

./scripts/build/macos/bundle-macos-zh.sh
```

Output:

```text
GeneralsXZH-macos-arm64.zip
```

Extract and verify it:

```sh
rm -rf build/macos-app
mkdir -p build/macos-app
ditto -x -k GeneralsXZH-macos-arm64.zip build/macos-app

codesign --verify --deep --strict --verbose=2 \
  build/macos-app/GeneralsXZH.app
```

Launch the packaged app:

```sh
open build/macos-app/GeneralsXZH.app
```

The `.app` contains the engine and runtime libraries, but it still uses the
local game-data runtime. Keep `build/macos-runtime/GeneralsZH` available.

## Clean builds and troubleshooting

### Repository moved or Xcode was upgraded

CMake and FetchContent caches contain absolute paths. Remove only the generated
platform build directory and configure again:

```sh
cmake -E remove_directory build/ios-vulkan
cmake --preset ios-vulkan
```

For macOS, use `build/macos-vulkan` instead.

### iPad is listed but the packaging script cannot find it

Set the identifier explicitly:

```sh
export GX_DEVICE_ID=<identifier-from-xcrun-devicectl-list-devices>
```

Xcode 26.6 may report a connected trusted device as `available (paired)` rather
than `connected`; the packaging script accepts both labels.

### Useful verification and logs

```sh
file build/ios-package/GeneralsXZH.app/GeneralsXZH
codesign --verify --deep build/ios-package/GeneralsXZH.app

file build/macos-vulkan/GeneralsMD/GeneralsXZH
codesign --verify --deep --strict build/macos-app/GeneralsXZH.app
```

Build and runtime logs are written under `logs/`, including:

```text
logs/build_zh_macos-vulkan.log
logs/run_zh_macos.log
```
