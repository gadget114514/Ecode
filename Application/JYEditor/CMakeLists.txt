cmake_minimum_required(VERSION 3.15)

set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "vcpkg triplet")
project(JYEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static CRT — must match the x64-windows-static triplet.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(JYEditor WIN32
    src/main.cpp
    src/EditorWindow.cpp
    src/EditorWindow.h
    src/FileUtils.cpp
    src/FileUtils.h
    src/JSONEditorLocalization.cpp
    src/JSONEditorLocalization.h
    resources/resource.rc
)

# Link common Windows libraries
target_link_libraries(JYEditor PRIVATE comctl32 shlwapi)

target_compile_definitions(JYEditor PRIVATE UNICODE _UNICODE)

if(MSVC)
    target_compile_options(JYEditor PRIVATE /utf-8)
endif()


# nlohmann_json via vcpkg
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(JYEditor PRIVATE nlohmann_json::nlohmann_json)

# yaml-cpp built from source via FetchContent
include(FetchContent)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
set(YAML_CPP_FORMAT_SOURCE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 0.8.0
)
FetchContent_MakeAvailable(yaml-cpp)
target_link_libraries(JYEditor PRIVATE yaml-cpp)
