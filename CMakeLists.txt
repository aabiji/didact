cmake_minimum_required(VERSION 3.31)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
project(didact)
include(FetchContent)

set(RAYLIB_VERSION 5.5)
FetchContent_Declare(
    raylib
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/raysan5/raylib/archive/refs/tags/${RAYLIB_VERSION}.tar.gz
    FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(raylib)

set(MINIAUDIO_VERSION 0.11.23)
FetchContent_Declare(
    miniaudio
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/mackron/miniaudio/archive/refs/tags/${MINIAUDIO_VERSION}.tar.gz
    FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(miniaudio)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ffmepg_libav REQUIRED IMPORTED_TARGET
     libavcodec
     libavutil
     libavformat
     libswresample
)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/queue.cpp
    src/reader.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE include)
target_link_libraries(${PROJECT_NAME} raylib PkgConfig::ffmepg_libav miniaudio)
