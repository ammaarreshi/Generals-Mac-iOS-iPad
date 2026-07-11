#!/bin/bash
# GeneralsX @build felipebraz 16/02/2026 Deploy Linux build to runtime directory
# GeneralsX @bugfix BenderAI 19/02/2026 Add non-empty check to guard against stale CMake placeholder files

set -e

# Directories
# GeneralsX @bugfix BenderAI 09/03/2026 Resolve repository root correctly from scripts/build/linux.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build/linux64-deploy"
DXVK_LIB_DIR="${BUILD_DIR}/_deps/dxvk-src/lib"
SDL3_LIB_DIR="${BUILD_DIR}/_deps/sdl3-build"
SDL3_IMAGE_LIB_DIR="${BUILD_DIR}/_deps/sdl3_image-build"
GAMESPY_LIB="${BUILD_DIR}/libgamespy.so"
FFMPEG_LIB_DIR="/usr/lib/x86_64-linux-gnu"
FFMPEG_DEP_LIB_DIR="/lib/x86_64-linux-gnu"
# GeneralsX @bugfix BenderAI 01/04/2026 Align deploy runtime selection with launcher logic by preferring the directory that contains .big assets.
PREFERRED_RUNTIME_DIR="${HOME}/GeneralsX/GeneralsZH"
LEGACY_RUNTIME_DIR="${HOME}/GeneralsX/GeneralsMD"
RUNTIME_DIR="${PREFERRED_RUNTIME_DIR}"

if [[ -d "${PREFERRED_RUNTIME_DIR}" && -n "$(compgen -G "${PREFERRED_RUNTIME_DIR}/*.big" 2>/dev/null)" ]]; then
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
# Note: CMakeLists.txt uses OUTPUT_NAME GeneralsXZH on Linux (see GeneralsMD/Code/Main/CMakeLists.txt)
BINARY_SRC="${BUILD_DIR}/GeneralsMD/GeneralsXZH"

echo "Deploying GeneralsXZH (Linux) to ${RUNTIME_DIR}"

# Check if binary exists and is non-zero (CMake creates a 0-byte placeholder before the link step)
if [[ ! -f "${BINARY_SRC}" ]]; then
    echo "ERROR: Binary not found at ${BINARY_SRC}"
    echo "Build first: ./scripts/build/linux/docker-build-linux-zh.sh linux64-deploy"
    exit 1
fi
if [[ ! -s "${BINARY_SRC}" ]]; then
    echo "ERROR: Binary at ${BINARY_SRC} is empty (0 bytes) - build may have failed"
    echo "Check build logs: ./scripts/build/linux/docker-build-linux-zh.sh linux64-deploy"
    exit 1
fi

# Check if DXVK libraries exist
if [[ ! -d "${DXVK_LIB_DIR}" ]]; then
    echo "ERROR: DXVK libraries not found at ${DXVK_LIB_DIR}"
    echo "Configure first: ./scripts/build/linux/docker-configure-linux.sh linux64-deploy"
    exit 1
fi

# Check if SDL3 libraries exist
if [[ ! -d "${SDL3_LIB_DIR}" ]]; then
    echo "ERROR: SDL3 libraries not found at ${SDL3_LIB_DIR}"
    echo "Build first: ./scripts/build/linux/docker-build-linux-zh.sh linux64-deploy"
    exit 1
fi

if [[ ! -d "${SDL3_IMAGE_LIB_DIR}" ]]; then
    echo "ERROR: SDL3_image libraries not found at ${SDL3_IMAGE_LIB_DIR}"
    echo "Build first: ./scripts/build/linux/docker-build-linux-zh.sh linux64-deploy"
    exit 1
fi

# Check if GameSpy library exists
if [[ ! -f "${GAMESPY_LIB}" ]]; then
    echo "ERROR: GameSpy library not found at ${GAMESPY_LIB}"
    echo "Build first: ./scripts/build/linux/docker-build-linux-zh.sh linux64-deploy"
    exit 1
fi

# Create runtime directory if needed
mkdir -p "${RUNTIME_DIR}"

# Copy executable (deploy as GeneralsXZH for run script compatibility)
echo "  Copying GeneralsXZH..."
cp -v "${BINARY_SRC}" "${RUNTIME_DIR}/GeneralsXZH"
chmod +x "${RUNTIME_DIR}/GeneralsXZH"

# Copy DXVK libraries
echo "  Copying DXVK libraries..."
cp -v "${DXVK_LIB_DIR}"/libdxvk_d3d8.so* "${RUNTIME_DIR}/"
cp -v "${DXVK_LIB_DIR}"/libdxvk_d3d9.so* "${RUNTIME_DIR}/" 2>/dev/null || true

# Copy SDL3 and SDL3_image libraries (for cursor loading and window management)
echo "  Copying SDL3 libraries..."
cp -v "${SDL3_LIB_DIR}"/libSDL3.so* "${RUNTIME_DIR}/"
cp -v "${SDL3_IMAGE_LIB_DIR}"/libSDL3_image.so* "${RUNTIME_DIR}/"

# Copy GameSpy library (for online multiplayer)
echo "  Copying GameSpy library..."
cp -v "${GAMESPY_LIB}" "${RUNTIME_DIR}/"

copy_ldd_deps() {
    local root="$1"
    [[ -e "${root}" ]] || return 0

    while IFS= read -r dep; do
        case "${dep}" in
            linux-vdso.so.1 | \
            /lib64/ld-linux* | /lib/*/ld-linux* | /usr/lib/*/ld-linux* | /usr/lib64/ld-linux* | \
            /lib/*/libc.so.* | /lib64/libc.so.* | /usr/lib/*/libc.so.* | /usr/lib64/libc.so.* | \
            /lib/*/libm.so.* | /lib64/libm.so.* | /usr/lib/*/libm.so.* | /usr/lib64/libm.so.* | \
            /lib/*/libpthread.so.* | /lib64/libpthread.so.* | /usr/lib/*/libpthread.so.* | /usr/lib64/libpthread.so.* | \
            /lib/*/librt.so.* | /lib64/librt.so.* | /usr/lib/*/librt.so.* | /usr/lib64/librt.so.* | \
            /lib/*/libdl.so.* | /lib64/libdl.so.* | /usr/lib/*/libdl.so.* | /usr/lib64/libdl.so.*)
                continue
                ;;
        esac

        cp -a "${dep}" "${RUNTIME_DIR}/" 2>/dev/null || true
        if [[ -L "${dep}" ]]; then
            local resolved
            resolved="$(readlink -f "${dep}")"
            cp -a "${resolved}" "${RUNTIME_DIR}/" 2>/dev/null || true
        fi
    done < <(ldd "${root}" | awk '{for (i = 1; i <= NF; ++i) { if ($i ~ /^\//) { print $i; break } }}' | sort -u)
}

# GeneralsX @build GitHubCopilot 17/05/2026 Deploy FFmpeg runtime libs transitively so runtime does not depend on host SONAME layout.
echo "  Copying FFmpeg runtime libraries..."
shopt -s nullglob
ffmpeg_roots=(
    "${FFMPEG_LIB_DIR}"/libavcodec.so*
    "${FFMPEG_LIB_DIR}"/libavformat.so*
    "${FFMPEG_LIB_DIR}"/libavutil.so*
    "${FFMPEG_LIB_DIR}"/libswresample.so*
    "${FFMPEG_LIB_DIR}"/libswscale.so*
    "${FFMPEG_DEP_LIB_DIR}"/libavcodec.so*
    "${FFMPEG_DEP_LIB_DIR}"/libavformat.so*
    "${FFMPEG_DEP_LIB_DIR}"/libavutil.so*
    "${FFMPEG_DEP_LIB_DIR}"/libswresample.so*
    "${FFMPEG_DEP_LIB_DIR}"/libswscale.so*
)
shopt -u nullglob
for ffmpeg_root in "${ffmpeg_roots[@]}"; do
    cp -a "${ffmpeg_root}" "${RUNTIME_DIR}/" 2>/dev/null || true
    copy_ldd_deps "${ffmpeg_root}"
done

if ! compgen -G "${RUNTIME_DIR}/libavcodec.so*" > /dev/null; then
    echo "ERROR: Missing required runtime library: libavcodec.so*"
    echo "Install FFmpeg runtime/dev packages (e.g. libavcodec-dev) and rebuild/deploy"
    exit 1
fi

# Set RPATH so executable finds libraries in same directory
echo "  Setting RPATH to \$ORIGIN..."
patchelf --set-rpath '$ORIGIN' "${RUNTIME_DIR}/GeneralsXZH" 2>/dev/null || {
    echo "WARNING: patchelf not found. Install with: sudo apt install patchelf"
    echo "    Libraries will need LD_LIBRARY_PATH or manual RPATH setting"
}

# SagePatch (optional, gated by RTS_BUILD_OPTION_SAGE_PATCH at configure time).
# When the .so exists, deploy it. The engine auto-creates SagePatch.ini with
# defaults in the user data directory on first run.
SAGE_PATCH_LIB="${BUILD_DIR}/Patches/SagePatch/libsage_patch.so"
if [[ -f "${SAGE_PATCH_LIB}" ]]; then
    echo "  Deploying SagePatch (libsage_patch.so)..."
    cp -v "${SAGE_PATCH_LIB}" "${RUNTIME_DIR}/"
fi

# GeneralsX @build BenderAI 08/06/2026 Deploy ExtrasMenu.wnd so the engine can
# load it via TheFileSystem (local files override BIG archives).
EXTRAS_WND_SRC="${PROJECT_ROOT}/GeneralsZH/Data/Window/Menus/ExtrasMenu.wnd"
if [[ -f "${EXTRAS_WND_SRC}" ]]; then
    mkdir -p "${RUNTIME_DIR}/Window/Menus"
    cp -v "${EXTRAS_WND_SRC}" "${RUNTIME_DIR}/Window/Menus/ExtrasMenu.wnd"
fi

# Copy run wrapper script
echo "  Copying run.sh wrapper..."
cat > "${RUNTIME_DIR}/run.sh" << 'EOF'
#!/bin/bash
# GeneralsX @build felipebraz 16/02/2026 - Wrapper script for runtime directory
# Sets LD_LIBRARY_PATH to find DXVK libraries

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Set LD_LIBRARY_PATH to current directory (where DXVK libs are)
export LD_LIBRARY_PATH="${SCRIPT_DIR}:${LD_LIBRARY_PATH:-}"

# Set DXVK environment
export DXVK_WSI_DRIVER="SDL3"
export DXVK_LOG_LEVEL="${DXVK_LOG_LEVEL:-info}"
# DXVK HUD is disabled by default because Generals has a native FPS counter.
# Set DXVK_HUD=fps,memory,version or similar to customize. See DXVK docs for full list.
export DXVK_HUD="${DXVK_HUD:-0}"

# SagePatch (optional QoL features). Loaded via LD_PRELOAD so it can interpose
# SDL3 functions for hot-keys (F11 screenshot, Scroll Lock cursor lock,
# Ctrl+PageUp/PageDown brightness, Ctrl+1..5 window snap).
if [[ -f "${SCRIPT_DIR}/libsage_patch.so" && "${SAGE_PATCH_DISABLED:-0}" != "1" ]]; then
    if [[ -n "${LD_PRELOAD:-}" ]]; then
        export LD_PRELOAD="${SCRIPT_DIR}/libsage_patch.so:${LD_PRELOAD}"
    else
        export LD_PRELOAD="${SCRIPT_DIR}/libsage_patch.so"
    fi
fi

# GeneralsX @feature felipebraz 25/02/2026 Auto-detect base Generals install path
# Set CNC_GENERALS_INSTALLPATH if not already set and ../Generals/ exists
if [[ -z "${CNC_GENERALS_INSTALLPATH:-}" && -d "${SCRIPT_DIR}/../Generals" ]]; then
    export CNC_GENERALS_INSTALLPATH="${SCRIPT_DIR}/../Generals/"
fi

# GeneralsX @bugfix BenderAI 06/03/2026 - Exclude LLVMpipe Vulkan ICD (LLVM 20.x crash workaround)
# libvulkan_lvp.so (LLVMpipe) crashes during static initialization with LLVM 20.x.
# Filter hardware-only ICDs via VK_DRIVER_FILES to prevent loading the crashing library.
# User can override by setting VK_DRIVER_FILES or VK_ICD_FILENAMES before running.
if [[ -z "${VK_DRIVER_FILES:-}" && -z "${VK_ICD_FILENAMES:-}" ]]; then
    _hw_icds=""
    for _dir in /usr/share/vulkan/icd.d /etc/vulkan/icd.d; do
        [[ -d "$_dir" ]] || continue
        for _f in "$_dir"/*.json; do
            [[ -f "$_f" ]] || continue
            _base="$(basename "$_f")"
            case "${_base,,}" in
                *lvp* | *lavapipe* | *softpipe* | *llvmpipe*)
                    echo "INFO: Vulkan ICD filter: skipping software ICD '$_base'" ;;
                *)
                    _hw_icds="${_hw_icds:+${_hw_icds}:}$_f" ;;
            esac
        done
    done
    if [[ -n "$_hw_icds" ]]; then
        export VK_DRIVER_FILES="$_hw_icds"
        echo "INFO: Vulkan ICD filter: VK_DRIVER_FILES=$VK_DRIVER_FILES"
    else
        echo "WARNING: Vulkan ICD filter: no hardware ICDs found, LLVMpipe exclusion skipped"
        echo "WARNING: If startup crashes, set VK_DRIVER_FILES to your hardware Vulkan ICD JSON"
    fi
fi

# GeneralsX @bugfix 09/03/2026 - Work around openal-soft 1.25.1 movaps alignment crash
# alcOpenDevice() crashes with SIGSEGV in a 'movaps %xmm1, 0x26260(%rbx)' instruction
# inside openal-soft's device initializer. movaps requires 16-byte alignment; if the
# ALCdevice struct is not aligned correctly, it faults regardless of which backend is
# selected. Disabling CPU extensions forces openal-soft to use scalar code paths that
# do not have alignment requirements. The pipewire backend is also excluded because it
# has its own crash at device-open time on PipeWire 1.4.x.
# These env vars are read by openal-soft's static constructor at library load time,
# so they must be set here in the launcher before the binary starts.
# User can override by setting ALSOFT_DISABLE_CPU_EXTS or ALSOFT_DRIVERS explicitly.
if [[ -z "${ALSOFT_DISABLE_CPU_EXTS:-}" ]]; then
    export ALSOFT_DISABLE_CPU_EXTS="all"
    echo "INFO: OpenAL: ALSOFT_DISABLE_CPU_EXTS=all (movaps alignment crash workaround)"
fi
if [[ -z "${ALSOFT_DRIVERS:-}" ]]; then
    export ALSOFT_DRIVERS="pulse,alsa,oss,jack,null,wave"
    echo "INFO: OpenAL: ALSOFT_DRIVERS=$ALSOFT_DRIVERS (pipewire excluded)"
fi

# The engine resolves Local FS lookups (Data/INI/Default/... overrides, etc.)
# relative to the binary's cwd. Without this cd, anything launched via absolute
# path misses every loose INI / asset and only sees BIG-archived data.
cd "${SCRIPT_DIR}"

# Run game with all arguments
exec "./GeneralsXZH" "$@"
EOF
chmod +x "${RUNTIME_DIR}/run.sh"

echo ""
echo "Deploy complete"
echo "   Executable: ${RUNTIME_DIR}/GeneralsXZH"
echo "   SDL3 libs:  ${RUNTIME_DIR}/libSDL3*.so* + ${RUNTIME_DIR}/libSDL3_image*.so*"
echo "   GameSpy:    ${RUNTIME_DIR}/libgamespy.so"
echo "   FFmpeg:     ${RUNTIME_DIR}/libavcodec*.so* + libavformat*.so* + libavutil*.so*"
# GeneralsX @tweak BenderAI 28/04/2026 Mirror macOS deploy summary labels/order for cross-platform script UX consistency.
echo "   DXVK d3d9:  ${RUNTIME_DIR}/libdxvk_d3d9.so*"
echo "   DXVK d3d8:  ${RUNTIME_DIR}/libdxvk_d3d8.so*"
echo "   Vulkan:     system loader (not deployed by this script)"
echo "   MoltenVK:   n/a on Linux"
echo "   VK ICD:     system JSONs (/usr/share/vulkan/icd.d or /etc/vulkan/icd.d)"
echo "   DXVK conf:  not deployed (DXVK defaults/environment)"
echo "   Wrapper:    ${RUNTIME_DIR}/run.sh"
echo ""
# GeneralsX @tweak BenderAI 28/04/2026 Keep Linux deploy run instructions aligned across Generals and Zero Hour scripts.
echo "Run with:"
echo "  ${PROJECT_ROOT}/scripts/build/linux/run-linux-zh.sh -win"
echo "  or: cd ~/GeneralsX/GeneralsZH && ./run.sh -win"
