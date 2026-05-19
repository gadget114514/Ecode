cmake_minimum_required(VERSION 3.10)
project(FastDired LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(FastFD WIN32 src/main.cpp)

target_link_libraries(FastFD,
    d2d1
    dwrite
    shell32
    shlwapi
    user32
    kernel32
    advapi32
    ole32
)

target_compile_definitions(FastFD PRIVATE
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
    target_compile_options(FastFD PRIVATE /W4 /EHsc /utf-8)
endif()

set_target_properties(FastDired PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)
