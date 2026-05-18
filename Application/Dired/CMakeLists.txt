cmake_minimum_required(VERSION 3.10)
project(Dired LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Dired WIN32 src/main.cpp)

target_link_libraries(Dired
    comctl32
    comdlg32
    shell32
    shlwapi
    user32
    kernel32
    advapi32
)

target_compile_definitions(Dired PRIVATE
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
    target_compile_options(Dired PRIVATE /W4 /EHsc /utf-8)
endif()

set_target_properties(Dired PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)
