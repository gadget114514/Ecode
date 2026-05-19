cmake_minimum_required(VERSION 3.10)
project(Terminal LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Terminal WIN32 src/main.cpp)

target_link_libraries(Terminal
    d2d1
    dwrite
    user32
    kernel32
    advapi32
    ole32
)

target_compile_definitions(Terminal PRIVATE
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
    target_compile_options(Terminal PRIVATE /W4 /EHsc /utf-8)
endif()
