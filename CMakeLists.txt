cmake_minimum_required(VERSION 3.17)
project(img2wav C)

set(CMAKE_C_STANDARD 11)

if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

add_subdirectory(src)
add_subdirectory(tests)