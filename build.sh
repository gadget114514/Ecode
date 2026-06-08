#!/bin/bash
set -e

BUILD_TYPE="${1:-Release}"
TARGET="$2"
ROOT="$(cd "$(dirname "$0")" && pwd)"

# Build Prompts plugin first (triggers its POST_BUILD to copy frontend)
echo "Building Prompts plugin..."
PROMPTS_DIR="$ROOT/Application/Prompts"
PROMPTS_BUILD_DIR="$PROMPTS_DIR/build"

mkdir -p "$PROMPTS_BUILD_DIR"
cmake -S "$PROMPTS_DIR" -B "$PROMPTS_BUILD_DIR" -G "Ninja" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" 2>/dev/null || \
cmake -S "$PROMPTS_DIR" -B "$PROMPTS_BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$PROMPTS_BUILD_DIR" --config "$BUILD_TYPE" --target Prompts

# Also copy frontend files directly from source to bin/Release/plugins/
echo "Copying Prompts frontend to output..."
cp -r "$PROMPTS_DIR/frontend/"* "$ROOT/bin/$BUILD_TYPE/plugins/" 2>/dev/null || true

# Main build
echo "Configuring CMake ($BUILD_TYPE)..."
BUILD_DIR="$ROOT/build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

echo "Building..."
if [ -n "$TARGET" ]; then
    cmake --build . --config "$BUILD_TYPE" --target "$TARGET"
else
    cmake --build . --config "$BUILD_TYPE"
fi

echo "Done."
