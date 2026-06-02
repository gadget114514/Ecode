cmake_minimum_required(VERSION 3.10)
project(localmsg-cli LANGUAGES CXX)

add_executable(localmsg-cli main.cpp)
target_link_libraries(localmsg-cli winhttp.lib ws2_32.lib shlwapi.lib)

# Output directory matching parent project pattern
set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../bin")
set_target_properties(localmsg-cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}/$<CONFIG>/plugins")
