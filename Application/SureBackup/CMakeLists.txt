cmake_minimum_required(VERSION 3.10)

project(SureBackup)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CORE_LOGIC
    src/BackupEngine.cpp
    src/Strategies/BackupUtils.cpp
    src/Strategies/StandardBackupStrategy.cpp
    src/Strategies/ParallelBackupStrategy.cpp
    src/Strategies/ComparingBackupStrategy.cpp
)

set(HEADER_FILES
    src/BackupEngine.h
    src/Configuration.h
    src/Localization.h
    src/Strategies/BackupUtils.h
    src/Strategies/IBackupStrategy.h
    src/Strategies/ParallelBackupStrategy.h
    src/Strategies/StandardBackupStrategy.h
    src/Strategies/ComparingBackupStrategy.h
    src/Strategies/Types.h
    src/resources/resource.h
    src/resources/resource.rc
)

add_executable(SureBackup WIN32 src/main.cpp ${CORE_LOGIC} ${HEADER_FILES})

target_link_libraries(SureBackup
    comctl32
    shlwapi
    mpr
    ole32
    uuid
)

if(MSVC)
    target_compile_options(SureBackup PRIVATE /W4 /EHsc /utf-8)
    target_compile_definitions(SureBackup PRIVATE UNICODE _UNICODE NOMINMAX)
endif()
