set(SOURCE_FILES linear_probing_hash_table.c binary_heap.c prioritized_hash_table.c)

if(WITH_KEY_FROM_VALUE_FUNC)
	add_definitions(-DOHA_WITH_KEY_FROM_VALUE_SUPPORT)
endif()

# shared lib
if(NOT DEFINED OHA_DISABLE_BUILD_SHARED_LIB)
    add_library(${LIBNAME} SHARED ${SOURCE_FILES})
    target_compile_options(${LIBNAME} PRIVATE ${PROJECT_COMPILE_OPTIONS} -fPIC)
    target_link_libraries(${LIBNAME} PRIVATE m)
    target_include_directories(${LIBNAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)
    set(VERSION_MAJOR 0)
    set(VERSION_MINOR 0)
    set(VERSION_PATCH 0)
    set(VERSION_STRING ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})
    set_target_properties(${LIBNAME} PROPERTIES VERSION ${VERSION_STRING}
                                                SOVERSION ${VERSION_MAJOR})
    install(TARGETS ${LIBNAME}
            LIBRARY
            DESTINATION lib/${LIBNAME}
            COMPONENT lib)
endif()

# static lib
add_library(${LIBNAME}_static STATIC ${SOURCE_FILES})
target_compile_options(${LIBNAME}_static PRIVATE ${PROJECT_COMPILE_OPTIONS})
target_link_libraries(${LIBNAME}_static PRIVATE m)
target_include_directories(${LIBNAME}_static PUBLIC ${PROJECT_SOURCE_DIR}/include)
install(TARGETS ${LIBNAME}_static
        ARCHIVE
        DESTINATION lib/${LIBNAME}
        COMPONENT lib)

# static lib with fixed key size of 8 byte
add_library(${LIBNAME}_static_8 STATIC ${SOURCE_FILES})
target_compile_options(${LIBNAME}_static_8 PRIVATE ${PROJECT_COMPILE_OPTIONS})
target_link_libraries(${LIBNAME}_static_8 PRIVATE  m)
target_include_directories(${LIBNAME}_static_8 PUBLIC ${PROJECT_SOURCE_DIR}/include)
install(TARGETS ${LIBNAME}_static_8
        ARCHIVE
        DESTINATION lib/${LIBNAME}
        COMPONENT lib)
target_compile_definitions(${LIBNAME}_static_8 PRIVATE OHA_FIX_KEY_SIZE_IN_BYTES=8)

# header install command
install(FILES "${PROJECT_SOURCE_DIR}/include/oha.h"
        DESTINATION include/${LIBNAME}
        COMPONENT dev)
