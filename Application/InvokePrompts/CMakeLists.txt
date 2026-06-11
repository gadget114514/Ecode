cmake_minimum_required(VERSION 3.10)
project(InvokePrompts LANGUAGES C)

add_executable(InvokePrompts WIN32 src/main.c)

target_link_libraries(InvokePrompts shell32)

if(MSVC)
    target_compile_options(InvokePrompts PRIVATE /W3 /utf-8)
    target_compile_definitions(InvokePrompts PRIVATE UNICODE _UNICODE)
endif()

set_target_properties(InvokePrompts PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)
