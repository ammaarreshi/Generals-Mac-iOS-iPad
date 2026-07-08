#!/bin/bash
# Package the iOS build of Zero Hour into a signed .app and install it on a device.
#
# Flow:
#   1. xcodegen + xcodebuild produce a provisioning-shell app with automatic signing
#      (bundle id me.ammaar.generalszh, team 7S264298H8, Info.plist with file sharing).
#   2. The stub executable is replaced with the real z_generals engine binary.
#   3. Runtime dylibs (DXVK d3d8/d3d9, SDL3, SDL3_image) are embedded in Frameworks/
#      and their install names rewritten to @rpath.
#   4. Everything is re-signed inside-out with the Apple Development identity and the
#      provisioning profile from step 1, preserving entitlements.
#   5. Optional: install to the first connected device via devicectl.
#
# Usage: ./scripts/build/ios/package-ios-zh.sh [--dev] [--install]
#   --dev      skip bundling the 2.7 GB of game assets (code-only iteration)
#   --install  install the packaged app to the first connected device
set -euo pipefail

DEV_MODE=0
DO_INSTALL=0
for arg in "$@"; do
    case "$arg" in
        --dev)     DEV_MODE=1 ;;
        --install) DO_INSTALL=1 ;;
        *) echo "ERROR: unknown argument '$arg' (usage: $0 [--dev] [--install])"; exit 1 ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build/ios-vulkan"
IOS_DIR="${PROJECT_ROOT}/ios"
DERIVED="${IOS_DIR}/build"
OUT_DIR="${PROJECT_ROOT}/build/ios-package"
APP_NAME="GeneralsXZH"
IDENTITY="${GX_SIGN_IDENTITY:-Apple Development}"

# Signing/bundle identity — override for your own Apple Developer account:
#   GX_TEAM_ID=ABCDE12345 GX_BUNDLE_ID=com.you.generalszh ./package-ios-zh.sh --install
TEAM_ID="${GX_TEAM_ID:-7S264298H8}"
BUNDLE_ID="${GX_BUNDLE_ID:-me.ammaar.generalszh}"

GAME_BIN="${BUILD_DIR}/GeneralsMD/GeneralsXZH.app/GeneralsXZH"
DXVK_BUILD="${BUILD_DIR}/_deps/dxvk-build-macos"

if [[ ! -f "${GAME_BIN}" ]]; then
    echo "ERROR: engine binary not found at ${GAME_BIN} — build the ios-vulkan preset first."
    exit 1
fi

echo "==> Generating Xcode project (xcodegen)"
(cd "${IOS_DIR}" && xcodegen generate --quiet)

echo "==> Building provisioning shell app"
xcodebuild -project "${IOS_DIR}/${APP_NAME}.xcodeproj" \
    -scheme "${APP_NAME}" -configuration Release \
    -destination 'generic/platform=iOS' \
    -derivedDataPath "${DERIVED}" \
    DEVELOPMENT_TEAM="${TEAM_ID}" \
    PRODUCT_BUNDLE_IDENTIFIER="${BUNDLE_ID}" \
    -allowProvisioningUpdates build | tail -3

SHELL_APP="${DERIVED}/Build/Products/Release-iphoneos/${APP_NAME}.app"
if [[ ! -d "${SHELL_APP}" ]]; then
    echo "ERROR: shell app not produced at ${SHELL_APP}"
    exit 1
fi

echo "==> Assembling final app"
rm -rf "${OUT_DIR}"
mkdir -p "${OUT_DIR}"
cp -R "${SHELL_APP}" "${OUT_DIR}/"
APP="${OUT_DIR}/${APP_NAME}.app"

# Replace stub executable with the engine
cp "${GAME_BIN}" "${APP}/${APP_NAME}"

# Embed runtime dylibs
mkdir -p "${APP}/Frameworks"
for lib in \
    "${DXVK_BUILD}/src/d3d8/libdxvk_d3d8.0.dylib" \
    "${DXVK_BUILD}/src/d3d9/libdxvk_d3d9.0.dylib" \
    "${BUILD_DIR}/_deps/sdl3-build/libSDL3.0.dylib" \
    "${BUILD_DIR}/_deps/sdl3_image-build/libSDL3_image.0.dylib" \
    "${BUILD_DIR}/_deps/openal_soft-build/libopenal.1.24.2.dylib" \
    "${BUILD_DIR}/libgamespy.dylib"; do
    if [[ -f "${lib}" ]]; then
        cp "${lib}" "${APP}/Frameworks/"
        echo "    embedded $(basename "${lib}")"
    else
        case "$(basename "${lib}")" in
            libgamespy.dylib)
                echo "    (skip, optional: $(basename "${lib}"))" ;;
            *)
                echo "ERROR: required dylib not built: ${lib}"
                echo "  Build first: cmake --preset ios-vulkan && cmake --build build/ios-vulkan --target z_generals"
                exit 1 ;;
        esac
    fi
done

# openal-soft's install name is libopenal.1.dylib; the embedded file must match it
if [[ -f "${APP}/Frameworks/libopenal.1.24.2.dylib" ]]; then
    mv "${APP}/Frameworks/libopenal.1.24.2.dylib" "${APP}/Frameworks/libopenal.1.dylib"
fi

# MoltenVK: DXVK dlopens @executable_path/Frameworks/MoltenVK.framework/MoltenVK.
# An app without it launches and dies at Vulkan init, so missing framework is fatal.
# (This used to live in /tmp, which the OS periodically cleans — hence the hard error.)
MVK_FRAMEWORK="${GX_MOLTENVK:-${HOME}/GeneralsX/MoltenVK/MoltenVK/MoltenVK/dynamic/MoltenVK.xcframework/ios-arm64/MoltenVK.framework}"
if [[ -d "${MVK_FRAMEWORK}" ]]; then
    cp -R "${MVK_FRAMEWORK}" "${APP}/Frameworks/"
    echo "    embedded MoltenVK.framework"
else
    echo "ERROR: MoltenVK.framework not found at ${MVK_FRAMEWORK}"
    echo "  Run scripts/build/ios/fetch-moltenvk.sh (pinned version + checksum)."
    
    exit 1
fi

# Game assets inside the bundle (iOS-sanctioned home for read-only resources):
# the app is fully self-contained, nothing lives in Documents. Skip with --dev
# for fast code-only iterations (the engine falls back to Documents assets).
GAME_DATA_SRC="${GX_GAME_DATA:-${HOME}/GeneralsX/GeneralsZH}"
FONTS_SRC="${GX_FONTS:-${HOME}/GeneralsX/ios-staging/fonts}"
CONFIG_SRC="${GX_CONFIG:-${IOS_DIR}/config}"
if [[ "${DEV_MODE}" != "1" ]]; then
    echo "==> Bundling game assets into the app"
    mkdir -p "${APP}/GameData"
    rsync -a --exclude=".*" \
        --exclude="*.dylib" --exclude="run.sh" --exclude="GeneralsXZH" \
        --exclude="GeneralsXZH.dxvk-cache" --exclude="*_d3d9.log" \
        --exclude="MoltenVK_icd.json" --exclude="dxvk.conf" --exclude="fontconfig" \
        --exclude="*.DLL" --exclude="*.dll" --exclude="*.dat" --exclude="*.ico" \
        --exclude="*.bmp" --exclude="*.doc" --exclude="*.lcf" --exclude="Launcher.txt" \
        --exclude="MSS" --exclude="Manuals" --exclude="steamapps" \
        --exclude="steam_appid.txt" --exclude="00000000.*" \
        --exclude="RedistInstallers" --exclude="_CommonRedist" --exclude="*.txt" \
        "${GAME_DATA_SRC}/" "${APP}/GameData/"
    if [[ -d "${FONTS_SRC}" ]]; then
        mkdir -p "${APP}/GameData/fonts"
        cp "${FONTS_SRC}"/*.ttf "${APP}/GameData/fonts/"
    else
        echo "ERROR: fonts not staged at ${FONTS_SRC} — the app would render no text."
        echo "  Run scripts/build/ios/stage-fonts.sh once (downloads Liberation fonts and"
        echo "  renames them to the arial/couriernew/timesnewroman names the game expects)."
        exit 1
    fi
    for cfg in dxvk.conf Options.ini; do
        if [[ ! -f "${CONFIG_SRC}/${cfg}" ]]; then
            echo "ERROR: ${CONFIG_SRC}/${cfg} missing (should ship with the repo in ios/config/)"
            exit 1
        fi
    done
    cp "${CONFIG_SRC}/dxvk.conf" "${APP}/GameData/dxvk.conf"
    cp "${CONFIG_SRC}/Options.ini" "${APP}/GameData/DefaultOptions.ini"
    echo "    bundled $(du -sh "${APP}/GameData" | cut -f1) of game data"
fi

# Loose icon PNGs alongside the compiled asset catalog: SpringBoard on some
# iOS versions won't read Assets.car icons from developer-signed sideloads
# until a reboot, but it always honors CFBundleIconFiles PNGs in the bundle root.
ICON_SRC="${IOS_DIR}/Stub/Assets.xcassets/AppIcon.appiconset/icon.png"
if [[ -f "${ICON_SRC}" ]]; then
    sips -z 120 120 "${ICON_SRC}" --out "${APP}/AppIcon60x60@2x.png"  >/dev/null
    sips -z 152 152 "${ICON_SRC}" --out "${APP}/AppIcon76x76@2x.png"  >/dev/null
    sips -z 167 167 "${ICON_SRC}" --out "${APP}/AppIcon83.5x83.5@2x.png" >/dev/null
    echo "    icon PNG fallbacks added"
fi

# Point the executable's rpath at the embedded frameworks
install_name_tool -add_rpath "@executable_path/Frameworks" "${APP}/${APP_NAME}" 2>/dev/null || true

echo "==> Re-signing"
ENTITLEMENTS="${OUT_DIR}/entitlements.plist"
codesign -d --entitlements - --xml "${SHELL_APP}" > "${ENTITLEMENTS}" 2>/dev/null

for f in "${APP}/Frameworks/"*.dylib; do
    [[ -f "$f" ]] && codesign --force --sign "${IDENTITY}" --timestamp=none "$f"
done
if [[ -d "${APP}/Frameworks/MoltenVK.framework" ]]; then
    codesign --force --sign "${IDENTITY}" --timestamp=none "${APP}/Frameworks/MoltenVK.framework"
fi
codesign --force --sign "${IDENTITY}" --timestamp=none \
    --entitlements "${ENTITLEMENTS}" "${APP}"

codesign --verify --deep "${APP}" && echo "    signature OK"

echo "==> App ready: ${APP}"

if [[ "${DO_INSTALL}" == "1" ]]; then
    echo "==> Installing to connected device"
    # Match only valid device-identifier shapes on the "connected" row, so a
    # multi-word model name (e.g. "iPad Pro 13-inch (M4)") can never be mistaken
    # for the identifier the way column counting was. Case-insensitive because
    # devicectl prints lowercase ids. Handles:
    #   - 40-char legacy physical UDID (hex)
    #   - 25-char modern physical UDID: 00008020-001234567890ABCD (8-16)
    #   - 36-char CoreDevice/simulator UUID: 8-4-4-4-12
    # -w so the state word "connected" does not also match "disconnected".
    DEVICE_ID=$(xcrun devicectl list devices 2>/dev/null \
        | grep -iw 'connected' \
        | grep -ioE '([0-9a-f]{40}|[0-9a-f]{8}-[0-9a-f]{16}|[0-9a-f]{8}-([0-9a-f]{4}-){3}[0-9a-f]{12})' \
        | head -1)
    if [[ -z "${DEVICE_ID}" ]]; then
        echo "ERROR: no connected device found (xcrun devicectl list devices)"
        exit 1
    fi
    xcrun devicectl device install app --device "${DEVICE_ID}" "${APP}"
    echo "==> Installed. Copy game assets to the app's Documents via Finder/Files app."
fi
