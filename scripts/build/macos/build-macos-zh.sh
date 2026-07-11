#!/usr/bin/env bash
# Build GeneralsXZH (Zero Hour) natively on macOS (Apple Silicon / arm64)
# GeneralsX @build BenderAI 24/02/2026 - Phase 5 macOS port
#
# Prerequisites (install once):
#   brew install cmake ninja meson
#   Install Vulkan SDK from https://vulkan.lunarg.com/sdk/home#mac
#   (installs to ~/VulkanSDK/<version>/macOS)
#
# Usage:
#   ./scripts/build/macos/build-macos-zh.sh               # configure + build
#   ./scripts/build/macos/build-macos-zh.sh --build-only  # skip configure if already done
#
# After building:
#   ./scripts/build/macos/deploy-macos-zh.sh  # copy to runtime dir
#   ./scripts/build/macos/run-macos-zh.sh -win # launch windowed

set -eo pipefail

PRESET="macos-vulkan"
BUILD_DIR="build/${PRESET}"
LOG_FILE="logs/build_zh_${PRESET}.log"
SKIP_CONFIGURE=0
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

for arg in "$@"; do
    case "$arg" in
        --build-only) SKIP_CONFIGURE=1 ;;
    esac
done

mkdir -p logs

echo "Building GeneralsXZH (macOS, preset: ${PRESET})..."

# ── Prerequisite checks ──────────────────────────────────────────────────────

check_tool() {
    local tool="$1" hint="$2"
    if ! command -v "$tool" &>/dev/null; then
        echo "ERROR: '$tool' not found. ${hint}"
        exit 1
    fi
}

check_tool cmake "brew install cmake"
check_tool ninja "brew install ninja"
check_tool meson "brew install meson"
check_tool python3 "brew install python3"

# GeneralsX @build Copilot 03/05/2026 Auto-detect VCPKG_ROOT for preset toolchain resolution
resolve_vcpkg_root() {
    local candidate=""
    local -a candidates=()
    local brew_vcpkg_root=""

    if [[ -n "${VCPKG_ROOT:-}" ]]; then
        if [[ -f "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" ]]; then
            echo "Using VCPKG_ROOT from environment: ${VCPKG_ROOT}"
            return 0
        fi
        echo "WARNING: VCPKG_ROOT is set but invalid: ${VCPKG_ROOT}"
    fi

    if command -v brew &>/dev/null; then
        brew_vcpkg_root="$(brew --prefix vcpkg 2>/dev/null || true)"
    fi

    candidates+=("${PROJECT_ROOT}/vcpkg")
    candidates+=("${PROJECT_ROOT}/../vcpkg")
    candidates+=("${PWD}/vcpkg")
    candidates+=("${HOME}/vcpkg")
    candidates+=("/opt/vcpkg")
    candidates+=("/opt/homebrew/opt/vcpkg")
    candidates+=("/usr/local/opt/vcpkg")
    if [[ -n "${brew_vcpkg_root}" ]]; then
        candidates+=("${brew_vcpkg_root}")
    fi

    for candidate in "${candidates[@]}"; do
        if [[ -f "${candidate}/scripts/buildsystems/vcpkg.cmake" ]]; then
            export VCPKG_ROOT="${candidate}"
            echo "Using detected VCPKG_ROOT: ${VCPKG_ROOT}"
            return 0
        fi
    done

    echo "ERROR: VCPKG_ROOT is not configured and no local vcpkg installation was detected."
    echo "Set VCPKG_ROOT to a valid vcpkg root containing scripts/buildsystems/vcpkg.cmake"
    echo "Example: export VCPKG_ROOT=\"/opt/homebrew/opt/vcpkg\""
    exit 1
}

resolve_vcpkg_root

# Vulkan SDK check — honor an explicit $VULKAN_SDK / $VULKAN_SDK_ROOT first
# (issue #1: users export it from ~/.zshrc or install outside ~/VulkanSDK),
# then fall back to the conventional ~/VulkanSDK/<version>/macOS glob.
RESOLVED_VULKAN_SDK=""
for sdk_candidate in "${VULKAN_SDK:-}" "${VULKAN_SDK_ROOT:-}" "${PROJECT_ROOT}/.local/vulkan-sdk/macOS" "${PROJECT_ROOT}/.local/vulkan-sdk" "${HOME}/VulkanSDK"/*/macOS; do
    [[ -n "${sdk_candidate}" ]] || continue
    # accept either the .../macOS dir itself or an SDK root that contains it
    if [[ -f "${sdk_candidate}/lib/libvulkan.dylib" ]]; then
        RESOLVED_VULKAN_SDK="${sdk_candidate}"
        break
    elif [[ -f "${sdk_candidate}/macOS/lib/libvulkan.dylib" ]]; then
        RESOLVED_VULKAN_SDK="${sdk_candidate}/macOS"
        break
    fi
done
if [[ -z "${RESOLVED_VULKAN_SDK}" ]]; then
    echo "ERROR: Vulkan SDK not found (checked \$VULKAN_SDK, \$VULKAN_SDK_ROOT, ~/VulkanSDK/*/macOS)"
    echo "Install from: https://vulkan.lunarg.com/sdk/home#mac"
    echo "Then: export VULKAN_SDK=\$HOME/VulkanSDK/<version>/macOS"
    exit 1
fi
echo "Vulkan SDK found: ${RESOLVED_VULKAN_SDK}"
export VULKAN_SDK="${RESOLVED_VULKAN_SDK}"
# issue #2: DXVK's Meson configure needs glslangValidator, which ships in the
# SDK's bin/ — put it on PATH so a fresh machine doesn't fail mid-configure.
export PATH="${RESOLVED_VULKAN_SDK}/bin:${PATH}"

# ── Configure ────────────────────────────────────────────────────────────────

if [[ "$SKIP_CONFIGURE" -eq 0 ]]; then
    echo "Configuring CMake (preset: ${PRESET})..."
    echo "  NOTE: First run fetches DXVK from git and builds it via Meson."
    echo "  This can take 5-10 minutes. Subsequent builds reuse the cache."
    cmake --preset "${PRESET}" 2>&1 | tee "${LOG_FILE}"
fi

# ── Build ────────────────────────────────────────────────────────────────────

JOBS=$(( ($(sysctl -n hw.logicalcpu) + 1) / 2 ))
echo "Building GeneralsXZH (${JOBS} parallel jobs)..."

cmake --build "${BUILD_DIR}" --target z_generals -j"${JOBS}" 2>&1 | tee -a "${LOG_FILE}"

# ── Result ───────────────────────────────────────────────────────────────────

BINARY="${BUILD_DIR}/GeneralsMD/GeneralsXZH"
if [[ -f "${BINARY}" ]]; then
    SIZE=$(du -sh "${BINARY}" | cut -f1)
    echo ""
    echo "Build complete."
    echo "  Binary : ${BINARY} (${SIZE})"
    echo "  Log    : ${LOG_FILE}"
    echo ""
    echo "Next steps:"
    echo "  1. Copy game files to ~/GeneralsX/GeneralsZH/ (preferred; legacy fallback: ~/GeneralsX/GeneralsMD/)"
    echo "  2. Run: ./scripts/build/macos/deploy-macos-zh.sh"
    echo "  3. Run: ./scripts/build/macos/run-macos-zh.sh -win"
else
    echo "ERROR: Binary not found at ${BINARY}"
    exit 1
fi
