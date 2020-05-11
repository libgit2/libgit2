find_package(PythonInterp)

if(NOT PYTHONINTERP_FOUND)
	message(FATAL_ERROR "Could not find a python interpeter, which is needed to build the tests. "
	                    "Make sure python is available, or pass -DBUILD_CLAR=OFF to skip building the tests")
endif()

get_property(LIBGIT2_OBJECTS GLOBAL PROPERTY libgit2_objects)
get_property(LIBGIT2_INCLUDES GLOBAL PROPERTY libgit2_includes)
get_property(LIBGIT2_SYSTEM_INCLUDES GLOBAL PROPERTY libgit2_system_includes)
get_property(LIBGIT2_LIBS GLOBAL PROPERTY libgit2_libs)

set(CLAR_SOURCE_DIR "${libgit2_SOURCE_DIR}/tests/clar")
set(CLAR_FIXTURES "${libgit2_SOURCE_DIR}/tests/resources/")
add_definitions(-DCLAR_FIXTURE_PATH=\"${CLAR_FIXTURES}\")
add_definitions(-DCLAR_TMPDIR=\"libgit2_tests\")
add_definitions(-D_FILE_OFFSET_BITS=64)

# Ensure that we do not use deprecated functions internally
add_definitions(-DGIT_DEPRECATE_HARD)

include_directories(${CLAR_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${libgit2_BINARY_DIR}/src)
file(GLOB_RECURSE SRC_CASES */*.c */*.h)
list(SORT SRC_CASES)

file(GLOB_RECURSE SRC_CLAR "${CLAR_SOURCE_DIR}/*.c" "${CLAR_SOURCE_DIR}/*.h")
list(SORT SRC_CLAR)

if(MSVC_IDE)
	list(APPEND SRC_CLAR "precompiled.c")
endif()

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/clar.suite
	COMMAND ${PYTHON_EXECUTABLE} "${CLAR_SOURCE_DIR}/generate.py" -o "${CMAKE_CURRENT_BINARY_DIR}" -f -xonline -xstress -xperf .
	DEPENDS ${SRC_CASES}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set_source_files_properties(
	"${CLAR_SOURCE_DIR}/clar.c"
	PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/clar.suite)

include_directories(${LIBGIT2_INCLUDES})
include_directories(SYSTEM ${LIBGIT2_SYSTEM_INCLUDES})

add_executable(libgit2_tests global.c ${SRC_CLAR} ${SRC_CASES} ${LIBGIT2_OBJECTS})

set_target_properties(libgit2_tests PROPERTIES C_STANDARD 90)
set_target_properties(libgit2_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${libgit2_BINARY_DIR})

target_include_directories(libgit2_tests PRIVATE "${libgit2_SOURCE_DIR}/src/util" "${libgit2_SOURCE_DIR}/src/libgit2" PUBLIC "${libgit2_SOURCE_DIR}/include")
target_link_libraries(libgit2_tests ${LIBGIT2_LIBS})
ide_split_sources(libgit2_tests)

if(MSVC_IDE)
	# Precompiled headers
	set_target_properties(libgit2_tests PROPERTIES COMPILE_FLAGS "/Yuprecompiled.h /FIprecompiled.h")
	set_source_files_properties("precompiled.c" COMPILE_FLAGS "/Ycprecompiled.h")
endif()

function(add_clar_test name)
	if(NOT USE_LEAK_CHECKER STREQUAL "OFF")
		add_test(${name} "${libgit2_SOURCE_DIR}/script/${USE_LEAK_CHECKER}.sh" "${libgit2_BINARY_DIR}/libgit2_tests" ${ARGN})
	else()
		add_test(${name} "${libgit2_BINARY_DIR}/libgit2_tests" ${ARGN})
	endif()
endfunction(add_clar_test)

add_clar_test(offline             -v -xonline)
add_clar_test(invasive            -v -score::ftruncate -sfilter::stream::bigfile -sodb::largefiles -siterator::workdir::filesystem_gunk -srepo::init -srepo::init::at_filesystem_root)
add_clar_test(online              -v -sonline)
add_clar_test(gitdaemon           -v -sonline::push)
add_clar_test(ssh                 -v -sonline::push -sonline::clone::ssh_cert -sonline::clone::ssh_with_paths -sonline::clone::path_whitespace_ssh)
add_clar_test(proxy               -v -sonline::clone::proxy)
add_clar_test(auth_clone          -v -sonline::clone::cred)
add_clar_test(auth_clone_and_push -v -sonline::clone::push -sonline::push)
