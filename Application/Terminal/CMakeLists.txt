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
)

add_executable(Terminal WIN32 ${SOURCES})

target_link_libraries(Terminal
    user32
    gdi32
    shell32
    ole32
    d2d1
    dwrite
)

set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set_target_properties(Terminal PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
