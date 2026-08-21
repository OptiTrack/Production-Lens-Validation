# ---------------------------------------------------------------------------
# Reads the project version from the VERSION file at the repository root and
# exposes it to the build.
#
# The VERSION file is the single source of truth: the root CMakeLists.txt, the
# standalone CameraViewerApp build (winBuild.bat / build.sh), the generated
# Version.h, the Windows VERSIONINFO resource and the release workflow all read
# that one file. Bump it there and nothing else needs editing.
#
# Sets:
#   PLV_VERSION              e.g. 0.8.0
#   PLV_VERSION_MAJOR/MINOR/PATCH
#   PLV_VERSION_COMMIT       short commit hash, or "unknown"
#   PLV_VERSION_FULL         e.g. 0.8.0+a1b2c3d  (or just 0.8.0 without git)
# ---------------------------------------------------------------------------

if(DEFINED PLV_VERSION)
    return()  # already read by a parent scope
endif()

set(_plv_version_file "${CMAKE_CURRENT_LIST_DIR}/../VERSION")
if(NOT EXISTS "${_plv_version_file}")
    message(FATAL_ERROR "Version file not found: ${_plv_version_file}")
endif()

file(READ "${_plv_version_file}" _plv_version_raw)
string(STRIP "${_plv_version_raw}" PLV_VERSION)

if(NOT PLV_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
    message(FATAL_ERROR
        "VERSION must contain a MAJOR.MINOR.PATCH version, got '${PLV_VERSION}'")
endif()
set(PLV_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(PLV_VERSION_MINOR "${CMAKE_MATCH_2}")
set(PLV_VERSION_PATCH "${CMAKE_MATCH_3}")

# Build metadata: which commit this came from. Resolved at configure time, so
# re-run CMake if you want a stale hash refreshed.
set(PLV_VERSION_COMMIT "unknown")
find_package(Git QUIET)
if(Git_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
        OUTPUT_VARIABLE _plv_git_sha
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _plv_git_result
    )
    if(_plv_git_result EQUAL 0 AND _plv_git_sha)
        set(PLV_VERSION_COMMIT "${_plv_git_sha}")
    endif()
endif()

if(PLV_VERSION_COMMIT STREQUAL "unknown")
    set(PLV_VERSION_FULL "${PLV_VERSION}")
else()
    set(PLV_VERSION_FULL "${PLV_VERSION}+${PLV_VERSION_COMMIT}")
endif()

message(STATUS "Project version: ${PLV_VERSION_FULL}")
