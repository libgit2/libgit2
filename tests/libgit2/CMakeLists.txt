# tests: the unit and integration tests for libgit2

include(../clar/clar.cmake)

set(CLAR_PATH "${PROJECT_SOURCE_DIR}/tests/clar")
set(TEST_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

# Ensure that we do not use deprecated functions internally
add_definitions(-DGIT_DEPRECATE_HARD)

set(TEST_INCLUDES "${CLAR_PATH}" "${TEST_PATH}" "${CMAKE_CURRENT_BINARY_DIR}")
file(GLOB_RECURSE TEST_LIBGIT2_SRC ${TEST_PATH}/*/*.c ${TEST_PATH}/*/*.h)

if(MSVC_IDE)
	list(APPEND TEST_LIBGIT2_SRC "precompiled.c")
endif()

add_executable(libgit2_tests ${CLAR_SRC} ${TEST_LIBGIT2_SRC} ${LIBGIT2_OBJECTS})

set_target_properties(libgit2_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
target_include_directories(libgit2_tests PRIVATE ${TEST_INCLUDES} ${LIBGIT2_INCLUDES} ${LIBGIT2_DEPENDENCY_INCLUDES})
target_include_directories(libgit2_tests SYSTEM PRIVATE ${LIBGIT2_SYSTEM_INCLUDES})
target_link_libraries(libgit2_tests ${LIBGIT2_SYSTEM_LIBS})

ide_split_sources(libgit2_tests)

generate_clar_suite(libgit2_tests "${TEST_LIBGIT2_SRC}" "-xonline -xstress -xperf")

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

add_clar_test(libgit2_tests offline             -v -xonline)
add_clar_test(libgit2_tests invasive            -v -sfilter::stream::bigfile -sodb::largefiles -siterator::workdir::filesystem_gunk -srepo::init -srepo::init::at_filesystem_root -sonline::clone::connect_timeout_default)
add_clar_test(libgit2_tests online              -v -sonline -xonline::customcert)
add_clar_test(libgit2_tests online_customcert   -v -sonline::customcert)
add_clar_test(libgit2_tests gitdaemon           -v -sonline::push)
add_clar_test(libgit2_tests gitdaemon_namespace -v -sonline::clone::namespace)
add_clar_test(libgit2_tests gitdaemon_sha256    -v -sonline::clone::sha256)
add_clar_test(libgit2_tests ssh                 -v -sonline::push -sonline::clone::ssh_cert -sonline::clone::ssh_with_paths -sonline::clone::path_whitespace_ssh -sonline::clone::ssh_auth_methods)
add_clar_test(libgit2_tests proxy               -v -sonline::clone::proxy)
add_clar_test(libgit2_tests auth_clone          -v -sonline::clone::cred)
add_clar_test(libgit2_tests auth_clone_and_push -v -sonline::clone::push -sonline::push)
