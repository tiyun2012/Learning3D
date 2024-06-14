cmake_minimum_required (VERSION 3.10)

if(PLATFORM_WIN32 OR PLATFORM_LINUX OR PLATFORM_MACOS)
    add_subdirectory(Tutorial25_StatePackager)
endif()
