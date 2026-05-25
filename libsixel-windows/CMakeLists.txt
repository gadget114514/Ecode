cmake_minimum_required(VERSION 3.10)
project(libsixel VERSION 1.8.7 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

option(BUILD_WIC "Build WIC (Windows Imaging Component) codec DLL" OFF)

# --- libsixel static library ---
set(LIBSIXEL_SRC_DIR "${PROJECT_SOURCE_DIR}/libsixel-master/libsixel-master/src")
add_library(libsixel STATIC
    ${LIBSIXEL_SRC_DIR}/output.c
    ${LIBSIXEL_SRC_DIR}/fromsixel.c
    ${LIBSIXEL_SRC_DIR}/tosixel.c
    ${LIBSIXEL_SRC_DIR}/quant.c
    ${LIBSIXEL_SRC_DIR}/dither.c
    ${LIBSIXEL_SRC_DIR}/frame.c
    ${LIBSIXEL_SRC_DIR}/pixelformat.c
    ${LIBSIXEL_SRC_DIR}/scale.c
    ${LIBSIXEL_SRC_DIR}/chunk.c
    ${LIBSIXEL_SRC_DIR}/loader.c
    ${LIBSIXEL_SRC_DIR}/frompnm.c
    ${LIBSIXEL_SRC_DIR}/fromgif.c
    ${LIBSIXEL_SRC_DIR}/encoder.c
    ${LIBSIXEL_SRC_DIR}/decoder.c
    ${LIBSIXEL_SRC_DIR}/stb_image_write.c
    ${LIBSIXEL_SRC_DIR}/writer.c
    ${LIBSIXEL_SRC_DIR}/status.c
    ${LIBSIXEL_SRC_DIR}/malloc_stub.c
    ${LIBSIXEL_SRC_DIR}/allocator.c
    ${PROJECT_SOURCE_DIR}/tty.c
)

target_include_directories(libsixel
    PRIVATE
        ${LIBSIXEL_SRC_DIR}
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
