#!/bin/bash
# GeneralsX @build BenderAI 24/02/2026 Deploy macOS build to runtime directory
# Copies GeneralsXZH binary and required dylibs to ~/GeneralsX/GeneralsZH (legacy fallback: ~/GeneralsX/GeneralsMD)

set -e

# GeneralsX @bugfix BenderAI 09/03/2026 Resolve repository root correctly from scripts/build/macos.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build/macos-vulkan"
SDL3_LIB_DIR="${BUILD_DIR}/_deps/sdl3-build"
SDL3_IMAGE_LIB_DIR="${BUILD_DIR}/_deps/sdl3_image-build"
FONTCONFIG_ETC_DIR="${BUILD_DIR}/vcpkg_installed/arm64-osx/etc/fonts"
GAMESPY_LIB="${BUILD_DIR}/libgamespy.dylib"
# GeneralsX @bugfix BenderAI 09/03/2026 Resolve DXVK dylib paths from both install copy and Meson output to avoid stale runtime libs.
DXVK_D3D8_LIB_INSTALL="${BUILD_DIR}/libdxvk_d3d8.0.dylib"
DXVK_D3D9_LIB_INSTALL="${BUILD_DIR}/libdxvk_d3d9.0.dylib"
DXVK_D3D8_LIB_MESON="${BUILD_DIR}/_deps/dxvk-build-macos/src/d3d8/libdxvk_d3d8.0.dylib"
DXVK_D3D9_LIB_MESON="${BUILD_DIR}/_deps/dxvk-build-macos/src/d3d9/libdxvk_d3d9.0.dylib"
# GeneralsX @bugfix BenderAI 01/04/2026 Align deploy runtime selection with launcher logic by preferring the directory that contains .big assets.
PREFERRED_RUNTIME_DIR="${HOME}/GeneralsX/GeneralsZH"
LEGACY_RUNTIME_DIR="${HOME}/GeneralsX/GeneralsMD"
LOCAL_BUILD_RUNTIME_DIR="${PROJECT_ROOT}/build/macos-runtime/GeneralsZH"
LOCAL_RUNTIME_DIR="${PROJECT_ROOT}/.local/GeneralsZH"
RUNTIME_DIR="${PREFERRED_RUNTIME_DIR}"

if [[ -n "${GX_RUNTIME_DIR:-}" ]]; then
    RUNTIME_DIR="${GX_RUNTIME_DIR}"
    echo "INFO: Using runtime directory from GX_RUNTIME_DIR=${GX_RUNTIME_DIR}"
elif [[ -d "${LOCAL_BUILD_RUNTIME_DIR}" && -n "$(compgen -G "${LOCAL_BUILD_RUNTIME_DIR}/*.big" 2>/dev/null)" ]]; then
    RUNTIME_DIR="${LOCAL_BUILD_RUNTIME_DIR}"
    echo "INFO: Detected Zero Hour assets in local build runtime ${LOCAL_BUILD_RUNTIME_DIR}; deploying there"
elif [[ -d "${LOCAL_RUNTIME_DIR}" && -n "$(compgen -G "${LOCAL_RUNTIME_DIR}/*.big" 2>/dev/null)" ]]; then
    RUNTIME_DIR="${LOCAL_RUNTIME_DIR}"
    echo "INFO: Detected Zero Hour assets in local runtime ${LOCAL_RUNTIME_DIR}; deploying there"
elif [[ -d "${PREFERRED_RUNTIME_DIR}" && -n "$(compgen -G "${PREFERRED_RUNTIME_DIR}/*.big" 2>/dev/null)" ]]; then
    RUNTIME_DIR="${PREFERRED_RUNTIME_DIR}"
    echo "INFO: Detected Zero Hour assets in ${PREFERRED_RUNTIME_DIR}; deploying there"
elif [[ -d "${LEGACY_RUNTIME_DIR}" && -n "$(compgen -G "${LEGACY_RUNTIME_DIR}/*.big" 2>/dev/null)" ]]; then
    RUNTIME_DIR="${LEGACY_RUNTIME_DIR}"
    echo "INFO: Detected Zero Hour assets in legacy runtime ${LEGACY_RUNTIME_DIR}; deploying there"
elif [[ -d "${PREFERRED_RUNTIME_DIR}" ]]; then
    RUNTIME_DIR="${PREFERRED_RUNTIME_DIR}"
elif [[ -d "${LEGACY_RUNTIME_DIR}" ]]; then
    RUNTIME_DIR="${LEGACY_RUNTIME_DIR}"
    echo "INFO: No .big assets found; using existing legacy runtime ${LEGACY_RUNTIME_DIR}"
fi

# Locate the installed Vulkan SDK: explicit $VULKAN_SDK / $VULKAN_SDK_ROOT
# first (issue #1), then the conventional ~/VulkanSDK/<version>/macOS glob.
_VULKAN_SDK_ENV="${VULKAN_SDK:-}"; _VULKAN_SDK_ROOT_ENV="${VULKAN_SDK_ROOT:-}"
VULKAN_SDK_ROOT=""
for sdk_candidate in "${_VULKAN_SDK_ENV}" "${_VULKAN_SDK_ROOT_ENV}" "${PROJECT_ROOT}/.local/vulkan-sdk/macOS" "${PROJECT_ROOT}/.local/vulkan-sdk" "${HOME}/VulkanSDK"/*/macOS; do
    [[ -n "${sdk_candidate}" ]] || continue
    if [[ -f "${sdk_candidate}/lib/libvulkan.dylib" ]]; then
        VULKAN_SDK_ROOT="${sdk_candidate}"
        break
    elif [[ -f "${sdk_candidate}/macOS/lib/libvulkan.dylib" ]]; then
        VULKAN_SDK_ROOT="${sdk_candidate}/macOS"
        break
    fi
done
BINARY_SRC="${BUILD_DIR}/GeneralsMD/GeneralsXZH"

DXVK_D3D8_LIB="${DXVK_D3D8_LIB_INSTALL}"
DXVK_D3D9_LIB="${DXVK_D3D9_LIB_INSTALL}"
if [[ ! -f "${DXVK_D3D8_LIB}" && -f "${DXVK_D3D8_LIB_MESON}" ]]; then
    DXVK_D3D8_LIB="${DXVK_D3D8_LIB_MESON}"
fi
if [[ ! -f "${DXVK_D3D9_LIB}" && -f "${DXVK_D3D9_LIB_MESON}" ]]; then
    DXVK_D3D9_LIB="${DXVK_D3D9_LIB_MESON}"
fi

echo "Deploying GeneralsXZH (macOS) to ${RUNTIME_DIR}"

if [[ ! -f "${BINARY_SRC}" ]]; then
    echo "ERROR: Binary not found at ${BINARY_SRC}"
    echo "Build first: cmake --build build/macos-vulkan --target z_generals"
    exit 1
fi
if [[ ! -s "${BINARY_SRC}" ]]; then
    echo "ERROR: Binary at ${BINARY_SRC} is empty - build may have failed"
    exit 1
fi

mkdir -p "${RUNTIME_DIR}"

echo "  Copying GeneralsXZH..."
cp -v "${BINARY_SRC}" "${RUNTIME_DIR}/GeneralsXZH"
chmod +x "${RUNTIME_DIR}/GeneralsXZH"

echo "  Copying SDL3 libraries..."
cp -v "${SDL3_LIB_DIR}"/libSDL3.0.dylib "${RUNTIME_DIR}/"
ln -sf libSDL3.0.dylib "${RUNTIME_DIR}/libSDL3.dylib" 2>/dev/null || true
cp -v "${SDL3_IMAGE_LIB_DIR}"/libSDL3_image.0.4.0.dylib "${RUNTIME_DIR}/"
ln -sf libSDL3_image.0.4.0.dylib "${RUNTIME_DIR}/libSDL3_image.0.dylib" 2>/dev/null || true
ln -sf libSDL3_image.0.4.0.dylib "${RUNTIME_DIR}/libSDL3_image.dylib" 2>/dev/null || true

echo "  Copying GameSpy library..."
cp -v "${GAMESPY_LIB}" "${RUNTIME_DIR}/"

echo "  Copying DXVK libraries (d3d9 + d3d8)..."
# d3d8 links against d3d9 via @rpath — both must be present in the runtime dir
if [[ ! -f "${DXVK_D3D9_LIB}" || ! -f "${DXVK_D3D8_LIB}" ]]; then
    echo "ERROR: Required DXVK dylibs were not found in expected locations:"
    echo "  d3d9 install: ${DXVK_D3D9_LIB_INSTALL}"
    echo "  d3d8 install: ${DXVK_D3D8_LIB_INSTALL}"
    echo "  d3d9 meson:   ${DXVK_D3D9_LIB_MESON}"
    echo "  d3d8 meson:   ${DXVK_D3D8_LIB_MESON}"
    echo "Build DXVK first: cmake --build build/macos-vulkan --target dxvk_d3d8_install"
    exit 1
fi
cp -v "${DXVK_D3D9_LIB}" "${RUNTIME_DIR}/libdxvk_d3d9.0.dylib"
ln -sf libdxvk_d3d9.0.dylib "${RUNTIME_DIR}/libdxvk_d3d9.dylib" 2>/dev/null || true
cp -v "${DXVK_D3D8_LIB}" "${RUNTIME_DIR}/libdxvk_d3d8.0.dylib"
ln -sf libdxvk_d3d8.0.dylib "${RUNTIME_DIR}/libdxvk_d3d8.dylib" 2>/dev/null || true

echo "  Deploying Vulkan + MoltenVK libraries..."
if [[ -n "${VULKAN_SDK_ROOT}" ]]; then
    # Copy libvulkan loader (DXVK dlopen's "libvulkan.dylib" on macOS)
    cp -v "${VULKAN_SDK_ROOT}/lib/libvulkan.dylib" "${RUNTIME_DIR}/"
    cp -v "${VULKAN_SDK_ROOT}/lib/libvulkan.1.dylib" "${RUNTIME_DIR}/" 2>/dev/null || true
    # Copy MoltenVK ICD driver (provides vkGetInstanceProcAddr via libvulkan.dylib)
    cp -v "${VULKAN_SDK_ROOT}/lib/libMoltenVK.dylib" "${RUNTIME_DIR}/"
    # Write MoltenVK ICD manifest (Vulkan loader needs VK_ICD_FILENAMES to point here)
    cat > "${RUNTIME_DIR}/MoltenVK_icd.json" <<'EOF'
{
    "file_format_version": "1.0.0",
    "ICD": {
        "library_path": "./libMoltenVK.dylib",
        "api_version": "1.4.0",
        "is_portability_driver": true
    }
}
EOF
    echo "  Vulkan SDK libs deployed from: ${VULKAN_SDK_ROOT}"
else
    echo "WARNING: Vulkan SDK not found (checked \$VULKAN_SDK, \$VULKAN_SDK_ROOT, ~/VulkanSDK/*/macOS)."
    echo "  Install the Vulkan SDK from https://vulkan.lunarg.com/"
    echo "  DXVK will fail to find vkGetInstanceProcAddr at runtime."
fi

# Write wrapper run script that sets DYLD_LIBRARY_PATH at launch time
echo "  Deploying dxvk.conf..."
# GeneralsX @bugfix BenderAI 13/03/2026 Make DXVK config deployment explicit and fail fast.
# Missing dxvk.conf silently caused terrain shader debugging to be misleading on macOS.
DXVK_CONF_SRC="${PROJECT_ROOT}/resources/dxvk/dxvk.conf"
DXVK_CONF_LEGACY_SRC="${PROJECT_ROOT}/GeneralsMD/Run/dxvk.conf"
if [[ -f "${DXVK_CONF_SRC}" ]]; then
    cp -v "${DXVK_CONF_SRC}" "${RUNTIME_DIR}/dxvk.conf"
elif [[ -f "${DXVK_CONF_LEGACY_SRC}" ]]; then
    echo "WARNING: Using legacy DXVK config path: ${DXVK_CONF_LEGACY_SRC}"
    cp -v "${DXVK_CONF_LEGACY_SRC}" "${RUNTIME_DIR}/dxvk.conf"
else
    echo "ERROR: ${DXVK_CONF_SRC} not found."
    echo "       Refusing deploy because DXVK runtime config is required for macOS terrain investigation."
    exit 1
fi

# SagePatch (optional, gated by RTS_BUILD_OPTION_SAGE_PATCH at configure time).
# When the dylib exists, deploy it. The engine auto-creates SagePatch.ini with
# defaults in the user data directory on first run.
SAGE_PATCH_LIB="${BUILD_DIR}/Patches/SagePatch/libsage_patch.dylib"
if [[ -f "${SAGE_PATCH_LIB}" ]]; then
    echo "  Deploying SagePatch (libsage_patch.dylib)..."
    cp -v "${SAGE_PATCH_LIB}" "${RUNTIME_DIR}/"
fi

# GeneralsX @build BenderAI 08/06/2026 Deploy ExtrasMenu.wnd so the engine can
# load it via TheFileSystem (local files override BIG archives).
EXTRAS_WND_SRC="${PROJECT_ROOT}/GeneralsZH/Data/Window/Menus/ExtrasMenu.wnd"
if [[ -f "${EXTRAS_WND_SRC}" ]]; then
    mkdir -p "${RUNTIME_DIR}/Window/Menus"
    cp -v "${EXTRAS_WND_SRC}" "${RUNTIME_DIR}/Window/Menus/ExtrasMenu.wnd"
fi

# GeneralsX @bugfix Copilot 24/03/2026 Deploy Fontconfig config into runtime dir so FreeType/Fontconfig can resolve fonts on macOS.
# GeneralsX @bugfix BenderAI 24/03/2026 Guard Fontconfig conf.d copy so missing directory does not abort deploy under set -e.
echo "  Deploying Fontconfig config..."
if [[ -f "${FONTCONFIG_ETC_DIR}/fonts.conf" ]]; then
    mkdir -p "${RUNTIME_DIR}/fontconfig"
    cp -v "${FONTCONFIG_ETC_DIR}/fonts.conf" "${RUNTIME_DIR}/fontconfig/fonts.conf"
    rm -rf "${RUNTIME_DIR}/fontconfig/conf.d"
    if [[ -d "${FONTCONFIG_ETC_DIR}/conf.d" ]]; then
        cp -R "${FONTCONFIG_ETC_DIR}/conf.d" "${RUNTIME_DIR}/fontconfig/conf.d"
    else
        echo "WARNING: Fontconfig conf.d directory not found at ${FONTCONFIG_ETC_DIR}/conf.d."
        echo "  Runtime may fail to resolve some fonts if per-font configs are missing."
    fi
else
    echo "WARNING: Fontconfig config not found at ${FONTCONFIG_ETC_DIR}."
    echo "  Runtime may fail to resolve fonts in Save/Load/Replay menus."
fi

echo "  Writing run.sh wrapper..."
cat > "${RUNTIME_DIR}/run.sh" << WRAPPER
#!/bin/bash
# GeneralsX @build BenderAI 24/02/2026 - macOS wrapper for runtime directory
SCRIPT_DIR="\$(cd "\$(dirname "\$0")" && pwd)"

# SDL3 and gamespy dylibs are in same dir; Vulkan/MoltenVK stays in SDK
export DYLD_LIBRARY_PATH="\${SCRIPT_DIR}:\${DYLD_LIBRARY_PATH:-}"

# SagePatch (optional QoL features). Loaded via DYLD_INSERT_LIBRARIES so it
# can interpose SDL3 functions for hot-keys (F11 screenshot, Scroll Lock cursor
# lock, Ctrl+PageUp/PageDown brightness, Ctrl+1..5 window snap).
if [[ -f "\${SCRIPT_DIR}/libsage_patch.dylib" && "\${SAGE_PATCH_DISABLED:-0}" != "1" ]]; then
    if [[ -n "\${DYLD_INSERT_LIBRARIES:-}" ]]; then
        export DYLD_INSERT_LIBRARIES="\${SCRIPT_DIR}/libsage_patch.dylib:\${DYLD_INSERT_LIBRARIES}"
    else
        export DYLD_INSERT_LIBRARIES="\${SCRIPT_DIR}/libsage_patch.dylib"
    fi
fi

# GeneralsX @bugfix fbraz3 20/03/2026 DXVK requires DXVK_WSI_DRIVER on non-Win32; must match game windowing (SDL3)
export DXVK_WSI_DRIVER="SDL3"

# DXVK HUD: kept opt-in. MoltenVK on macOS 26 cannot compile DXVK's HUD
# pipeline shader (uses gl_DrawID / SPIR-V DrawIndex which has no MSL
# equivalent yet), so defaulting it on breaks the swap chain blit pipeline.
# Users wanting an FPS overlay set DXVK_HUD=fps themselves.
export DXVK_HUD="\${DXVK_HUD:-0}"

# MoltenVK ICD manifest — deployed alongside the binary by deploy-macos-zh.sh
if [[ -f "\${SCRIPT_DIR}/MoltenVK_icd.json" ]]; then
    export VK_ICD_FILENAMES="\${SCRIPT_DIR}/MoltenVK_icd.json"
    # GeneralsX @bugfix fbraz3 20/03/2026 Vulkan Loader 1.3.236+ uses VK_DRIVER_FILES; keep VK_ICD_FILENAMES for older loaders
    export VK_DRIVER_FILES="\${SCRIPT_DIR}/MoltenVK_icd.json"
fi

# GeneralsX @bugfix Copilot 24/03/2026 Set bundled Fontconfig config path to avoid "Cannot load default config file: (null)" on macOS.
if [[ -f "\${SCRIPT_DIR}/fontconfig/fonts.conf" ]]; then
    export FONTCONFIG_FILE="\${SCRIPT_DIR}/fontconfig/fonts.conf"
    export FONTCONFIG_PATH="\${SCRIPT_DIR}/fontconfig"
fi

# Auto-detect base Generals install path
if [[ -z "\${CNC_GENERALS_INSTALLPATH:-}" && -d "\${SCRIPT_DIR}/../Generals" ]]; then
    export CNC_GENERALS_INSTALLPATH="\${SCRIPT_DIR}/../Generals/"
fi

# The engine resolves Local FS lookups (e.g. INI overrides under
# Data/INI/Default/...) relative to the binary's cwd. Without this cd, anything
# launched via absolute path (Finder, gtimeout, full-path invocation) misses
# every loose INI / asset and only sees what is bundled inside the BIG files.
cd "\${SCRIPT_DIR}"

exec "./GeneralsXZH" "\$@"
WRAPPER
chmod +x "${RUNTIME_DIR}/run.sh"

echo ""
echo "Deploy complete"
echo "   Executable: ${RUNTIME_DIR}/GeneralsXZH"
echo "   SDL3 libs:  ${RUNTIME_DIR}/libSDL3*.dylib"
echo "   GameSpy:    ${RUNTIME_DIR}/libgamespy.dylib"
echo "   DXVK d3d9:  ${RUNTIME_DIR}/libdxvk_d3d9.0.dylib"
echo "   DXVK d3d8:  ${RUNTIME_DIR}/libdxvk_d3d8.0.dylib"
echo "   Vulkan:     ${RUNTIME_DIR}/libvulkan.dylib"
echo "   MoltenVK:   ${RUNTIME_DIR}/libMoltenVK.dylib"
echo "   VK ICD:     ${RUNTIME_DIR}/MoltenVK_icd.json"
echo "   DXVK conf:  ${RUNTIME_DIR}/dxvk.conf"
# GeneralsX @bugfix BenderAI 24/03/2026 Show Fontconfig status only when deployed to avoid misleading summary output.
if [[ -f "${RUNTIME_DIR}/fontconfig/fonts.conf" ]]; then
    echo "   Fontconfig: ${RUNTIME_DIR}/fontconfig/fonts.conf"
else
    echo "   Fontconfig: (not deployed)"
fi
echo "   Wrapper:    ${RUNTIME_DIR}/run.sh"
echo ""
echo "Run with:"
echo "  ${PROJECT_ROOT}/scripts/build/macos/run-macos-zh.sh -win"
echo "  or: cd ~/GeneralsX/GeneralsZH && ./run.sh -win"
