cmake_minimum_required(VERSION 3.15)
project(JYEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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


# If using vcpkg, these find_packages will work if installed
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(JYEditor PRIVATE nlohmann_json::nlohmann_json)

# YAML-CPP
find_package(yaml-cpp CONFIG REQUIRED)
target_link_libraries(JYEditor PRIVATE yaml-cpp::yaml-cpp)
