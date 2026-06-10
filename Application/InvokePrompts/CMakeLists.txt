cmake_minimum_required(VERSION 3.15)
project(InvokePrompts LANGUAGES NONE)

find_program(DOTNET_EXECUTABLE dotnet REQUIRED)

add_custom_target(InvokePrompts ALL
    COMMAND "${DOTNET_EXECUTABLE}" build
        "${CMAKE_CURRENT_SOURCE_DIR}/InvokePrompts.csproj"
        -c Release
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Building InvokePrompts (dotnet)"
    USES_TERMINAL
)
