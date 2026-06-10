cmake_minimum_required(VERSION 3.15)
project(raytracer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# set(CMAKE_CXX_STANDARD_LIBRARIES pthread)

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -mtune=native -flto -ffast-math")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -DDEBUG")

add_executable(raytracer
    src/main.cpp)

find_package(Threads REQUIRED)
target_link_libraries(raytracer Threads::Threads)
