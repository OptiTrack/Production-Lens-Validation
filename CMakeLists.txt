# ---------------------------------------------------------------------------
# Production-Lens-Validation - top level build
#
#   Unit tests only (no Qt / OpenCV / Camera SDK needed):
#       cmake -B build -DBUILD_APP=OFF
#       cmake --build build --config Release
#       ctest --test-dir build -C Release --output-on-failure
#
#   Full application, Windows (see CameraViewerApp/WindowsBuildInstructions.txt):
#       cmake -B build -A x64 -DCAMERA_SDK_PATH=CameraSDK
#       cmake --build build --config Release
#
#   Full application, Ubuntu (see CameraViewerApp/UbuntuBuildInstructions.txt):
#       cmake -B build -DCMAKE_BUILD_TYPE=Release \
#             -DCAMERA_SDK_PATH=OptiTrack_Camera_SDK_3.4.1_Final_Ubuntu
#       cmake --build build --parallel
#
# CameraViewerApp/CMakeLists.txt still works as its own top level project, so
# CameraViewerApp/winBuild.bat and CameraViewerApp/build.sh keep working
# unchanged.
# ---------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.16)

project(ProductionLensValidation
    VERSION 0.1.0
    DESCRIPTION "OptiTrack lens focus validation tooling"
    LANGUAGES CXX
)

option(BUILD_APP "Build the CameraViewerApp GUI application (needs Qt6, OpenCV and the Camera SDK)" ON)
option(BUILD_TESTS "Build the unit test suite" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Collect binaries in one predictable place: build/bin (build/bin/<Config> for
# multi-config generators such as Visual Studio). CI packages from here.
if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

# Platform-independent scoring logic: always built, on every platform.
add_subdirectory(CameraViewerApp/core)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_APP)
    add_subdirectory(CameraViewerApp)
endif()

message(STATUS "ProductionLensValidation: BUILD_APP=${BUILD_APP} BUILD_TESTS=${BUILD_TESTS}")
