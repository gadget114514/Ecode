#!/usr/bin/env bash
# Copy frontend files (JS/HTML/CSS) to deployed Prompts for quick dev iteration
set -euo pipefail

FRONTEND_SRC="D:/ws/Ecode/Application/Prompts/frontend"
FRONTEND_DST="D:/ws/Ecode/bin/Release/plugins/Prompts/resources/frontend"

echo "=== Copying frontend files ==="
cp "$FRONTEND_SRC"/*.html "$FRONTEND_DST/"
cp "$FRONTEND_SRC"/*.js "$FRONTEND_DST/"
cp "$FRONTEND_SRC"/*.css "$FRONTEND_DST/"
cp -r "$FRONTEND_SRC"/lang "$FRONTEND_DST/"
cp -r "$FRONTEND_SRC"/lib "$FRONTEND_DST/"
cp -r "$FRONTEND_SRC"/wizards "$FRONTEND_DST/"
echo "Done."
