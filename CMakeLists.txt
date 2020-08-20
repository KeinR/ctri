cmake_minimum_required(VERSION 3.0)
project(ctri VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall")

set(sources src/main.cpp extern/glad/glad.c)

add_executable(ctri ${sources})

find_package(glfw3 3.3 REQUIRED)
target_link_libraries(ctri PUBLIC glfw)
target_include_directories(ctri PUBLIC extern)
