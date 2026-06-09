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

# Package the Electron app with electron-builder
echo "  npm run build..."
CSC_IDENTITY_AUTO_DISCOVERY=false npm --prefix "$ELECTRON_DIR" run build

# Copy the packaged installer / unpacked app to bin/<BUILD_TYPE>/plugins/
PLUGINS_OUT="$ROOT/bin/$BUILD_TYPE/plugins"
mkdir -p "$PLUGINS_OUT"

DIST_DIR="$ELECTRON_DIR/dist"
if [ -d "$DIST_DIR/win-unpacked" ]; then
    cp -r "$DIST_DIR/win-unpacked" "$PLUGINS_OUT/Prompts"
elif [ -d "$DIST_DIR" ]; then
    # Copy any installer exe that was produced
    find "$DIST_DIR" -maxdepth 1 -name "*.exe" -exec cp {} "$PLUGINS_OUT/" \; 2>/dev/null || true
fi

# Always copy frontend sources for other plugins that embed them directly
echo "Copying Prompts frontend to output..."
cp -r "$PROMPTS_DIR/frontend/." "$PLUGINS_OUT/" 2>/dev/null || true

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
