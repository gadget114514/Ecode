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

# Copy the packaged app to bin/<BUILD_TYPE>/plugins/
PLUGINS_OUT="$ROOT/bin/$BUILD_TYPE/plugins"
mkdir -p "$PLUGINS_OUT"

# Package the Electron app with @electron/packager (no winCodeSign needed)
# Output directly to target plugins directory
echo "  electron-packager..."
cd "$ELECTRON_DIR"
npx electron-packager . Prompts \
    --platform=win32 \
    --arch=x64 \
    --out="$PLUGINS_OUT" \
    --overwrite \
    --asar \
    --extra-resource=../frontend
cd "$ROOT"

# Rename to Prompts
if [ -d "$PLUGINS_OUT/Prompts-win32-x64" ]; then
    # embedded (plugins): bin/<BUILD_TYPE>/plugins/Prompts/
    rm -rf "$PLUGINS_OUT/Prompts"
    mv "$PLUGINS_OUT/Prompts-win32-x64" "$PLUGINS_OUT/Prompts"
    
    # standalone: bin/<BUILD_TYPE>/Prompts/
    STANDALONE_OUT="$ROOT/bin/$BUILD_TYPE/Prompts"
    rm -rf "$STANDALONE_OUT"
    cp -r "$PLUGINS_OUT/Prompts/." "$STANDALONE_OUT"
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
