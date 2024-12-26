# util: the unit tests for libgit2's utility library

add_executable(util_tests)

include(../clar/clar.cmake)

set(TEST_UTIL_SRC
	alloc.c
	array.c
	assert.c
	bitvec.c
	copy.c
	crlf.h
	dirent.c
	encoding.c
	errors.c
	filebuf.c
	ftruncate.c
	futils.c
	gitstr.c
	hashmap.c
	hex.c
	hostname.c
	iconv.c
	init.c
	integer.c
	link.c
	memmem.c
	mkdir.c
	path/core.c
	path/win32.c
	pool.c
	posix.c
	pqueue.c
	precompiled.c
	precompiled.h
	process/env.c
	process/start.c
	process/win32.c
	qsort.c
	regexp.c
	rmdir.c
	sha1.c
	sha256.c
	sortedcache.c
	stat.c
	str/basic.c
	str/oom.c
	str/percent.c
	str/quote.c
	str/splice.c
	string.c
	strtol.c
	url/http.c
	url/joinpath.c
	url/parse.c
	url/pattern.c
	url/redirect.c
	url/scp.c
	url/valid.c
	utf8.c
	vector.c
	wildmatch.c
	zstream.c)

if(MSVC)
	list(APPEND TEST_UTIL_SRC precompiled.c precompiled.h)
endif()

set(CLAR_PATH "${libgit2_SOURCE_DIR}/tests/clar")
set(TEST_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

# Ensure that we do not use deprecated functions internally
add_definitions(-DGIT_DEPRECATE_HARD)

set(TEST_INCLUDES "${CLAR_PATH}" "${TEST_PATH}" "${CMAKE_CURRENT_BINARY_DIR}")

generate_clar_suite(util_tests "${TEST_UTIL_SRC}" "")

target_sources(util_tests PRIVATE ${CLAR_SRC} ${TEST_UTIL_SRC} ${LIBGIT2_OBJECTS})

set_target_properties(util_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${libgit2_BINARY_DIR})

target_include_directories(util_tests PRIVATE ${TEST_INCLUDES} ${LIBGIT2_INCLUDES} ${LIBGIT2_DEPENDENCY_INCLUDES})
target_include_directories(util_tests SYSTEM PRIVATE ${LIBGIT2_SYSTEM_INCLUDES})
target_link_libraries(util_tests ${LIBGIT2_SYSTEM_LIBS})

ide_split_sources(util_tests)

if(MSVC)
	# Precompiled headers
	set_target_properties(util_tests PROPERTIES COMPILE_FLAGS "/Yuprecompiled.h /FIprecompiled.h")
	set_source_files_properties("precompiled.c" COMPILE_FLAGS "/Ycprecompiled.h")
endif()

enable_testing()

add_clar_test(util_tests util -v)
