if(DEFINED OHA_DISABLE_BUILD_SHARED_LIB)
    message(STATUS "use: 'set(OHA_DISABLE_BUILD_SHARED_LIB ON)' to enable tests and building of shared lib")
    return()
endif()

macro(add_unit_test test_name test_file)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} oha_unity)
    target_compile_options(${test_name} PRIVATE ${PROJECT_COMPILE_OPTIONS})
    add_test(NAME ${test_name}
            COMMAND ${test_name}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endmacro()


# build unity
set(MODULE_UNITY_PATH ${CMAKE_CURRENT_SOURCE_DIR}/Unity)
add_library(${LIBNAME}_unity SHARED ${MODULE_UNITY_PATH}/src/unity.c)
target_include_directories(${LIBNAME}_unity PUBLIC ${MODULE_UNITY_PATH}/src/)
target_compile_options(${LIBNAME}_unity PRIVATE ${PROJECT_COMPILE_OPTIONS})

# add tests
add_unit_test(linear_hash_table_test_shared linear_hash_table_test.c)
target_link_libraries(linear_hash_table_test_shared ${LIBNAME})

add_unit_test(linear_hash_table_test_static linear_hash_table_test.c)
target_link_libraries(linear_hash_table_test_static ${LIBNAME}_static)

# add tests
add_unit_test(linear_hash_table_test_fix_key_8 linear_hash_table_test.c)
target_link_libraries(linear_hash_table_test_fix_key_8 ${LIBNAME}_static_8)

add_unit_test(binary_heap_test_shared binary_heap_test.c)
target_link_libraries(binary_heap_test_shared ${LIBNAME})

# benchmark
add_executable(benchmark_shared benchmark.cpp)
target_link_libraries(benchmark_shared ${LIBNAME})

add_executable(benchmark_static benchmark.cpp)
target_link_libraries(benchmark_static ${LIBNAME}_static)

add_executable(benchmark_static_8 benchmark.cpp)
target_link_libraries(benchmark_static_8 ${LIBNAME}_static_8)
