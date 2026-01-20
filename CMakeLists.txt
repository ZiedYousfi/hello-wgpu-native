cmake_minimum_required(VERSION 3.16)
project(hello-wgpu-native VERSION 0.1.0)

set(CMAKE_C_STANDARD 11)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(hello-wgpu src/main.c)