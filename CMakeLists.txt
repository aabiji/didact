cmake_minimum_required(VERSION 3.28)
project(didact)
include(FetchContent)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=mold")
set(CMAKE_SHARED_LINKER_FLAGS "-fuse-ld=mold")

# SDL3 + SDL_Image + SDL_TTF
FetchContent_Declare(
    sdl3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG main
    FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(sdl3)

FetchContent_Declare(
    sdl_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image
    GIT_TAG main
    FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(sdl_image)

FetchContent_Declare(
    sdl_ttf
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf
    GIT_TAG main
    FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(sdl_ttf)

# miniaudio
FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG master
)
FetchContent_MakeAvailable(miniaudio)

# ReNameNoise - a fork of rnnoise
FetchContent_Declare(
    renamenoise
    GIT_REPOSITORY https://github.com/mumble-voip/ReNameNoise.git
    GIT_TAG master
)
FetchContent_MakeAvailable(renamenoise)

# sherpa-onnx
FetchContent_Declare(
    sherpa_onnx
    GIT_REPOSITORY https://github.com/k2-fsa/sherpa-onnx.git
    GIT_TAG master
)
FetchContent_MakeAvailable(sherpa_onnx)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/transcriber/analyzer.cpp
    src/transcriber/audio.cpp
    src/transcriber/transcriber.cpp
)

target_link_libraries(
    ${PROJECT_NAME}
    SDL3::SDL3
    miniaudio
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf
    renamenoise
    sherpa-onnx-cxx-api
    sherpa-onnx-c-api
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_include_directories(${PROJECT_NAME} PUBLIC
    ${sherpa_onnx_SOURCE_DIR}/sherpa-onnx/c-api
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
