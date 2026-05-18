cmake_minimum_required(VERSION 3.10)
project(CSVEditor)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define UNICODE for Win32 API
add_definitions(-DUNICODE -D_UNICODE)

# ── Core Library ──────────────────────────────────────────────────────────

set(CSVEDITOR_LIB_SOURCES
    src/MemoryMappedFile.cpp
    src/PieceTable.cpp
    src/CsvDocument.cpp
    src/EditorState.cpp
    src/Localization.cpp
    src/ConfigManager.cpp
)

add_library(CSVEditorLib STATIC ${CSVEDITOR_LIB_SOURCES})

target_include_directories(CSVEditorLib
    PUBLIC  include
    PRIVATE src
)

target_link_libraries(CSVEditorLib
    PUBLIC user32 gdi32 dwrite shlwapi
)

# ── GUI Executable ────────────────────────────────────────────────────────

add_executable(CSVEditor WIN32
    src/main.cpp
    src/MainWindow.cpp
    src/DirectXResources.cpp
    src/CSVEditor.rc
)

target_link_libraries(CSVEditor PRIVATE CSVEditorLib)

if(MSVC)
    target_compile_options(CSVEditor PRIVATE /utf-8)
    target_compile_options(CSVEditorLib PRIVATE /utf-8)
endif()

# ── Tests ─────────────────────────────────────────────────────────────────

add_executable(CSVEditorTests
    src/test_main.cpp
)

target_link_libraries(CSVEditorTests PRIVATE CSVEditorLib)

# ── Install ───────────────────────────────────────────────────────────────

install(TARGETS CSVEditorLib
    ARCHIVE DESTINATION lib
)

install(FILES include/csv_editor.h DESTINATION include)
