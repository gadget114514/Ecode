cmake_minimum_required(VERSION 3.10)
project(Ecode LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define Unicode for Win32
add_definitions(-DUNICODE -D_UNICODE)

# Treat all source files as UTF-8 on MSVC (avoids encoding errors with Unicode literals)
if(MSVC)
  add_compile_options(/utf-8)
endif()

# Include directories
include_directories(include)
include_directories(FastFileSearch/src)

# Source files
file(GLOB SOURCES "src/*.cpp")
list(APPEND SOURCES "Application/Terminal/src/TerminalGridView.cpp")
file(GLOB FASTFS_SOURCES "FastFileSearch/src/MFTReader.cpp" "FastFileSearch/src/FatReader.cpp" "FastFileSearch/src/exFatReader.cpp")
list(APPEND SOURCES ${FASTFS_SOURCES})
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/test_piecetable.cpp")
set(DUKTAPE_SOURCES "src-duktape/duktape.c")
set(RESOURCES "src/Ecode.rc")

# Executable (GUI)
add_executable(ecode WIN32 ${SOURCES} ${DUKTAPE_SOURCES} ${RESOURCES})

# Executable (Console for testing)
add_executable(ecode_console EXCLUDE_FROM_ALL ${SOURCES} ${DUKTAPE_SOURCES} ${RESOURCES})
target_compile_definitions(ecode_console PRIVATE ECODE_CONSOLE_BUILD)

# Test executables base
set(TEST_BASE_SOURCES 
    tests/TestGlobals.cpp
    src/FileUtils.cpp
)

add_executable(test_piecetable EXCLUDE_FROM_ALL
    tests/test_piecetable.cpp 
    src/PieceTable.cpp
)

add_executable(test_piecetable_stress EXCLUDE_FROM_ALL
    tests/test_piecetable_stress.cpp
    src/PieceTable.cpp
)

add_executable(test_editor_core EXCLUDE_FROM_ALL
    tests/test_editor_core.cpp
    ${TEST_BASE_SOURCES}
    src/Buffer.cpp
    src/PieceTable.cpp
    src/MemoryMappedFile.cpp
    src/Process.cpp
    src/SettingsManager.cpp
    src-duktape/duktape.c
    src-duktape/duktape.c
    src/FileUtils.cpp
)
target_link_libraries(test_editor_core 
    user32 
    gdi32 
    shell32 
    ole32 
    comdlg32
    comctl32
    d2d1 
    dwrite
)

add_executable(test_undoredo_stress EXCLUDE_FROM_ALL
    tests/test_undoredo_stress.cpp
    src/PieceTable.cpp
)
target_link_libraries(test_undoredo_stress 
    user32 
    gdi32 
    shell32 
    ole32 
    comdlg32
    comctl32
    d2d1 
    dwrite
)

add_executable(test_search_replace EXCLUDE_FROM_ALL
    tests/test_search_replace.cpp
    ${TEST_BASE_SOURCES}
    src/Buffer.cpp
    src/PieceTable.cpp
    src/MemoryMappedFile.cpp
    src/Process.cpp
    src/SettingsManager.cpp
    src-duktape/duktape.c
    src-duktape/duktape.c
)
target_link_libraries(test_search_replace 
    user32 
    gdi32 
    shell32 
    ole32 
    comdlg32
    comctl32
    d2d1 
    dwrite
)

add_executable(test_file_io EXCLUDE_FROM_ALL
    tests/test_file_io.cpp
    ${TEST_BASE_SOURCES}
    src/Buffer.cpp
    src/PieceTable.cpp
    src/MemoryMappedFile.cpp
    src/Process.cpp
    src/SettingsManager.cpp
)
target_link_libraries(test_file_io 
    user32 
    gdi32 
    shell32 
    ole32 
    comdlg32
    comctl32
    d2d1 
    dwrite
)

add_executable(performance_benchmark EXCLUDE_FROM_ALL
    tests/performance_benchmark.cpp
    ${TEST_BASE_SOURCES}
    src/PieceTable.cpp
    src/Buffer.cpp
    src/MemoryMappedFile.cpp
    src/Process.cpp
    src/SettingsManager.cpp
)
target_link_libraries(performance_benchmark 
    user32 
    gdi32 
    shell32 
    ole32 
    comdlg32
    comctl32
    d2d1 
    dwrite
)

add_executable(test_visual_wrap EXCLUDE_FROM_ALL
    tests/test_visual_wrap.cpp
    ${TEST_BASE_SOURCES}
    src/EditorBufferRenderer.cpp
    src/EditorBufferRenderer_Draw.cpp
    src/Buffer.cpp
    src/PieceTable.cpp
    src/MemoryMappedFile.cpp
    src/Localization.cpp
    src/SettingsManager.cpp
    src/Editor.cpp
    src/Process.cpp
)
target_link_libraries(test_visual_wrap 
    user32 
    gdi32 
    shell32 
    ole32 
    comdlg32
    comctl32
    d2d1 
    dwrite
)

add_executable(test_find_in_files EXCLUDE_FROM_ALL
    tests/test_find_in_files.cpp
    ${TEST_BASE_SOURCES}
    src/Editor.cpp
    src/Buffer.cpp
    src/PieceTable.cpp
    src/MemoryMappedFile.cpp
    src/Process.cpp
    src/SettingsManager.cpp
    src-duktape/duktape.c
)
target_link_libraries(test_find_in_files 
    user32 
    gdi32 
    shell32 
    ole32 
    comdlg32
    comctl32
    d2d1 
    dwrite
)

add_executable(test_shortcuts EXCLUDE_FROM_ALL
    tests/test_shortcuts.cpp
    ${TEST_BASE_SOURCES}
    src/Editor.cpp
    src/Buffer.cpp
    src/PieceTable.cpp
    src/MemoryMappedFile.cpp
    src/Process.cpp
    src/SettingsManager.cpp
    src/Localization.cpp
    src/EditorBufferRenderer.cpp
    src/EditorBufferRenderer_Draw.cpp
)
target_link_libraries(test_shortcuts 
    user32 
    gdi32 
    shell32 
    ole32 
    comdlg32
    comctl32
    d2d1 
    dwrite
)

add_executable(test_string_helpers EXCLUDE_FROM_ALL
    tests/test_string_helpers.cpp
)

add_executable(test_localization EXCLUDE_FROM_ALL
    tests/test_localization.cpp
    src/Localization.cpp
)

add_executable(test_buffer_stress EXCLUDE_FROM_ALL
    tests/test_buffer_stress.cpp
    ${TEST_BASE_SOURCES}
    src/Buffer.cpp
    src/PieceTable.cpp
    src/MemoryMappedFile.cpp
    src/Process.cpp
    src/SettingsManager.cpp
    src-duktape/duktape.c
)
target_link_libraries(test_buffer_stress
    user32
    gdi32
    shell32
    ole32
    comdlg32
    comctl32
    d2d1
    dwrite
)

add_executable(test_cursor_move EXCLUDE_FROM_ALL
    tests/test_cursor_move.cpp
    ${TEST_BASE_SOURCES}
    src/Buffer.cpp
    src/PieceTable.cpp
    src/MemoryMappedFile.cpp
    src/Process.cpp
    src/SettingsManager.cpp
    src-duktape/duktape.c
    src-duktape/duktape.c
    src/FileUtils.cpp
)
target_link_libraries(test_cursor_move
    user32
    gdi32
    shell32
    ole32
    comdlg32
    comctl32
    d2d1
    dwrite
)

# Libraries to link
target_link_libraries(ecode
    user32
    gdi32
    shell32
    ole32
    comdlg32
    comctl32
    d2d1
    dwrite
    imm32
    dwmapi
)

target_link_libraries(ecode_console 
    user32 
    gdi32 
    shell32 
    ole32 
    comdlg32 
    comctl32 
    d2d1 
    dwrite
    imm32
    dwmapi
)

# Output directory
set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set_target_properties(ecode PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set_target_properties(ecode_console PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(TEST_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin/tests")
set_target_properties(test_piecetable PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIR}")
set_target_properties(test_piecetable_stress PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIR}")
set_target_properties(test_editor_core PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIR}")
set_target_properties(test_undoredo_stress PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIR}")
set_target_properties(test_search_replace PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIR}")
set_target_properties(test_file_io PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIR}")
set_target_properties(test_visual_wrap PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIR}")
set_target_properties(test_find_in_files PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIR}")
set_target_properties(test_shortcuts PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIR}")
set_target_properties(performance_benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIR}")
set_target_properties(test_string_helpers PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIR}")
set_target_properties(test_localization PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIR}")
set_target_properties(test_buffer_stress PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIR}")

# Copy scripts directory to output
add_custom_command(TARGET ecode POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts"
    "$<TARGET_FILE_DIR:ecode>/scripts"
    COMMENT "Copying scripts to output directory..."
)
add_custom_command(TARGET ecode POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/doc"
    "$<TARGET_FILE_DIR:ecode>/doc"
    COMMENT "Copying doc to output directory..."
)

add_custom_command(TARGET ecode_console POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts"
    "$<TARGET_FILE_DIR:ecode_console>/scripts"
    COMMENT "Copying scripts to output directory..."
)
add_custom_command(TARGET ecode_console POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/doc"
    "$<TARGET_FILE_DIR:ecode_console>/doc"
    COMMENT "Copying doc to output directory..."
)

# Copy each pre-built plugin EXE into the output directory.
# Each plugin is built independently via its own CMakeLists.txt.
macro(add_plugin name dir)
    set(plugin_src "${CMAKE_CURRENT_SOURCE_DIR}/${dir}")
    set(plugin_build "${plugin_src}/build")
    set(plugin_out "$<TARGET_FILE_DIR:ecode>")
    add_custom_command(TARGET ecode POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${plugin_out}/plugins"
        COMMAND ${CMAKE_COMMAND}
            -D src_dir="${plugin_src}"
            -D build_dir="${plugin_build}"
            -D out_dir="${plugin_out}"
            -D name="${name}"
            -D config=$<CONFIG>
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_plugin.cmake"
        COMMENT "Copying ${name}.exe..."
    )
endmacro()

add_plugin(Dired     Application/Dired)
add_plugin(CSVEditor Application/CSVEditor)
add_plugin(FastFileSearch Application/FastFileSearch)
add_plugin(FastFD Application/FastFD)
add_plugin(Terminal  Application/Terminal)

# Copy FastFileSearch to apps/ directory for app launcher
set(plugin_out "$<TARGET_FILE_DIR:ecode>")
add_custom_command(TARGET ecode POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${plugin_out}/apps"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${plugin_out}/plugins/FastFileSearch.exe"
        "${plugin_out}/apps/FastFileSearch.exe"
    COMMENT "Copying FastFileSearch.exe to apps/..."
)

# Custom/unknown plugins: copy any .exe from plugins_custom/ to output
set(CUSTOM_PLUGIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/plugins_custom")
set(PLUGIN_OUT_DIR "$<TARGET_FILE_DIR:ecode>/plugins")
file(GLOB CUSTOM_PLUGINS "${CUSTOM_PLUGIN_DIR}/*.exe")
foreach(PLUGIN ${CUSTOM_PLUGINS})
    get_filename_component(PLUGIN_NAME ${PLUGIN} NAME)
    add_custom_command(TARGET ecode POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PLUGIN}"
        "${PLUGIN_OUT_DIR}/${PLUGIN_NAME}"
        COMMENT "Copying custom plugin ${PLUGIN_NAME}..."
    )
endforeach()

find_package(nlohmann_json QUIET)
if(nlohmann_json_FOUND)
    add_plugin(JYEditor Application/JYEditor)
    # Copy yaml-cpp.dll alongside JYEditor.exe
    add_custom_command(TARGET ecode POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/Application/JYEditor/vcpkg_installed/vcpkg/blds/yaml-cpp/x64-windows-rel/yaml-cpp.dll"
        "$<TARGET_FILE_DIR:ecode>/yaml-cpp.dll"
        COMMENT "Copying yaml-cpp.dll..."
    )
else()
    message(STATUS "JYEditor skipped (nlohmann_json not found)")
endif()



# Installer
find_program(ISCC_EXECUTABLE ISCC 
    PATHS "C:/Program Files (x86)/Inno Setup 6"
)

if(ISCC_EXECUTABLE)
    message(STATUS "Found Inno Setup compiler: ${ISCC_EXECUTABLE}")
    add_custom_target(installer
        COMMAND ${ISCC_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/installer/ecode.iss"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Building Inno Setup installer..."
        VERBATIM
    )
    add_dependencies(installer ecode)

    # All-plugins installer
    add_custom_target(installer_allplugins
        COMMAND ${ISCC_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/installer/ecode_allplugins.iss"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Building Inno Setup installer with all plugins..."
        VERBATIM
    )
    add_dependencies(installer_allplugins ecode)
else()
    message(WARNING "Inno Setup compiler (ISCC.exe) not found. Installer target will not be available.")
endif()

# Build instructions for the user
# mkdir build
# cd build
# cmake ..
# cmake --build . --target installer
# cmake --build . --target installer_allplugins
