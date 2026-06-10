cmake_minimum_required(VERSION 3.10)
project(Ecode LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Git-based version auto-detection
execute_process(
    COMMAND git describe --tags --always --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_DESCRIBE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_DESCRIBE)
    set(GIT_DESCRIBE "v1.0")
endif()
string(REGEX MATCH "^[vV]" HAS_PREFIX "${GIT_DESCRIBE}")
if(HAS_PREFIX)
    string(REGEX REPLACE "^[vV]" "" GIT_CLEAN "${GIT_DESCRIBE}")
else()
    set(GIT_CLEAN "${GIT_DESCRIBE}")
endif()
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" VERSION_MATCH "${GIT_CLEAN}")
if(VERSION_MATCH)
    set(ECODE_VERSION_STRING "${GIT_DESCRIBE}")
    set(ECODE_VERSION_MAJOR "${CMAKE_MATCH_1}")
    set(ECODE_VERSION_MINOR "${CMAKE_MATCH_2}")
    set(ECODE_VERSION_PATCH "${CMAKE_MATCH_3}")
else()
    set(ECODE_VERSION_STRING "${GIT_DESCRIBE}")
    set(ECODE_VERSION_MAJOR "1")
    set(ECODE_VERSION_MINOR "0")
    set(ECODE_VERSION_PATCH "0")
endif()
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/version_config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/version_config.h"
)

add_compile_definitions(UNICODE _UNICODE)

if(MSVC)
  add_compile_options(/utf-8)
endif()

# Generate icon resource file from 24x24 icon set
set(ICONS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/images/24x24-free-application-icons/24x24-free-application-icons/ico")
set(GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/generated")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${GEN_DIR}"
)
execute_process(
    COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_icons_rc.py"
            "${ICONS_DIR}"
            "${GEN_DIR}/icons.rc"
            "${GEN_DIR}/icons.h"
            "11000"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/generated/icons.rc")
    file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/src/generated/icons.rc" "")
endif()

set(ECODE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AppMain.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Dialogs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Editor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorBufferRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorBufferRenderer_Draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FileUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Globals.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Globals.test.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/IMEState.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/JsApi_EditorSettings.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/JsApi_FileAndBuffer.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/JsApi_Lsp.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/JsApi_ShellAndMinibuffer.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/JsApi_StatusAndDialogs.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/JsApi_TextEditing.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Localization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LspClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MemoryMappedFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MinibufferHandler.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PieceTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Process.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ScriptEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SettingsManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/TabDragHandler.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/TabIconHelper.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/TabSwitcherView.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/TopBar.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/TreeViewHelpers.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/UIHelpers.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WindowHandlers_Command.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WindowHandlers_Core.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WindowHandlers_Input.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/copyright.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Ecode.rc
    ${CMAKE_CURRENT_SOURCE_DIR}/Application/Terminal/src/TerminalGridView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Application/FastFileSearch/src/MFTReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Application/FastFileSearch/src/FatReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Application/FastFileSearch/src/exFatReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src-duktape/duktape.c
)

set(ECODE_LIBS
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

add_executable(ecode WIN32 ${ECODE_SOURCES})

target_include_directories(ecode PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Application/FastFileSearch/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/generated
)

target_link_libraries(ecode PRIVATE ${ECODE_LIBS})

# Console build for testing
add_executable(ecode_console EXCLUDE_FROM_ALL ${ECODE_SOURCES})
target_compile_definitions(ecode_console PRIVATE ECODE_CONSOLE_BUILD)
target_include_directories(ecode_console PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Application/FastFileSearch/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/generated
)
target_link_libraries(ecode_console PRIVATE ${ECODE_LIBS})

set(TEST_BASE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/TestGlobals.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FileUtils.cpp
)

set(TEST_COMMON_LIBS
    user32
    gdi32
    shell32
    ole32
    comdlg32
    comctl32
    d2d1
    dwrite
)

function(add_ecode_test name)
    add_executable(${name} EXCLUDE_FROM_ALL ${ARGN})
    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/generated
    )
endfunction()

# Test executables
add_ecode_test(test_piecetable
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_piecetable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PieceTable.cpp
)

add_ecode_test(test_piecetable_stress
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_piecetable_stress.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PieceTable.cpp
)

add_ecode_test(test_editor_core
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_editor_core.cpp
    ${TEST_BASE_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Editor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PieceTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MemoryMappedFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Process.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SettingsManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FileUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src-duktape/duktape.c
)
target_link_libraries(test_editor_core PRIVATE ${TEST_COMMON_LIBS})

add_ecode_test(test_session_management
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_session_management.cpp
    ${TEST_BASE_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Editor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PieceTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MemoryMappedFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Process.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SettingsManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FileUtils.cpp
)
target_link_libraries(test_session_management PRIVATE ${TEST_COMMON_LIBS})

add_ecode_test(test_undoredo_stress
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_undoredo_stress.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PieceTable.cpp
)
target_link_libraries(test_undoredo_stress PRIVATE ${TEST_COMMON_LIBS})

add_ecode_test(test_search_replace
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_search_replace.cpp
    ${TEST_BASE_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Editor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PieceTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MemoryMappedFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Process.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SettingsManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src-duktape/duktape.c
)
target_link_libraries(test_search_replace PRIVATE ${TEST_COMMON_LIBS})

add_ecode_test(test_file_io
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_file_io.cpp
    ${TEST_BASE_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Editor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PieceTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MemoryMappedFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Process.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SettingsManager.cpp
)
target_link_libraries(test_file_io PRIVATE ${TEST_COMMON_LIBS})

add_ecode_test(performance_benchmark
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/performance_benchmark.cpp
    ${TEST_BASE_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Editor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PieceTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MemoryMappedFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Process.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SettingsManager.cpp
)
target_link_libraries(performance_benchmark PRIVATE ${TEST_COMMON_LIBS})

add_ecode_test(test_visual_wrap
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_visual_wrap.cpp
    ${TEST_BASE_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/IMEState.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorBufferRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorBufferRenderer_Draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PieceTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MemoryMappedFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Localization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SettingsManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Editor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Process.cpp
)
target_link_libraries(test_visual_wrap PRIVATE ${TEST_COMMON_LIBS})

add_ecode_test(test_find_in_files
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_find_in_files.cpp
    ${TEST_BASE_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Editor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PieceTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MemoryMappedFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Process.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SettingsManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src-duktape/duktape.c
)
target_link_libraries(test_find_in_files PRIVATE ${TEST_COMMON_LIBS})

add_ecode_test(test_shortcuts
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_shortcuts.cpp
    ${TEST_BASE_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/IMEState.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Editor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PieceTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MemoryMappedFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Process.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SettingsManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Localization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorBufferRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorBufferRenderer_Draw.cpp
)
target_link_libraries(test_shortcuts PRIVATE ${TEST_COMMON_LIBS})

add_ecode_test(test_string_helpers
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_string_helpers.cpp
)

add_ecode_test(test_localization
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_localization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Localization.cpp
)

add_ecode_test(test_buffer_stress
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_buffer_stress.cpp
    ${TEST_BASE_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Editor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PieceTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MemoryMappedFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Process.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SettingsManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src-duktape/duktape.c
)
target_link_libraries(test_buffer_stress PRIVATE ${TEST_COMMON_LIBS})

add_ecode_test(test_cursor_move
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cursor_move.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PieceTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MemoryMappedFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Process.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SettingsManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FileUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/TestGlobals.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Localization.cpp
)
target_link_libraries(test_cursor_move PRIVATE ${TEST_COMMON_LIBS})

add_ecode_test(test_memorymappedfile
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_memorymappedfile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MemoryMappedFile.cpp
)

add_ecode_test(test_fileutils
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_fileutils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/FileUtilsTestGlobals.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FileUtils.cpp
)
target_link_libraries(test_fileutils PRIVATE
    user32 gdi32 shell32 ole32 comdlg32 comctl32
)

add_ecode_test(test_editor_buffer_ops
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_editor_buffer_ops.cpp
    ${TEST_BASE_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Editor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PieceTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MemoryMappedFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Process.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SettingsManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FileUtils.cpp
)
target_link_libraries(test_editor_buffer_ops PRIVATE ${TEST_COMMON_LIBS})

add_ecode_test(test_piecetable_advanced
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_piecetable_advanced.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PieceTable.cpp
)

add_ecode_test(test_telnetd_iac
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_telnetd_iac.c
)

add_ecode_test(test_localmsg_cli
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_localmsg_cli.cpp
)
target_link_libraries(test_localmsg_cli PRIVATE kernel32)

add_ecode_test(test_localmsg_server
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_localmsg_server.cpp
)
target_link_libraries(test_localmsg_server PRIVATE kernel32)

# Output directory
set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set_target_properties(ecode PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set_target_properties(ecode_console PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")

set(TEST_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin/tests")
foreach(tgt
    test_piecetable test_piecetable_stress test_editor_core
    test_session_management test_undoredo_stress test_search_replace
    test_file_io test_visual_wrap test_find_in_files test_shortcuts
    performance_benchmark test_string_helpers test_localization
    test_buffer_stress test_cursor_move test_memorymappedfile
    test_fileutils test_editor_buffer_ops test_piecetable_advanced
    test_telnetd_iac test_localmsg_cli test_localmsg_server
)
    set_target_properties(${tgt} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIR}")
endforeach()

# Copy scripts and doc to output
macro(copy_assets_to_target target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts"
        "$<TARGET_FILE_DIR:${target}>/scripts"
        COMMENT "Copying scripts to output directory..."
    )
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/doc"
        "$<TARGET_FILE_DIR:${target}>/doc"
        COMMENT "Copying doc to output directory..."
    )
endmacro()

copy_assets_to_target(ecode)
copy_assets_to_target(ecode_console)

# ── Plugin system ──────────────────────────────────────────────────────────

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

add_plugin(Dired          Application/Dired)
add_plugin(CSVEditor      Application/CSVEditor)
add_plugin(FastFileSearch Application/FastFileSearch)
add_plugin(FastFD         Application/FastFD)
add_plugin(Terminal       Application/Terminal)
add_plugin(SureBackup     Application/SureBackup)
add_plugin(dukluv         Application/dukluv)
add_plugin(JYEditor       Application/JYEditor)
add_plugin(Prompts        Application/Prompts)
add_plugin(FastDired      Application/FastDired)

# Macro to configure and build a plugin via its own CMake build tree
macro(add_plugin_build name dir)
    set(p_build "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/build")
    add_custom_command(TARGET ecode PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${p_build}"
        COMMAND ${CMAKE_COMMAND} -S "${CMAKE_CURRENT_SOURCE_DIR}/${dir}"
            -B "${p_build}" -G "${CMAKE_GENERATOR}" -A x64
        COMMAND ${CMAKE_COMMAND} --build "${p_build}" --config $<CONFIG>
        COMMENT "Building ${name}..."
    )
endmacro()

add_plugin_build(Dired          Application/Dired)
add_plugin_build(CSVEditor      Application/CSVEditor)
add_plugin_build(FastFileSearch Application/FastFileSearch)
add_plugin_build(FastFD         Application/FastFD)
add_plugin_build(Terminal       Application/Terminal)
add_plugin_build(SureBackup     Application/SureBackup)
add_plugin_build(dukluv         Application/dukluv)
add_plugin_build(JYEditor       Application/JYEditor)
add_plugin_build(FastDired      Application/FastDired)

# Custom/unknown plugins: copy any .exe from plugins_custom/
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

# Build LocalMsg plugin via its own CMake build tree
set(LOCALMSG_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Application/LocalMsg/build")
add_custom_command(TARGET ecode PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${plugin_out}/plugins"
    COMMAND ${CMAKE_COMMAND} --build "${LOCALMSG_BUILD_DIR}" --config $<CONFIG>
    COMMENT "Building LocalMsg..."
)

# ── Installer ─────────────────────────────────────────────────────────────

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
