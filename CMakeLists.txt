cmake_minimum_required(VERSION 3.15)
project(Prompts LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(Prompts WIN32
    src/main.cpp
    src/App.cpp
    src/Bridge.cpp
    src/NodeData.cpp
    src/Storage.cpp
    src/JsonParser.cpp
    src/Base64.cpp
    src/PipelineRunner.cpp
    src/AIProvider.cpp
    src/PromptsLocalization.cpp
    resources/resource.rc
)

target_include_directories(Prompts PRIVATE src resources ${CMAKE_CURRENT_SOURCE_DIR}/packages/wv2/build/native/include)
target_link_libraries(Prompts PRIVATE
    comctl32
    shlwapi
    user32
    kernel32
    winhttp
    ole32
    oleaut32
    ws2_32
    "${CMAKE_CURRENT_SOURCE_DIR}/packages/wv2/build/native/x64/WebView2Loader.dll.lib"
)

target_compile_definitions(Prompts PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

if(MSVC)
    target_compile_options(Prompts PRIVATE /utf-8 /W4 /EHsc)
endif()

# Copy frontend files to output
add_custom_command(TARGET Prompts POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/frontend"
    "$<TARGET_FILE_DIR:Prompts>/frontend"
    COMMENT "Copying frontend files..."
)
add_custom_command(TARGET Prompts POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_CURRENT_SOURCE_DIR}/packages/wv2/build/native/x64/WebView2Loader.dll"
    "$<TARGET_FILE_DIR:Prompts>/WebView2Loader.dll"
    COMMENT "Copying WebView2Loader.dll..."
)

set_target_properties(Prompts PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)
