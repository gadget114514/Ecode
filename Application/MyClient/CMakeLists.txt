cmake_minimum_required(VERSION 3.10)
project(MyClient LANGUAGES C)

add_executable(myclient mytelnet.c)

target_link_libraries(myclient ws2_32 user32 kernel32)

target_compile_definitions(myclient PRIVATE
    _CRT_SECURE_NO_WARNINGS
    _WINSOCK_DEPRECATED_NO_WARNINGS
)

if(MSVC)
    target_compile_options(myclient PRIVATE /utf-8)
endif()

set_target_properties(myclient PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)
