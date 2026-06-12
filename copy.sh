#!/usr/bin/env bash
# Copy frontend files (JS/HTML/CSS) and app.asar to deployed Prompts for quick dev iteration
set -euo pipefail

FRONTEND_SRC="D:/ws/Ecode/Application/Prompts/frontend"
FRONTEND_DST="D:/ws/Ecode/bin/Release/plugins/Prompts/resources/frontend"
ELECTRON_SRC="D:/ws/Ecode/Application/Prompts/electron"
ASAR_SRC="D:/ws/Ecode/Application/Prompts/electron/dist/Prompts-win32-x64/resources/app.asar"
ASAR_DST="D:/ws/Ecode/bin/Release/plugins/Prompts/resources/app.asar"

echo "=== Packaging app.asar ==="
TEMP_PACK="D:/ws/Ecode/Application/Prompts/electron/temp_pack"
mkdir -p "$TEMP_PACK"
cp "$ELECTRON_SRC"/main.js "$TEMP_PACK/"
cp "$ELECTRON_SRC"/preload.js "$TEMP_PACK/"
cp "$ELECTRON_SRC"/package.json "$TEMP_PACK/"

npx asar pack "$TEMP_PACK" "$ASAR_SRC"

rm -rf "$TEMP_PACK"

echo "=== Copying frontend files ==="
cp "$FRONTEND_SRC"/*.html "$FRONTEND_DST/"
cp "$FRONTEND_SRC"/*.js "$FRONTEND_DST/"
cp "$FRONTEND_SRC"/*.css "$FRONTEND_DST/"
cp -r "$FRONTEND_SRC"/lang "$FRONTEND_DST/"
cp -r "$FRONTEND_SRC"/lib "$FRONTEND_DST/"
cp -r "$FRONTEND_SRC"/wizards "$FRONTEND_DST/"

if [ -f "$ASAR_SRC" ]; then
    echo "=== Copying app.asar ==="
    cp "$ASAR_SRC" "$ASAR_DST"
else
    echo "Warning: app.asar not found at $ASAR_SRC"
fi

echo "Done."
