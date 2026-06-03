#!/bin/bash
set -e

BUILD_DIR="build"
BUILD_TYPE="${1:-Release}"
TARGET="$2"

cd "$(dirname "$0")"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "Configuring CMake ($BUILD_TYPE)..."
cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

echo "Building..."
if [ -n "$TARGET" ]; then
    cmake --build . --config "$BUILD_TYPE" --target "$TARGET"
else
    cmake --build . --config "$BUILD_TYPE"
fi

echo "Done."
