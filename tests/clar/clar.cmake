# clar: a test framework for c

if(NOT CLAR_PATH)
	set(CLAR_PATH "${PROJECT_SOURCE_DIR}/tests/clar")
endif()

set(CLAR_SRC
	"${CLAR_PATH}/clar.c"
	"${CLAR_PATH}/clar.h"
	"${CLAR_PATH}/clar_libgit2.c"
	"${CLAR_PATH}/clar_libgit2.h"
	"${CLAR_PATH}/clar_libgit2_alloc.c"
	"${CLAR_PATH}/clar_libgit2_alloc.h"
	"${CLAR_PATH}/clar_libgit2_timer.c"
	"${CLAR_PATH}/clar_libgit2_timer.h"
	"${CLAR_PATH}/clar_libgit2_trace.c"
	"${CLAR_PATH}/clar_libgit2_trace.h"
	"${CLAR_PATH}/main.c")

if(NOT "${CMAKE_VERSION}" VERSION_LESS 3.27)
        cmake_policy(SET CMP0148 OLD)
endif()

set(Python_ADDITIONAL_VERSIONS 3 2.7)
find_package(PythonInterp)

if(NOT PYTHONINTERP_FOUND)
	message(FATAL_ERROR "Could not find a python interpreter, which is needed to build the tests. "
	                     "Make sure python is available, or pass -DBUILD_TESTS=OFF to skip building the tests")
endif()

if(NOT CLAR_FIXTURES)
	set(CLAR_FIXTURES "${PROJECT_SOURCE_DIR}/tests/resources/")
endif()

if(NOT CLAR_TMPDIR)
	set(CLAR_TMPDIR libgit2_tests)
endif()

add_definitions(-DCLAR_FIXTURE_PATH=\"${CLAR_FIXTURES}\")
add_definitions(-DCLAR_TMPDIR=\"${CLAR_TMPDIR}\")
add_definitions(-DCLAR_WIN32_LONGPATHS)
add_definitions(-D_FILE_OFFSET_BITS=64)

function(ADD_CLAR_TEST project name)
        if(NOT USE_LEAK_CHECKER STREQUAL "OFF")
                add_test(${name} "${PROJECT_SOURCE_DIR}/script/${USE_LEAK_CHECKER}.sh" "${PROJECT_BINARY_DIR}/${project}" ${ARGN})
        else()
                add_test(${name} "${PROJECT_BINARY_DIR}/${project}" ${ARGN})
        endif()
endfunction()

function(GENERATE_CLAR_SUITE project sources flags)
	if(NOT CLAR_PATH)
		set(CLAR_PATH "${PROJECT_SOURCE_DIR}/tests/clar")
	endif()

	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/clar.suite ${CMAKE_CURRENT_BINARY_DIR}/clar_suite.h
		COMMAND ${PYTHON_EXECUTABLE} ${CLAR_PATH}/generate.py -o "${CMAKE_CURRENT_BINARY_DIR}" ${flags} -f .
		DEPENDS ${sources}
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

	set_source_files_properties(
		${CLAR_PATH}/clar.c
		PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/clar.suite)

	#
	# Old versions of gcc require us to declare our test
	# functions; don't do this on newer compilers to avoid
	# unnecessary recompilation.
	#
	if(CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 6.0)
	        target_compile_options(${project} PRIVATE -include "clar_suite.h")
	endif()
endfunction()
