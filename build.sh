#!/bin/bash
set -e

BUILD_TYPE="${1:-Release}"
TARGET="$2"
ROOT="$(cd "$(dirname "$0")" && pwd)"

# ── Prompts (Electron) ────────────────────────────────────────
echo "Building Prompts (Electron)..."
PROMPTS_DIR="$ROOT/Application/Prompts"
ELECTRON_DIR="$PROMPTS_DIR/electron"

# Install npm dependencies if needed
if [ ! -d "$ELECTRON_DIR/node_modules" ]; then
    echo "  npm install..."
    npm --prefix "$ELECTRON_DIR" install
fi

# Package the Electron app with @electron/packager (no winCodeSign needed)
echo "  npm run build..."
npm --prefix "$ELECTRON_DIR" run build

# Copy the packaged app to bin/<BUILD_TYPE>/plugins/
PLUGINS_OUT="$ROOT/bin/$BUILD_TYPE/plugins"
mkdir -p "$PLUGINS_OUT"

DIST_DIR="$ELECTRON_DIR/dist/Prompts-win32-x64"
if [ -d "$DIST_DIR" ]; then
    # embedded (plugins): bin/<BUILD_TYPE>/plugins/Prompts/
    rm -rf "$PLUGINS_OUT/Prompts"
    cp -r "$DIST_DIR/." "$PLUGINS_OUT/Prompts"
    # standalone: bin/<BUILD_TYPE>/Prompts/
    STANDALONE_OUT="$ROOT/bin/$BUILD_TYPE/Prompts"
    rm -rf "$STANDALONE_OUT"
    cp -r "$DIST_DIR/." "$STANDALONE_OUT"
fi

# ── Main build ────────────────────────────────────────────────
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
