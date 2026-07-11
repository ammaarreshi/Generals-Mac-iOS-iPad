#!/bin/bash
# GeneralsX @build BenderAI 03/03/2026 Bundle Linux GeneralsXZH binary + .so libs into a tarball archive
# Packages the same files as deploy-linux-zh.sh into GeneralsXZH-linux-x86_64.tar.gz

set -e

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
BINARY_SRC="${BUILD_DIR}/GeneralsMD/GeneralsXZH"
DXVK_CONF_SRC="${PROJECT_ROOT}/resources/dxvk/dxvk.conf"
OUTPUT_TARBALL="${PROJECT_ROOT}/GeneralsXZH-linux-x86_64.tar.gz"

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

        cp -a "${dep}" "${BUNDLE_DIR}/" 2>/dev/null || true
        if [[ -L "${dep}" ]]; then
            local resolved
            resolved="$(readlink -f "${dep}")"
            cp -a "${resolved}" "${BUNDLE_DIR}/" 2>/dev/null || true
        fi
    done < <(ldd "${root}" | awk '{for (i = 1; i <= NF; ++i) { if ($i ~ /^\//) { print $i; break } }}' | sort -u)
}

echo "Bundling GeneralsXZH (Linux x86_64)"

# Validate binary
if [[ ! -f "${BINARY_SRC}" ]]; then
    echo "ERROR: Binary not found at ${BINARY_SRC}"
    echo "Build first: ./scripts/build/linux/docker-build-linux-zh.sh linux64-deploy"
    exit 1
fi
if [[ ! -s "${BINARY_SRC}" ]]; then
    echo "ERROR: Binary at ${BINARY_SRC} is empty - build may have failed"
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

# Prepare temp staging directory
STAGE_DIR="$(mktemp -d)"
trap 'rm -rf "${STAGE_DIR}"' EXIT
BUNDLE_DIR="${STAGE_DIR}/GeneralsXZH-linux"
mkdir -p "${BUNDLE_DIR}"

echo "  Staging files to ${BUNDLE_DIR}..."

# Binary
echo "  + GeneralsXZH"
cp "${BINARY_SRC}" "${BUNDLE_DIR}/GeneralsXZH"
chmod +x "${BUNDLE_DIR}/GeneralsXZH"

# DXVK libraries
echo "  + DXVK libraries"
cp "${DXVK_LIB_DIR}"/libdxvk_d3d8.so* "${BUNDLE_DIR}/" 2>/dev/null || echo "    (WARNING: libdxvk_d3d8.so not found)"
cp "${DXVK_LIB_DIR}"/libdxvk_d3d9.so* "${BUNDLE_DIR}/" 2>/dev/null || true

# SDL3 and SDL3_image libraries
echo "  + SDL3 libraries"
cp "${SDL3_LIB_DIR}"/libSDL3.so* "${BUNDLE_DIR}/"
cp "${SDL3_IMAGE_LIB_DIR}"/libSDL3_image.so* "${BUNDLE_DIR}/"

# GameSpy library
echo "  + GameSpy library"
cp "${GAMESPY_LIB}" "${BUNDLE_DIR}/"

# SagePatch (optional, gated by RTS_BUILD_OPTION_SAGE_PATCH at configure time).
SAGE_PATCH_LIB="${BUILD_DIR}/Patches/SagePatch/libsage_patch.so"
if [[ -f "${SAGE_PATCH_LIB}" ]]; then
    echo "  + libsage_patch (SagePatch QoL)"
    cp "${SAGE_PATCH_LIB}" "${BUNDLE_DIR}/"
fi

# GeneralsX @build GitHubCopilot 17/05/2026 Copy FFmpeg runtime libs transitively so the bundle is independent of host SONAME layout.
echo "  + FFmpeg runtime libraries"
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
    cp -a "${ffmpeg_root}" "${BUNDLE_DIR}/" 2>/dev/null || true
    copy_ldd_deps "${ffmpeg_root}"
done

if ! compgen -G "${BUNDLE_DIR}/libavcodec.so*" > /dev/null; then
    echo "ERROR: Missing required bundle library libavcodec.so*"
    exit 1
fi

# DXVK config
if [[ -f "${DXVK_CONF_SRC}" ]]; then
    echo "  + dxvk.conf"
    cp "${DXVK_CONF_SRC}" "${BUNDLE_DIR}/dxvk.conf"
else
    echo "WARNING: ${DXVK_CONF_SRC} not found - terrain shaders may fail"
fi

# Run wrapper
echo "  + run.sh"
cat > "${BUNDLE_DIR}/run.sh" << 'WRAPPER'
#!/bin/bash
# GeneralsX @build BenderAI 03/03/2026 - Linux wrapper for bundled runtime
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Set LD_LIBRARY_PATH to find DXVK, SDL3, and other libs in same directory
export LD_LIBRARY_PATH="${SCRIPT_DIR}:${LD_LIBRARY_PATH:-}"

# Set DXVK environment
export DXVK_WSI_DRIVER="SDL3"
export DXVK_LOG_LEVEL="${DXVK_LOG_LEVEL:-info}"

# SagePatch (optional QoL: F11 screenshot, Scroll Lock cursor lock,
# Ctrl+PgUp/Dn brightness, Ctrl+1..5 window snap). Loaded via LD_PRELOAD only
# if libsage_patch.so is bundled. DXVK_HUD defaults to "fps" when active.
if [[ -f "${SCRIPT_DIR}/libsage_patch.so" && "${SAGE_PATCH_DISABLED:-0}" != "1" ]]; then
    if [[ -n "${LD_PRELOAD:-}" ]]; then
        export LD_PRELOAD="${SCRIPT_DIR}/libsage_patch.so:${LD_PRELOAD}"
    else
        export LD_PRELOAD="${SCRIPT_DIR}/libsage_patch.so"
    fi
fi
# DXVK HUD is disabled by default because Generals has a native FPS counter.
export DXVK_HUD="${DXVK_HUD:-0}"

# Auto-detect base Generals install path
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

exec "${SCRIPT_DIR}/GeneralsXZH" "$@"
WRAPPER
chmod +x "${BUNDLE_DIR}/run.sh"

# Create tarball
echo ""
echo "Creating ${OUTPUT_TARBALL}..."
rm -f "${OUTPUT_TARBALL}"
(cd "${STAGE_DIR}" && tar -czf "${OUTPUT_TARBALL}" GeneralsXZH-linux/)

echo ""
echo "Bundle complete: ${OUTPUT_TARBALL}"
echo "Contents:"
tar -tzf "${OUTPUT_TARBALL}" | head -30
echo ""
echo "To use: extract alongside your game data directory (GeneralsZH/)"
echo "  (legacy fallback also supported: GeneralsMD/)"
echo "  tar -xzf GeneralsXZH-linux-x86_64.tar.gz"
echo "  ./GeneralsXZH-linux/run.sh -win"
