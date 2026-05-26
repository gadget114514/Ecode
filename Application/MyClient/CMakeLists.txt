cmake_minimum_required(VERSION 3.10)
project(MyClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DUNICODE -D_UNICODE -D_WINSOCK_DEPRECATED_NO_WARNINGS)
if(MSVC)
  add_compile_options(/utf-8)
endif()

set(TERMINAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Terminal")
include_directories(${TERMINAL_DIR}/include)

set(SOURCES
    src/main.cpp
    ${TERMINAL_DIR}/src/TerminalEmulator.cpp
    ${TERMINAL_DIR}/src/TerminalBuffer.cpp
)

add_executable(myclient ${SOURCES})

target_link_libraries(myclient
    user32
    ws2_32
)

set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set_target_properties(myclient PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
