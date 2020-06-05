find_package(PythonInterp)

if(NOT PYTHONINTERP_FOUND)
	message(FATAL_ERROR "Could not find a python interpeter, which is needed to build the tests. "
	                    "Make sure python is available, or pass -DBUILD_CLAR=OFF to skip building the tests")
endif()

get_property(LIBGIT2CLIENT_OBJECTS GLOBAL PROPERTY libgit2client_objects)
get_property(LIBGIT2CLIENT_INCLUDES GLOBAL PROPERTY libgit2client_includes)
get_property(LIBGIT2CLIENT_SYSTEM_INCLUDES GLOBAL PROPERTY libgit2client_system_includes)
get_property(LIBGIT2CLIENT_LIBS GLOBAL PROPERTY libgit2client_libs)

set(CLAR_SOURCE_DIR "${libgit2_SOURCE_DIR}/tests/clar")
set(CLAR_FIXTURES "${libgit2_SOURCE_DIR}/tests/resources/")
add_definitions(-DCLAR_FIXTURE_PATH=\"${CLAR_FIXTURES}\")
add_definitions(-DCLAR_TMPDIR=\"libgit2client_tests\")
add_definitions(-D_FILE_OFFSET_BITS=64)

# Ensure that we do not use deprecated functions internally
add_definitions(-DGIT_DEPRECATE_HARD)

include_directories(${CLAR_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${libgit2_BINARY_DIR}/src)
file(GLOB_RECURSE SRC_CASES */*.c */*.h)
list(SORT SRC_CASES)

file(GLOB_RECURSE SRC_CLAR "${CLAR_SOURCE_DIR}/*.c" "${CLAR_SOURCE_DIR}/*.h")
list(SORT SRC_CLAR)

set(SRC_LOCAL global.c)

if(MSVC_IDE)
	list(APPEND SRC_LOCAL "precompiled.c")
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

include_directories(${LIBGIT2CLIENT_INCLUDES})
include_directories(SYSTEM ${LIBGIT2CLIENT_SYSTEM_INCLUDES})

add_executable(libgit2client_tests ${SRC_LOCAL} ${SRC_CLAR} ${SRC_CASES} ${LIBGIT2CLIENT_OBJECTS})

set_target_properties(libgit2client_tests PROPERTIES C_STANDARD 90)
set_target_properties(libgit2client_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${libgit2_BINARY_DIR})

target_include_directories(libgit2client_tests PRIVATE "${libgit2_SOURCE_DIR}/src/util" "${libgit2_SOURCE_DIR}/src/libgit2" PUBLIC "${libgit2_SOURCE_DIR}/include")
target_link_libraries(libgit2client_tests libgit2_meta ${LIBGIT2CLIENT_LIBS})
ide_split_sources(libgit2client_tests)

if(MSVC_IDE)
	# Precompiled headers
	set_target_properties(libgit2client_tests PROPERTIES COMPILE_FLAGS "/Yuprecompiled.h /FIprecompiled.h")
	set_source_files_properties("precompiled.c" COMPILE_FLAGS "/Ycprecompiled.h")
endif()

add_clar_test(client -v)
