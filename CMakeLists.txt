cmake_minimum_required(VERSION 3.10)
project(Ecode LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define Unicode for Win32
add_definitions(-DUNICODE -D_UNICODE)

# Include directories
include_directories(include)

# Source files
file(GLOB SOURCES "src/*.cpp")
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/test_piecetable.cpp")
set(DUKTAPE_SOURCES "src-duktape/duktape.c")
set(RESOURCES "src/Ecode.rc")

# Executable (GUI)
add_executable(ecode WIN32 ${SOURCES} ${DUKTAPE_SOURCES} ${RESOURCES})

# Executable (Console for testing)
add_executable(ecode_console ${SOURCES} ${DUKTAPE_SOURCES} ${RESOURCES})
target_compile_definitions(ecode_console PRIVATE ECODE_CONSOLE_BUILD)

# Test executables base
set(TEST_BASE_SOURCES 
    tests/TestGlobals.cpp
    src/FileUtils.cpp
)

add_executable(test_piecetable 
    tests/test_piecetable.cpp 
    src/PieceTable.cpp
)

add_executable(test_piecetable_stress
    tests/test_piecetable_stress.cpp
    src/PieceTable.cpp
)

add_executable(test_editor_core
    tests/test_editor_core.cpp
    ${TEST_BASE_SOURCES}
    src/Buffer.cpp
    src/PieceTable.cpp
    src/MemoryMappedFile.cpp
    src/Process.cpp
    src/SettingsManager.cpp
    src-duktape/duktape.c
    tests/DebugLog_Stub.cpp
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

add_executable(test_undoredo_stress
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

add_executable(test_search_replace
    tests/test_search_replace.cpp
    ${TEST_BASE_SOURCES}
    src/Buffer.cpp
    src/PieceTable.cpp
    src/MemoryMappedFile.cpp
    src/Process.cpp
    src/SettingsManager.cpp
    src-duktape/duktape.c
    tests/DebugLog_Stub.cpp
    src/FileUtils.cpp
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

add_executable(test_file_io
    tests/test_file_io.cpp
    ${TEST_BASE_SOURCES}
    src/Buffer.cpp
    src/PieceTable.cpp
    src/MemoryMappedFile.cpp
    src/Process.cpp
    src/SettingsManager.cpp
    tests/DebugLog_Stub.cpp
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

add_executable(performance_benchmark
    tests/performance_benchmark.cpp
    ${TEST_BASE_SOURCES}
    src/PieceTable.cpp
    src/Buffer.cpp
    src/MemoryMappedFile.cpp
    src/Process.cpp
    src/SettingsManager.cpp
    tests/DebugLog_Stub.cpp
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

add_executable(test_visual_wrap
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
    tests/DebugLog_Stub.cpp
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

add_executable(test_find_in_files
    tests/test_find_in_files.cpp
    ${TEST_BASE_SOURCES}
    src/Editor.cpp
    src/Buffer.cpp
    src/PieceTable.cpp
    src/MemoryMappedFile.cpp
    src/Process.cpp
    src/SettingsManager.cpp
    src-duktape/duktape.c
    tests/DebugLog_Stub.cpp
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

add_executable(test_shortcuts
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
    tests/DebugLog_Stub.cpp
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
)

# Output directory
set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set_target_properties(ecode PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set_target_properties(ecode_console PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set_target_properties(test_piecetable PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set_target_properties(test_piecetable_stress PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set_target_properties(test_editor_core PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set_target_properties(test_undoredo_stress PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set_target_properties(test_search_replace PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set_target_properties(test_file_io PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set_target_properties(test_visual_wrap PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set_target_properties(test_find_in_files PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set_target_properties(test_shortcuts PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set_target_properties(performance_benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")

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
else()
    message(WARNING "Inno Setup compiler (ISCC.exe) not found. Installer target will not be available.")
endif()

# Build instructions for the user
# mkdir build
# cd build
# cmake ..
# cmake --build . --target installer
