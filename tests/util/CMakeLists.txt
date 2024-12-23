# util: the unit tests for libgit2's utility library

include(../clar/clar.cmake)

set(CLAR_PATH "${libgit2_SOURCE_DIR}/tests/clar")
set(TEST_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

file(GLOB_RECURSE TEST_UTIL_SRC ${TEST_PATH}/*.c ${TEST_PATH}/*.h ${TEST_PATH}/*/*.c ${TEST_PATH}/*/*.h)

if(MSVC)
	list(APPEND TEST_UTIL_SRC precompiled.c precompiled.h)
endif()

# Ensure that we do not use deprecated functions internally
add_definitions(-DGIT_DEPRECATE_HARD)

set(TEST_INCLUDES "${CLAR_PATH}" "${TEST_PATH}" "${CMAKE_CURRENT_BINARY_DIR}")

add_executable(util_tests ${CLAR_SRC} ${TEST_UTIL_SRC} ${LIBGIT2_OBJECTS})

set_target_properties(util_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${libgit2_BINARY_DIR})

target_include_directories(util_tests PRIVATE ${TEST_INCLUDES} ${LIBGIT2_INCLUDES} ${LIBGIT2_DEPENDENCY_INCLUDES})
target_include_directories(util_tests SYSTEM PRIVATE ${LIBGIT2_SYSTEM_INCLUDES})
target_link_libraries(util_tests ${LIBGIT2_SYSTEM_LIBS})

ide_split_sources(util_tests)

generate_clar_suite(util_tests "${TEST_UTIL_SRC}" "")

if(MSVC)
	# Precompiled headers
	set_target_properties(util_tests PROPERTIES COMPILE_FLAGS "/Yuprecompiled.h /FIprecompiled.h")
	set_source_files_properties("precompiled.c" COMPILE_FLAGS "/Ycprecompiled.h")
endif()

enable_testing()

add_clar_test(util_tests util -v)
