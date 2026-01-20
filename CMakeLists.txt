cmake_minimum_required(VERSION 3.16)

project(
  hello-wgpu-native
  VERSION 0.1.0
  LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- WGPU Native ----
set(WGPU_NATIVE_ROOT
    ""
    CACHE PATH "Install prefix containing include/ and lib/")

find_path(
  WGPU_INCLUDE_DIR
  NAMES webgpu.h webgpu/webgpu.h
  PATHS "${WGPU_NATIVE_ROOT}/include"
        # Homebrew (Apple Silicon + Intel)
        /opt/homebrew/opt/wgpu-native/include
        /opt/homebrew/include
        /usr/local/opt/wgpu-native/include
        /usr/local/include
        # Linux common
        /usr/include
        /usr/local/include
        "${WGPU_NATIVE_ROOT}"
  PATH_SUFFIXES include)

find_library(
  WGPU_LIBRARY
  NAMES wgpu_native wgpu-native
  PATHS "${WGPU_NATIVE_ROOT}/lib"
        # Homebrew (Apple Silicon + Intel)
        /opt/homebrew/opt/wgpu-native/lib
        /opt/homebrew/lib
        /usr/local/opt/wgpu-native/lib
        /usr/local/lib
        # Linux common
        /usr/lib
        /usr/local/lib
        # multiarch
        /usr/lib/x86_64-linux-gnu
        /usr/lib/aarch64-linux-gnu
        /lib/x86_64-linux-gnu
        /lib/aarch64-linux-gnu
  PATH_SUFFIXES lib)

if(NOT WGPU_INCLUDE_DIR OR NOT WGPU_LIBRARY)
  message(
    FATAL_ERROR
      "wgpu-native not found.\n"
      "Install it (e.g. brew install wgpu-native) or set -DWGPU_NATIVE_ROOT=/path\n"
      "Searched for headers (webgpu.h / webgpu/webgpu.h) and libraries (wgpu_native / wgpu-native)."
  )
endif()

message(STATUS "WGPU include dir: ${WGPU_INCLUDE_DIR}")
message(STATUS "WGPU library:     ${WGPU_LIBRARY}")

# ---- Shim for deps that hardcode: #include <webgpu/webgpu.h> ----
set(WEBGPU_SHIM_DIR "${CMAKE_BINARY_DIR}/webgpu-shim")
file(MAKE_DIRECTORY "${WEBGPU_SHIM_DIR}/webgpu")
file(WRITE "${WEBGPU_SHIM_DIR}/webgpu/webgpu.h"
     "#pragma once\n#include <webgpu.h>\n")

# Provide a small INTERFACE target that carries include dirs so any library
# (like sdl3webgpu) can see the shim + real headers.
add_library(webgpu-headers INTERFACE)
target_include_directories(webgpu-headers INTERFACE "${WEBGPU_SHIM_DIR}"
                                                    "${WGPU_INCLUDE_DIR}")

# Wrap wgpu-native as a single imported target
if(NOT TARGET WGPU::wgpu)
  add_library(WGPU::wgpu UNKNOWN IMPORTED)
  set_target_properties(WGPU::wgpu PROPERTIES IMPORTED_LOCATION
                                              "${WGPU_LIBRARY}")
endif()

# Make WGPU::wgpu bring headers along transitively too
target_link_libraries(WGPU::wgpu INTERFACE webgpu-headers)

if(NOT TARGET webgpu)
  add_library(webgpu INTERFACE)
  target_link_libraries(webgpu INTERFACE WGPU::wgpu)
endif()

# ---- SDL3 ----
find_package(SDL3 CONFIG REQUIRED)

if(NOT SDL3)
  message(FATAL_ERROR "SDL3 not found.")
endif()

# ---- sdl3webgpu ----
add_subdirectory(sdl3webgpu)

# Ensure sdl3webgpu can see <webgpu/webgpu.h> (because it includes it while
# compiling itself)
target_link_libraries(sdl3webgpu PUBLIC webgpu-headers webgpu)

# ---- App ----
add_executable(hello-wgpu-native src/main.c)

target_link_libraries(hello-wgpu-native PRIVATE WGPU::wgpu SDL3::SDL3
                                                sdl3webgpu)
