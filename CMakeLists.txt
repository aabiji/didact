cmake_minimum_required(VERSION 3.28)
project(didact)
include(FetchContent)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SDL3
FetchContent_Declare(
    sdl3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG main
    FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(sdl3)

# Imgui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY "https://github.com/ocornut/imgui.git"
    GIT_TAG master
)
FetchContent_GetProperties(imgui)

if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

add_library(imgui
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui.h
    ${imgui_SOURCE_DIR}/imconfig.h
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_internal.h
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imstb_rectpack.h
    ${imgui_SOURCE_DIR}/imstb_textedit.h
    ${imgui_SOURCE_DIR}/imstb_truetype.h
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.h
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.h
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)
target_link_libraries(imgui SDL3::SDL3)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})
FetchContent_MakeAvailable(imgui)

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

# WhisperCPP
FetchContent_Declare(
    whispercpp
    GIT_REPOSITORY https://github.com/ggml-org/whisper.cpp.git
    GIT_TAG master
)
FetchContent_MakeAvailable(whispercpp)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/analyzer.cpp
    src/audio.cpp
)

target_link_libraries(
    ${PROJECT_NAME}
    SDL3::SDL3
    miniaudio
    renamenoise
    whisper
    imgui
)

target_include_directories(${PROJECT_NAME} PUBLIC ${imgui_SOURCE_DIR})
