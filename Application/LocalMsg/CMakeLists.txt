cmake_minimum_required(VERSION 3.10)
project(LocalMsg LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── mbedTLS
set(MBEDTLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mbedtls")
set(ENABLE_TESTING  OFF CACHE BOOL "" FORCE)
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
add_subdirectory(${MBEDTLS_DIR} mbedtls_build EXCLUDE_FROM_ALL)

# ── LocalMsg executable
add_executable(LocalMsg WIN32
    src/main.cpp
    src/manifest.rc)

set_target_properties(LocalMsg PROPERTIES
    LINK_FLAGS "/MANIFEST:NO")

target_include_directories(LocalMsg PRIVATE
    "${MBEDTLS_DIR}/include")

target_link_libraries(LocalMsg
    mbedtls
    mbedx509
    mbedcrypto
    user32
    gdi32
    comctl32
    comdlg32
    shell32
    ws2_32
    winhttp
    shlwapi
    iphlpapi
    kernel32)

target_compile_definitions(LocalMsg PRIVATE
    UNICODE
    _UNICODE
    _WIN32_WINNT=0x0A00
    _CRT_SECURE_NO_WARNINGS
    LOCALMSG_TLS)

if(MSVC)
    target_compile_options(LocalMsg PRIVATE /W4 /EHsc /utf-8)
    target_compile_options(LocalMsg PRIVATE /wd4996 /wd4244 /wd4267)
endif()

set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../bin")
set_target_properties(LocalMsg PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}/$<CONFIG>/plugins")

# ── ipmsg-cli
add_subdirectory(cli)
