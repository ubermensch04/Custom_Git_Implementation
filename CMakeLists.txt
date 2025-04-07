cmake_minimum_required(VERSION 3.13)

project(git-starter-cpp)

set(CMAKE_CXX_STANDARD 23) # Enable the C++23 standard

file(GLOB_RECURSE SOURCE_FILES src/*.cpp src/*.hpp)

add_executable(git ${SOURCE_FILES})

# Manually specify zlib from vcpkg
set(ZLIB_ROOT "C:/vcpkg/installed/x64-windows")
set(CURL_ROOT "C:/curl")  # Assuming this is where you extracted cURL

find_package(ZLIB REQUIRED)
find_package(CURL REQUIRED)

include_directories(${ZLIB_ROOT}/include)
include_directories(${CURL_ROOT}/include)

target_link_directories(git PRIVATE ${ZLIB_ROOT}/lib ${CURL_ROOT}/lib)

# Link with explicit zlib from vcpkg and cURL
target_link_libraries(git PRIVATE CURL::libcurl ZLIB::ZLIB)
