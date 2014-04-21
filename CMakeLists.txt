cmake_minimum_required(VERSION 2.8)
project(PBF)

find_package(OpenGL)

if (APPLE)
    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    find_library(COCOA_LIB Cocoa)
    find_library(IOKIT_LIB IOKit)
    find_library(COREVIDEO_LIB CoreVideo)
    find_package(Glew)
endif (APPLE)

add_subdirectory(lib)
add_subdirectory(src)
