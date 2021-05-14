macro(add_unit_test test_name test_file)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} oha_unity m)
    target_compile_options(${test_name} PRIVATE ${PROJECT_COMPILE_OPTIONS})
    add_test(NAME ${test_name}
            COMMAND ${test_name}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endmacro()


# build unity
set(MODULE_UNITY_PATH ${CMAKE_CURRENT_SOURCE_DIR}/Unity)
add_library(${LIBNAME}_unity STATIC ${MODULE_UNITY_PATH}/src/unity.c)
target_include_directories(${LIBNAME}_unity PUBLIC ${MODULE_UNITY_PATH}/src/)
target_compile_options(${LIBNAME}_unity PRIVATE ${PROJECT_COMPILE_OPTIONS})

# targets with shared lib
if(${BUILD_SHARED_LIB})

    message(STATUS "Added tests for shared lib build.")

    # add tests
    add_unit_test(lpht_tests_shared lpht_tests.c)
    target_link_libraries(lpht_tests_shared ${LIBNAME})

    # benchmark
    add_executable(benchmark_shared benchmark.cpp)
    target_link_libraries(benchmark_shared ${LIBNAME})

endif()

add_unit_test(utils utils.c)

#inline lib test
add_unit_test(lpht_tests_fast lpht_tests_ho.c)

# static lib test
add_unit_test(lpht_tests_static lpht_tests.c)
target_link_libraries(lpht_tests_static ${LIBNAME}_static)

add_unit_test(tpht_tests_static tpht_tests.c)
target_link_libraries(tpht_tests_static ${LIBNAME}_static)

add_unit_test(binary_heap_test_static binary_heap_test.c)
target_link_libraries(binary_heap_test_static ${LIBNAME}_static)

# benchmark
add_executable(benchmark_static benchmark.cpp)
target_link_libraries(benchmark_static ${LIBNAME}_static)

add_executable(benchmark_static_inline EXCLUDE_FROM_ALL benchmark.cpp)
target_compile_definitions(benchmark_static_inline PRIVATE -DOHA_INLINE_ALL -DOHA_DISABLE_NULL_POINTER_CHECKS)
target_compile_options(benchmark_static_inline PRIVATE -fpermissive)
target_include_directories(benchmark_static_inline PRIVATE ${PROJECT_SOURCE_DIR})
