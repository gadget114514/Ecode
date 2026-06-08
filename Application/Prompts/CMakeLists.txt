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
    src/PipelineVersionManager.cpp
    src/PipelineOptimizer.cpp
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

# Register frontend files as target dependencies (not compiled, just tracked)
file(GLOB_RECURSE PROMTS_FRONTEND_DEPS
    "${CMAKE_CURRENT_SOURCE_DIR}/frontend/*.html"
    "${CMAKE_CURRENT_SOURCE_DIR}/frontend/*.js"
    "${CMAKE_CURRENT_SOURCE_DIR}/frontend/*.css"
    "${CMAKE_CURRENT_SOURCE_DIR}/frontend/lang/*.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/frontend/wizards/*.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/frontend/lib/*.js"
)
set_source_files_properties(${PROMTS_FRONTEND_DEPS} PROPERTIES HEADER_FILE_ONLY TRUE)
target_sources(Prompts PRIVATE ${PROMTS_FRONTEND_DEPS})

# Copy frontend to output directories (source-based, always fresh)
set(PROMTS_SRC_FRONT "${CMAKE_CURRENT_SOURCE_DIR}/frontend")
set(PROMTS_ROOT_OUT "${CMAKE_CURRENT_SOURCE_DIR}/../../bin")
add_custom_command(TARGET Prompts POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROMTS_SRC_FRONT}" "$<TARGET_FILE_DIR:Prompts>/frontend"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROMTS_SRC_FRONT}" "${PROMTS_ROOT_OUT}/$<CONFIG>/plugins/frontend"
    COMMENT "Updating frontend to plugin dirs..."
)
add_custom_command(TARGET Prompts POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_CURRENT_SOURCE_DIR}/packages/wv2/build/native/x64/WebView2Loader.dll"
    "$<TARGET_FILE_DIR:Prompts>/WebView2Loader.dll"
    COMMENT "Copying WebView2Loader.dll..."
)

set_target_properties(Prompts PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/$<CONFIG>/plugins"
)

# Test executables
set(PROMPTS_TEST_COMMON_LIBS comctl32 shlwapi user32 kernel32 shell32 ole32 oleaut32)

if(MSVC)
    set(PROMPTS_TEST_COMMON_FLAGS /utf-8 /W4 /EHsc)
endif()

function(add_prompts_test name)
    add_executable(${name} EXCLUDE_FROM_ALL ${ARGN})
    if(MSVC)
        target_compile_options(${name} PRIVATE ${PROMPTS_TEST_COMMON_FLAGS})
    endif()
endfunction()

add_prompts_test(test_prompts_base64 tests/test_base64.cpp src/Base64.cpp)

add_prompts_test(test_prompts_jsonparser tests/test_jsonparser.cpp src/JsonParser.cpp)

add_prompts_test(test_prompts_localization tests/test_localization.cpp src/PromptsLocalization.cpp)

add_prompts_test(test_prompts_nodedata tests/test_nodedata.cpp src/NodeData.cpp)

add_prompts_test(test_prompts_storage tests/test_storage.cpp src/Storage.cpp src/JsonParser.cpp src/Base64.cpp src/NodeData.cpp)
target_link_libraries(test_prompts_storage PRIVATE ${PROMPTS_TEST_COMMON_LIBS})

add_prompts_test(test_prompts_pipelinerunner tests/test_pipelinerunner.cpp src/PipelineRunner.cpp src/Base64.cpp src/NodeData.cpp src/JsonParser.cpp src/AIProvider.cpp)
target_link_libraries(test_prompts_pipelinerunner PRIVATE ${PROMPTS_TEST_COMMON_LIBS} winhttp)

add_prompts_test(test_prompts_optimizer tests/test_optimizer.cpp
    src/PipelineVersionManager.cpp src/PipelineOptimizer.cpp
    src/Storage.cpp src/JsonParser.cpp src/Base64.cpp src/NodeData.cpp src/AIProvider.cpp)
target_link_libraries(test_prompts_optimizer PRIVATE ${PROMPTS_TEST_COMMON_LIBS} winhttp)

add_prompts_test(test_prompts_providers tests/test_providers.cpp src/AIProvider.cpp)
target_include_directories(test_prompts_providers PRIVATE src)
target_link_libraries(test_prompts_providers PRIVATE ${PROMPTS_TEST_COMMON_LIBS} winhttp)

add_prompts_test(test_prompts_versionmanager tests/test_pipelineversionmanager.cpp
    src/PipelineVersionManager.cpp src/PipelineOptimizer.cpp
    src/Storage.cpp src/JsonParser.cpp src/Base64.cpp src/NodeData.cpp src/AIProvider.cpp)
target_link_libraries(test_prompts_versionmanager PRIVATE ${PROMPTS_TEST_COMMON_LIBS} winhttp)

add_prompts_test(test_prompts_isolation tests/test_project_isolation.cpp src/Storage.cpp src/JsonParser.cpp src/Base64.cpp src/NodeData.cpp)
target_link_libraries(test_prompts_isolation PRIVATE ${PROMPTS_TEST_COMMON_LIBS})

add_prompts_test(test_prompts_checkpoint tests/test_checkpoint.cpp src/Storage.cpp src/JsonParser.cpp src/Base64.cpp src/NodeData.cpp src/PipelineRunner.cpp src/AIProvider.cpp)
target_link_libraries(test_prompts_checkpoint PRIVATE ${PROMPTS_TEST_COMMON_LIBS} winhttp)

add_prompts_test(test_prompts_chest tests/test_chest.cpp src/Storage.cpp src/JsonParser.cpp src/Base64.cpp src/NodeData.cpp)
target_link_libraries(test_prompts_chest PRIVATE ${PROMPTS_TEST_COMMON_LIBS})

add_prompts_test(test_prompts_stream tests/test_stream_model.cpp src/PipelineRunner.cpp src/Base64.cpp src/NodeData.cpp src/JsonParser.cpp src/AIProvider.cpp)
target_link_libraries(test_prompts_stream PRIVATE ${PROMPTS_TEST_COMMON_LIBS} winhttp)
