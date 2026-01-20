cmake_minimum_required(VERSION 3.16)
project(
  hello-wgpu-native
  VERSION 0.1.0
  LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(hello-wgpu-native src/main.c)

add_subdirectory(sdl3webgpu)

set(WGPU_NATIVE_ROOT
    ""
    CACHE PATH "Install prefix containing include/ and lib/")

find_path(
  WGPU_INCLUDE_DIR
  NAMES webgpu/webgpu.h webgpu.h
  PATHS "${WGPU_NATIVE_ROOT}/include" /opt/homebrew/include /usr/local/include
        /usr/include)

find_library(
  WGPU_LIBRARY
  NAMES wgpu_native wgpu-native
  PATHS "${WGPU_NATIVE_ROOT}/lib" /opt/homebrew/lib /usr/local/lib /usr/lib)

if(NOT WGPU_INCLUDE_DIR OR NOT WGPU_LIBRARY)
  message(
    FATAL_ERROR
      "wgpu-native not found.\n"
      "Install it (e.g. brew install wgpu-native) or set -DWGPU_NATIVE_ROOT=/path"
  )
endif()

find_library(
  SDL3
  NAMES SDL3
  PATHS /opt/homebrew/lib /usr/local/lib /usr/lib)

if(NOT SDL3)
  message(FATAL_ERROR "SDL3 not found.")
endif()

target_include_directories(hello-wgpu-native PRIVATE "${WGPU_INCLUDE_DIR}")
target_link_libraries(hello-wgpu-native PRIVATE "${WGPU_LIBRARY}" "${SDL3}")
