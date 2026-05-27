cmake_minimum_required(VERSION 3.10)
project(telnetd LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(telnetd telnetd.c)
target_link_libraries(telnetd ws2_32)

add_executable(conpty_test conpty_test.c)
add_executable(conpty_test2 conpty_test2.c)
add_executable(conpty_test3 conpty_test3.c)
