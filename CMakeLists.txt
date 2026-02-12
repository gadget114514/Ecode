cmake_minimum_required(VERSION 3.10)
project(Ecode LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define Unicode for Win32
add_definitions(-DUNICODE -D_UNICODE)

# Include directories
include_directories(include)

# Source files
file(GLOB SOURCES "src/*.cpp")
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/test_piecetable.cpp")
set(DUKTAPE_SOURCES "src-duktape/duktape.c")
set(RESOURCES "src/Ecode.rc")

# Executable
add_executable(ecode WIN32 ${SOURCES} ${DUKTAPE_SOURCES} ${RESOURCES})

# Test executable
add_executable(test_piecetable 
    src/test_piecetable.cpp 
    src/PieceTable.cpp
)

# Libraries to link
target_link_libraries(ecode 
    user32 
    gdi32 
    shell32 
    ole32 
    comdlg32
    comctl32
    d2d1 
    dwrite
)

# Output directory
set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set_target_properties(ecode PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")

# Installer
find_program(ISCC_EXECUTABLE ISCC 
    PATHS "C:/Program Files (x86)/Inno Setup 6"
)

if(ISCC_EXECUTABLE)
    message(STATUS "Found Inno Setup compiler: ${ISCC_EXECUTABLE}")
    add_custom_target(installer
        COMMAND ${ISCC_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/installer/ecode.iss"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Building Inno Setup installer..."
        VERBATIM
    )
    add_dependencies(installer ecode)
else()
    message(WARNING "Inno Setup compiler (ISCC.exe) not found. Installer target will not be available.")
endif()

# Build instructions for the user
# mkdir build
# cd build
# cmake ..
# cmake --build . --target installer
