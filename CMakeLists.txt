cmake_minimum_required(VERSION 3.0)
project(ctri VERSION 1.0 LANGUAGES CXX C)

option(RELEASE "Whether the program should be build for release" ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED true)

if (RELEASE)
    if (MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /O2")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -s")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -s")
    endif()
else()
    if (NOT MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall")
    endif()
endif()

set(sources src/main.cpp extern/glad/glad.c)

add_executable(ctri ${sources})

find_package(glfw3 3.3 REQUIRED)
target_link_libraries(ctri PUBLIC glfw)
target_include_directories(ctri PUBLIC extern)
