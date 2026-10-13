# GeneralsX @build nyanlintun 13/10/2026 Add an idempotent patch step for FetchContent sources. (#29)
# Usage: cmake -DPATCH_FILE=<file> -P apply_patch_once.cmake   (run from the source dir)
# Uses `patch` rather than `git apply`: FetchContent sources live under build/, inside
# this repository, where `git apply` resolves paths against the repo root and silently
# skips them. Already-applied patches are detected and left alone; anything else fails.

if(NOT PATCH_FILE OR NOT EXISTS "${PATCH_FILE}")
  message(FATAL_ERROR "apply_patch_once: PATCH_FILE not found: '${PATCH_FILE}'")
endif()

execute_process(
  COMMAND patch -p1 -R -s -f --dry-run -i "${PATCH_FILE}"
  RESULT_VARIABLE ALREADY_APPLIED
  OUTPUT_QUIET ERROR_QUIET)
if(ALREADY_APPLIED EQUAL 0)
  message(STATUS "apply_patch_once: already applied: ${PATCH_FILE}")
  return()
endif()

execute_process(
  COMMAND patch -p1 -s -f -i "${PATCH_FILE}"
  RESULT_VARIABLE PATCH_RESULT)
if(NOT PATCH_RESULT EQUAL 0)
  message(FATAL_ERROR "apply_patch_once: failed to apply ${PATCH_FILE}")
endif()
message(STATUS "apply_patch_once: applied ${PATCH_FILE}")
