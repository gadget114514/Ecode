cmake_minimum_required(VERSION 3.10)
project(FastFileSearch)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(FastFileSearch WIN32
    src/main.cpp
    src/Localization.cpp
    src/Localization.h
    src/MFTReader.cpp
    src/MFTReader.h
    src/NtfsStructs.h
    src/FatReader.cpp
    src/FatReader.h
    src/FatStructs.h
    src/exFatReader.cpp
    src/exFatReader.h
    src/exFatStructs.h
    src/resource.h
    src/FastFileSearch.rc
    src/app.manifest
)

target_link_libraries(FastFileSearch
    comctl32
    comdlg32
    shell32
    ole32
    user32
    kernel32
    advapi32
)

target_compile_definitions(FastFileSearch PRIVATE 
    UNICODE 
    _UNICODE 
    _CRT_SECURE_NO_WARNINGS
)

target_link_options(FastFileSearch PRIVATE 
    /MANIFEST:NO
)

if(MSVC)
    target_compile_options(FastFileSearch PRIVATE /W4 /EHsc /utf-8)
endif()

add_executable(test_console
    src/test_console.cpp
    src/MFTReader.cpp
    src/MFTReader.h
    src/NtfsStructs.h
    src/FatReader.cpp
    src/FatReader.h
    src/FatStructs.h
    src/exFatReader.cpp
    src/exFatReader.h
    src/exFatStructs.h
)
target_link_libraries(test_console
    kernel32
    user32
)
target_compile_definitions(test_console PRIVATE 
    UNICODE 
    _UNICODE 
    _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
    target_compile_options(test_console PRIVATE /W4 /EHsc /utf-8)
endif()
target_link_options(test_console PRIVATE 
    /MANIFEST:NO
)
