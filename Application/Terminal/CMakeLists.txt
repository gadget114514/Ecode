cmake_minimum_required(VERSION 3.10)
project(Terminal LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DUNICODE -D_UNICODE)
if(MSVC)
  add_compile_options(/utf-8)
endif()

include_directories(include)

set(SOURCES
    src/main.cpp
    src/TerminalBuffer.cpp
    src/TerminalEmulator.cpp
    src/TerminalView.cpp
    src/ConPtySession.cpp
    src/ImageManager.cpp
)

add_executable(Terminal WIN32 ${SOURCES})

target_link_libraries(Terminal
    user32
    gdi32
    shell32
    ole32
    d2d1
    dwrite
    windowscodecs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libsixel-windows/libsixel.lib
)

set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin")
# Terminal.exe goes into bin/<Config>/plugins/ alongside conpty.dll
set_target_properties(Terminal PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}/$<CONFIG>/plugins")

set(CONPTY_PKG "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party_reference/microsoft.windows.console.conpty.1.25.260512002-preview")

add_custom_command(TARGET Terminal POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CONPTY_PKG}/runtimes/win-x64/native/conpty.dll"
        "${OUTPUT_DIR}/$<CONFIG>/plugins/conpty.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CONPTY_PKG}/build/native/runtimes/x64/OpenConsole.exe"
        "${OUTPUT_DIR}/$<CONFIG>/plugins/OpenConsole.exe"
    COMMENT "Copying conpty.dll and OpenConsole.exe to plugins/"
)
