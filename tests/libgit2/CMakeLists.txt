# tests: the unit and integration tests for libgit2

set(Python_ADDITIONAL_VERSIONS 3 2.7)
find_package(PythonInterp)

if(NOT PYTHONINTERP_FOUND)
	message(FATAL_ERROR "Could not find a python interpreter, which is needed to build the tests. "
	                     "Make sure python is available, or pass -DBUILD_TESTS=OFF to skip building the tests")
ENDIF()

set(CLAR_FIXTURES "${PROJECT_SOURCE_DIR}/tests/resources/")
set(CLAR_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
add_definitions(-DCLAR_FIXTURE_PATH=\"${CLAR_FIXTURES}\")
add_definitions(-DCLAR_TMPDIR=\"libgit2_tests\")
add_definitions(-DCLAR_WIN32_LONGPATHS)
add_definitions(-D_FILE_OFFSET_BITS=64)

# Ensure that we do not use deprecated functions internally
add_definitions(-DGIT_DEPRECATE_HARD)

set(TEST_INCLUDES "${CLAR_PATH}" "${CMAKE_CURRENT_BINARY_DIR}")
file(GLOB_RECURSE SRC_TEST ${CLAR_PATH}/*/*.c ${CLAR_PATH}/*/*.h)
set(SRC_CLAR "main.c" "clar_libgit2.c" "clar_libgit2_trace.c" "clar_libgit2_timer.c" "clar.c")

if(MSVC_IDE)
	list(APPEND SRC_CLAR "precompiled.c")
endif()

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/clar.suite ${CMAKE_CURRENT_BINARY_DIR}/clar_suite.h
	COMMAND ${PYTHON_EXECUTABLE} generate.py -o "${CMAKE_CURRENT_BINARY_DIR}" -f -xonline -xstress -xperf .
	DEPENDS ${SRC_TEST}
	WORKING_DIRECTORY ${CLAR_PATH}
)

set_source_files_properties(
	${CLAR_PATH}/clar.c
	PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/clar.suite)

add_executable(libgit2_tests ${SRC_CLAR} ${SRC_TEST} ${LIBGIT2_OBJECTS})

set_target_properties(libgit2_tests PROPERTIES C_STANDARD 90)
set_target_properties(libgit2_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

target_include_directories(libgit2_tests PRIVATE ${TEST_INCLUDES} ${LIBGIT2_INCLUDES} ${LIBGIT2_DEPENDENCY_INCLUDES})
target_include_directories(libgit2_tests SYSTEM PRIVATE ${LIBGIT2_SYSTEM_INCLUDES})
target_link_libraries(libgit2_tests ${LIBGIT2_SYSTEM_LIBS})

ide_split_sources(libgit2_tests)

#
# Old versions of gcc require us to declare our test functions; don't do
# this on newer compilers to avoid unnecessary recompilation.
#
if(CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 6.0)
	target_compile_options(libgit2_tests PRIVATE -include "clar_suite.h")
endif()

if(MSVC_IDE)
	# Precompiled headers
	set_target_properties(libgit2_tests PROPERTIES COMPILE_FLAGS "/Yuprecompiled.h /FIprecompiled.h")
	set_source_files_properties("precompiled.c" COMPILE_FLAGS "/Ycprecompiled.h")
endif()

function(ADD_CLAR_TEST name)
	if(NOT USE_LEAK_CHECKER STREQUAL "OFF")
		add_test(${name} "${PROJECT_SOURCE_DIR}/script/${USE_LEAK_CHECKER}.sh" "${PROJECT_BINARY_DIR}/libgit2_tests" ${ARGN})
	else()
		add_test(${name} "${PROJECT_BINARY_DIR}/libgit2_tests" ${ARGN})
	endif()
endfunction(ADD_CLAR_TEST)

add_clar_test(offline             -v -xonline)
add_clar_test(invasive            -v -score::ftruncate -sfilter::stream::bigfile -sodb::largefiles -siterator::workdir::filesystem_gunk -srepo::init -srepo::init::at_filesystem_root)
add_clar_test(online              -v -sonline -xonline::customcert)
add_clar_test(online_customcert   -v -sonline::customcert)
add_clar_test(gitdaemon           -v -sonline::push)
add_clar_test(ssh                 -v -sonline::push -sonline::clone::ssh_cert -sonline::clone::ssh_with_paths -sonline::clone::path_whitespace_ssh)
add_clar_test(proxy               -v -sonline::clone::proxy)
add_clar_test(auth_clone          -v -sonline::clone::cred)
add_clar_test(auth_clone_and_push -v -sonline::clone::push -sonline::push)

#
# Header file validation project: ensure that we do not publish any sloppy
# definitions in our headers and that a consumer can include <git2.dll>
# even when they have aggressive C90 warnings enabled.
#

add_executable(headertest headertest.c)
set_target_properties(headertest PROPERTIES C_STANDARD 90)
set_target_properties(headertest PROPERTIES C_EXTENSIONS OFF)
target_include_directories(headertest PRIVATE ${LIBGIT2_INCLUDES})

if (MSVC)
	target_compile_options(headertest PUBLIC /W4 /WX)
else()
	target_compile_options(headertest PUBLIC -Wall -Wextra -pedantic -Werror)
endif()
