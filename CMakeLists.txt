cmake_minimum_required (VERSION 3.0)

project(oha C CXX)

# global vaiables
set(LIBNAME "oha")
set(PROJECT_COMPILE_OPTIONS -O3 -std=c11 -Wall -Wextra -Wpedantic)

option(ASAN "use adress sanitizer" OFF)
if(ASAN)
    message(STATUS "Enable ASAN.")
    set(PROJECT_COMPILE_OPTIONS -fsanitize=address -fno-omit-frame-pointer ${PROJECT_COMPILE_OPTIONS})
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
endif()

option(WITH_KEY_FROM_VALUE_FUNC "enabled the function: 'oha_lpht_get_key_from_value()'" OFF)

add_subdirectory(src)
include(CTest)
add_subdirectory(test)
