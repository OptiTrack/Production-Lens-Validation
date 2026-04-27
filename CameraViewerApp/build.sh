#!/bin/bash
# Usage: ./linuxBuild.sh /path/to/camera/sdk [--ffmpeg] [--asan]

if [ -z "$1" ]; then
    echo "Usage: $0 <CAMERA_SDK_PATH> [--ffmpeg] [--asan]"
    exit 1
fi

CAMERA_SDK_PATH="$1"
shift

BUILD_DIR=build
ENABLE_FFMPEG=OFF
ENABLE_ASAN=OFF

# Parse extra args
while [[ $# -gt 0 ]]; do
    case "$1" in
        --ffmpeg)
            ENABLE_FFMPEG=ON
            ;;
        --asan)
            ENABLE_ASAN=ON
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 <CAMERA_SDK_PATH> [--ffmpeg] [--asan]"
            exit 1
            ;;
    esac
    shift
done

# Clean build directory
if [ -d "$BUILD_DIR" ]; then
    echo "Warning: This will delete the existing '$BUILD_DIR' directory."
    read -p "Continue? [y/N] " confirm
    if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
        echo "Aborted."
        exit 1
    fi
    rm -rf "$BUILD_DIR"
fi

mkdir "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure
cmake \
  -DCAMERA_SDK_PATH="$CAMERA_SDK_PATH" \
  -DENABLE_FFMPEG=$ENABLE_FFMPEG \
  -DENABLE_ASAN=$ENABLE_ASAN \
  -DCMAKE_BUILD_TYPE=Debug \
  ..

# Build
make -j$(nproc)

# Run
if [ $? -eq 0 ]; then
    echo "Build successful!"
else
    echo "Build failed!"
    exit 1
fi
