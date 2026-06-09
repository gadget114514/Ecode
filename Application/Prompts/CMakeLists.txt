cmake_minimum_required(VERSION 3.15)
project(Prompts LANGUAGES NONE)

# ── Electron app (Node.js / npm) ──────────────────────────────
set(ELECTRON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/electron")

find_program(NPM_EXECUTABLE npm REQUIRED)

# npm install — run once; re-runs if package.json changes
add_custom_command(
    OUTPUT  "${ELECTRON_DIR}/node_modules/.package-lock.json"
    COMMAND "${NPM_EXECUTABLE}" install
    WORKING_DIRECTORY "${ELECTRON_DIR}"
    DEPENDS "${ELECTRON_DIR}/package.json"
    COMMENT "npm install (Prompts Electron)"
)

add_custom_target(prompts_install
    DEPENDS "${ELECTRON_DIR}/node_modules/.package-lock.json"
)

# Start the Electron app (development)
add_custom_target(prompts_start
    COMMAND "${NPM_EXECUTABLE}" start
    WORKING_DIRECTORY "${ELECTRON_DIR}"
    DEPENDS prompts_install
    COMMENT "Starting Prompts (Electron)"
    USES_TERMINAL
)

# Package with electron-builder
add_custom_target(prompts_build
    COMMAND "${NPM_EXECUTABLE}" run build
    WORKING_DIRECTORY "${ELECTRON_DIR}"
    DEPENDS prompts_install
    COMMENT "Building Prompts installer (Electron)"
    USES_TERMINAL
)

# Default build target installs npm dependencies
add_custom_target(Prompts ALL DEPENDS prompts_install)

# ── Frontend sources (IDE visibility only) ────────────────────
file(GLOB_RECURSE PROMPTS_FRONTEND_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/frontend/*.html"
    "${CMAKE_CURRENT_SOURCE_DIR}/frontend/*.js"
    "${CMAKE_CURRENT_SOURCE_DIR}/frontend/*.css"
    "${CMAKE_CURRENT_SOURCE_DIR}/frontend/lang/*.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/frontend/wizards/*.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/frontend/lib/*.js"
)

file(GLOB_RECURSE PROMPTS_ELECTRON_SOURCES
    "${ELECTRON_DIR}/main.js"
    "${ELECTRON_DIR}/preload.js"
    "${ELECTRON_DIR}/package.json"
)

add_custom_target(prompts_sources SOURCES
    ${PROMPTS_FRONTEND_SOURCES}
    ${PROMPTS_ELECTRON_SOURCES}
)
