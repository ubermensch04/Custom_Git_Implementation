cmake_minimum_required(VERSION 3.13)

project(git-starter-cpp)

set(CMAKE_CXX_STANDARD 23) # Enable the C++23 standard
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SOURCE_FILES src/*.cpp src/*.hpp)

add_executable(git ${SOURCE_FILES})

find_package(ZLIB REQUIRED)
find_package(CURL REQUIRED)

# Link via imported targets so CMake chooses the correct platform paths.
target_link_libraries(git PRIVATE CURL::libcurl ZLIB::ZLIB)
