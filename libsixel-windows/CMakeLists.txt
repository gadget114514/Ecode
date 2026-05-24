cmake_minimum_required(VERSION 3.10)
project(libsixel VERSION 1.8.7 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

option(BUILD_WIC "Build WIC (Windows Imaging Component) codec DLL" OFF)

# --- libsixel static library ---
add_library(libsixel STATIC
    ${PROJECT_SOURCE_DIR}/../src/output.c
    ${PROJECT_SOURCE_DIR}/../src/fromsixel.c
    ${PROJECT_SOURCE_DIR}/../src/tosixel.c
    ${PROJECT_SOURCE_DIR}/../src/quant.c
    ${PROJECT_SOURCE_DIR}/../src/dither.c
    ${PROJECT_SOURCE_DIR}/../src/frame.c
    ${PROJECT_SOURCE_DIR}/../src/pixelformat.c
    ${PROJECT_SOURCE_DIR}/../src/scale.c
    ${PROJECT_SOURCE_DIR}/../src/chunk.c
    ${PROJECT_SOURCE_DIR}/../src/loader.c
    ${PROJECT_SOURCE_DIR}/../src/frompnm.c
    ${PROJECT_SOURCE_DIR}/../src/fromgif.c
    ${PROJECT_SOURCE_DIR}/../src/encoder.c
    ${PROJECT_SOURCE_DIR}/../src/decoder.c
    ${PROJECT_SOURCE_DIR}/../src/stb_image_write.c
    ${PROJECT_SOURCE_DIR}/../src/writer.c
    ${PROJECT_SOURCE_DIR}/../src/status.c
    ${PROJECT_SOURCE_DIR}/../src/malloc_stub.c
    ${PROJECT_SOURCE_DIR}/../src/allocator.c
    ${PROJECT_SOURCE_DIR}/tty.c
)

target_include_directories(libsixel
    PRIVATE
        ${PROJECT_SOURCE_DIR}/../src
        ${PROJECT_SOURCE_DIR}
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_definitions(libsixel PRIVATE
    $<$<CONFIG:Debug>:HAVE_DEBUG>
)

# --- sixel_wic DLL (optional) ---
if(BUILD_WIC)
    add_library(sixel_wic SHARED
        ${PROJECT_SOURCE_DIR}/../wic/wic.cc
    )
    target_include_directories(sixel_wic
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
    )
    target_link_libraries(sixel_wic
        PRIVATE
            ole32
            uuid
            windowscodecs
    )
    target_compile_definitions(sixel_wic PRIVATE
        LIBSIXEL_STATIC
        $<$<CONFIG:Debug>:HAVE_DEBUG>
    )
    set_target_properties(sixel_wic PROPERTIES
        OUTPUT_NAME "sixel_wic"
    )
endif()
