cmake_minimum_required(VERSION 3.15)

project(EmbeddedToolkits)
set(CMAKE_CXX_STANDARD 20)

add_library(EmbeddedToolkits INTERFACE)
target_include_directories(EmbeddedToolkits INTERFACE include/)
