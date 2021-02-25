cmake_minimum_required (VERSION 3.0)

project(oha C CXX)

if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE "Release" CACHE
      STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo" "ASAN" "Coverage")
endif()

# CMAKE_BUILD_TYPE=ASAN
set(CMAKE_C_FLAGS_ASAN "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer -O0")
set(CMAKE_EXE_LINKER_FLAGS_ASAN "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_SHARED_LINKER_FLAGS_ASAN "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer")

# CMAKE_BUILD_TYPE=Coverage
set(CMAKE_C_FLAGS_COVERAGE "${CMAKE_C_FLAGS_DEBUG} --coverage -O0")
set(CMAKE_EXE_LINKER_FLAGS_COVERAGE "${CMAKE_EXE_LINKER_FLAGS_DEBUG} --coverage")
set(CMAKE_SHARED_LINKER_FLAGS_COVERAGE "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} --coverage")

option(BUILD_SHARED_LIB "enabled shared libaray build" ON)
message(STATUS "Build shared lib: ${BUILD_SHARED_LIB}")
option(WITH_KEY_FROM_VALUE_FUNC "enabled the function: 'oha_lpht_get_key_from_value()'" OFF)
if(WITH_KEY_FROM_VALUE_FUNC)
    set(OHA_COMPILE_DEFINITIONS -DOHA_WITH_KEY_FROM_VALUE_SUPPORT)
endif()

# global vaiables
set(LIBNAME "oha")
set(PROJECT_COMPILE_OPTIONS -std=c11 -Wall -Wextra -Wpedantic)

set(OHA_LINK_LIBS m)
set(SOURCE_FILES    linear_probing_hash_table.c
                    binary_heap.c
                    temporal_prioritized_hash_table.c)

# compile object files
add_library(${LIBNAME}_obj OBJECT ${SOURCE_FILES})
target_compile_options(${LIBNAME}_obj PUBLIC ${PROJECT_COMPILE_OPTIONS} -fPIC)
target_compile_definitions(${LIBNAME}_obj PRIVATE ${OHA_COMPILE_DEFINITIONS})

# link shared lib
if(${BUILD_SHARED_LIB})
    add_library(${LIBNAME} SHARED $<TARGET_OBJECTS:${LIBNAME}_obj>)
    set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/oha.symbols")
    set(LINK_OPTIONS -s -Wl,--gc-sections -Wl,--strip-all -Wl,--version-script=${LINKER_SCRIPT})
    set_target_properties(${LIBNAME} PROPERTIES LINK_DEPENDS ${LINKER_SCRIPT})
    target_link_libraries(${LIBNAME} PUBLIC ${OHA_LINK_LIBS} ${LINK_OPTIONS})
    target_include_directories(${LIBNAME} PUBLIC ${PROJECT_SOURCE_DIR})
    set(VERSION_MAJOR 0)
    set(VERSION_MINOR 1)
    set(VERSION_PATCH 0)
    set(VERSION_STRING ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})
    set_target_properties(${LIBNAME} PROPERTIES VERSION ${VERSION_STRING}
                                                SOVERSION ${VERSION_MAJOR})
    install(TARGETS ${LIBNAME}
            LIBRARY
            DESTINATION ${LIBNAME}
            COMPONENT lib)
endif()

# archive static lib
add_library(${LIBNAME}_static STATIC $<TARGET_OBJECTS:${LIBNAME}_obj>)
target_link_libraries(${LIBNAME}_static PUBLIC ${OHA_LINK_LIBS})
target_include_directories(${LIBNAME}_static PUBLIC ${PROJECT_SOURCE_DIR})
install(TARGETS ${LIBNAME}_static
        ARCHIVE
        DESTINATION ${LIBNAME}
        COMPONENT lib)

# header install command
install(FILES "${PROJECT_SOURCE_DIR}/oha.h"
        DESTINATION ${LIBNAME}
        COMPONENT dev)

include(CTest)
add_subdirectory(test)
