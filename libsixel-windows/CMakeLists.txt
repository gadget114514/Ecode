cmake_minimum_required(VERSION 3.10)
project(libsixel VERSION 1.8.7 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

option(BUILD_WIC "Build WIC (Windows Imaging Component) codec DLL" OFF)

set(LIBSIXEL_SRC "${PROJECT_SOURCE_DIR}/libsixel-master/libsixel-master/src")

# --- libsixel static library ---
set(LIBSIXEL_SRC_DIR "${PROJECT_SOURCE_DIR}/libsixel-master/libsixel-master/src")
add_library(libsixel STATIC
    ${LIBSIXEL_SRC}/output.c
    ${LIBSIXEL_SRC}/fromsixel.c
    ${LIBSIXEL_SRC}/tosixel.c
    ${LIBSIXEL_SRC}/quant.c
    ${LIBSIXEL_SRC}/dither.c
    ${LIBSIXEL_SRC}/frame.c
    ${LIBSIXEL_SRC}/pixelformat.c
    ${LIBSIXEL_SRC}/scale.c
    ${LIBSIXEL_SRC}/chunk.c
    ${LIBSIXEL_SRC}/loader.c
    ${LIBSIXEL_SRC}/frompnm.c
    ${LIBSIXEL_SRC}/fromgif.c
    ${LIBSIXEL_SRC}/encoder.c
    ${LIBSIXEL_SRC}/decoder.c
    ${LIBSIXEL_SRC}/stb_image_write.c
    ${LIBSIXEL_SRC}/writer.c
    ${LIBSIXEL_SRC}/status.c
    ${LIBSIXEL_SRC}/malloc_stub.c
    ${LIBSIXEL_SRC}/allocator.c
    ${PROJECT_SOURCE_DIR}/tty.c
)

target_include_directories(libsixel
    PRIVATE
        ${LIBSIXEL_SRC}
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
