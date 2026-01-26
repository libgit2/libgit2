# util: the unit tests for libgit2's utility library

if(NOT "${CMAKE_VERSION}" VERSION_LESS 3.27)
        cmake_policy(SET CMP0148 OLD)
endif()

set(Python_ADDITIONAL_VERSIONS 3 2.7)
find_package(PythonInterp)

if(NOT PYTHONINTERP_FOUND)
        message(FATAL_ERROR "Could not find a python interpeter, which is needed to build the tests. "
                             "Make sure python is available, or pass -DBUILD_TESTS=OFF to skip building the tests")
endif()

set(CLAR_PATH "${PROJECT_SOURCE_DIR}/deps/clar")
set(BENCHMARK_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
add_definitions(-DCLAR_TMPDIR=\"libgit2_bench\")
add_definitions(-DCLAR_WIN32_LONGPATHS)
add_definitions(-DCLAR_HAS_REALPATH)
add_definitions(-D_FILE_OFFSET_BITS=64)

file(GLOB BENCHMARK_SRC *.c *.h)
list(SORT BENCHMARK_SRC)

set(CLAR_SRC
	"${CLAR_PATH}/clar.c"
	"${CLAR_PATH}/clar.h"
	"${CLAR_PATH}/clar/fixtures.h"
	"${CLAR_PATH}/clar/print.h"
	"${CLAR_PATH}/clar/summary.h"
	"${CLAR_PATH}/clar/sandbox.h"
	"${CLAR_PATH}/clar/fs.h")

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/clar.suite ${CMAKE_CURRENT_BINARY_DIR}/clar_suite.h
	COMMAND ${PYTHON_EXECUTABLE} ${CLAR_PATH}/generate.py -p benchmark -o "${CMAKE_CURRENT_BINARY_DIR}" -f .
	DEPENDS ${BENCHMARK_SRC}
	WORKING_DIRECTORY ${BENCHMARK_PATH}
)

set_source_files_properties(
	${CLAR_PATH}/clar.c
	PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/clar.suite)

add_executable(libgit2_benchmarks ${CLAR_SRC}
	${BENCHMARK_SRC}
	$<TARGET_OBJECTS:util>
	${LIBGIT2_DEPENDENCY_OBJECTS})

target_link_libraries(libgit2_benchmarks libgit2package ${LIBGIT2_SYSTEM_LIBS})
if(NOT MSVC_IDE)
	target_link_libraries(libgit2_benchmarks m)
endif()

ide_split_sources(libgit2_benchmarks)

target_include_directories(libgit2_benchmarks PRIVATE
        "${CLAR_PATH}"
        "${libgit2_BINARY_DIR}/src/util"
        "${libgit2_BINARY_DIR}/include"
        "${libgit2_SOURCE_DIR}/src/util"
        "${libgit2_SOURCE_DIR}/include"
	"${CMAKE_CURRENT_BINARY_DIR}"
        "${LIBGIT2_DEPENDENCY_INCLUDES}"
        "${LIBGIT2_SYSTEM_INCLUDES}")

#
# Old versions of gcc require us to declare our test functions; don't do
# this on newer compilers to avoid unnecessary recompilation.
#
if(CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 6.0)
	target_compile_options(libgit2_benchmarks PRIVATE -include "clar_suite.h")
endif()

if(MSVC_IDE)
	set_target_properties(libgit2_benchmarks PROPERTIES COMPILE_FLAGS "/Yuprecompiled.h /FIprecompiled.h")
	set_source_files_properties("precompiled.c" COMPILE_FLAGS "/Ycprecompiled.h")
endif()
