#!/usr/bin/env bash
# Build Prompts Electron app directly to plugins directory
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== Building Prompts Electron app ==="
cd "$SCRIPT_DIR"
npm run build

echo ""
echo "=== Renaming output directory ==="
# electron-packager creates Prompts-win32-x64, rename to Prompts
PLUGINS_DIR="$(cd "$SCRIPT_DIR/../../../" && pwd)/bin/Release/plugins"
if [ -d "$PLUGINS_DIR/Prompts-win32-x64" ]; then
    # Remove old Prompts dir, rename new one
    rm -rf "$PLUGINS_DIR/Prompts"
    mv "$PLUGINS_DIR/Prompts-win32-x64" "$PLUGINS_DIR/Prompts"
fi

echo ""
echo "=== Done ==="
