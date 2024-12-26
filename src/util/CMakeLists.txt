# util: a shared library for common utility functions for libgit2 projects

add_library(util OBJECT)

configure_file(git2_features.h.in git2_features.h)

set(UTIL_SRC
	alloc.c alloc.h
	allocators/debugalloc.c allocators/debugalloc.h
	allocators/failalloc.c allocators/failalloc.h
	allocators/stdalloc.c allocators/stdalloc.h
	allocators/win32_leakcheck.c allocators/win32_leakcheck.h
	array.h
	assert_safe.h
	bitvec.h
	cc-compat.h
	ctype_compat.h
	date.c date.h
	errors.c errors.h
	filebuf.c filebuf.h
	fs_path.c fs_path.h
	futils.c futils.h
	git2_util.h
	hash.c hash.h
	hashmap.h
	hashmap_str.h
	integer.h
	map.h
	net.c net.h
	pool.c pool.h
	posix.c posix.h
	pqueue.c pqueue.h
	process.h
	rand.c rand.h
	regexp.c regexp.h
	runtime.c runtime.h
	sortedcache.c sortedcache.h
	staticstr.h
	str.c str.h
	strlist.c strlist.h
	strnlen.h
	thread.c thread.h
	tsort.c
	utf8.c utf8.h
	util.c util.h
	varint.c varint.h
	vector.c vector.h
	wildmatch.c wildmatch.h
	zstream.c zstream.h)

#
# Platform specific sources
#

if(WIN32 AND NOT CYGWIN)
	set(UTIL_SRC_OS
		win32/dir.c win32/dir.h
		win32/error.c win32/error.h
		win32/map.c
		win32/mingw-compat.h
		win32/msvc-compat.h
		win32/path_w32.c win32/path_w32.h
		win32/posix.h
		win32/posix_w32.c
		win32/precompiled.c win32/precompiled.h
		win32/process.c
		win32/reparse.h
		win32/thread.c win32/thread.h
		win32/utf-conv.c win32/utf-conv.h
		win32/version.h
		win32/w32_buffer.c win32/w32_buffer.h
		win32/w32_common.h
		win32/w32_leakcheck.c win32/w32_leakcheck.h
		win32/w32_util.c win32/w32_util.h
		win32/win32-compat.h)
elseif(NOT AMIGA)
	set(UTIL_SRC_OS
		unix/map.c
		unix/posix.h
		unix/process.c
		unix/pthread.h
		unix/realpath.c)
endif()

#
# Hash backend selection (SHA1)
#

if(USE_SHA1 STREQUAL "CollisionDetection")
	set(UTIL_SRC_SHA1
		hash/collisiondetect.c hash/collisiondetect.h
		hash/sha1dc/sha1.c hash/sha1dc/sha1.h
		hash/sha1dc/ubc_check.c hash/sha1dc/ubc_check.h)

	target_compile_definitions(util PRIVATE SHA1DC_NO_STANDARD_INCLUDES=1)
        target_compile_definitions(util PRIVATE SHA1DC_CUSTOM_INCLUDE_SHA1_C=\"git2_util.h\")
        target_compile_definitions(util PRIVATE SHA1DC_CUSTOM_INCLUDE_UBC_CHECK_C=\"git2_util.h\")
elseif(USE_SHA1 STREQUAL "OpenSSL" OR USE_SHA1 STREQUAL "OpenSSL-Dynamic" OR USE_SHA1 STREQUAL "OpenSSL-FIPS")
	set(UTIL_SRC_SHA1 hash/openssl.c hash/openssl.h)
	add_definitions(-DOPENSSL_API_COMPAT=0x10100000L)
elseif(USE_SHA1 STREQUAL "CommonCrypto")
	set(UTIL_SRC_SHA1 hash/common_crypto.c hash/common_crypto.h)
elseif(USE_SHA1 STREQUAL "mbedTLS")
	set(UTIL_SRC_SHA1 hash/mbedtls.c hash/mbedtls.h)
elseif(USE_SHA1 STREQUAL "Win32")
	set(UTIL_SRC_SHA1 hash/win32.c hash/win32.h)
else()
	message(FATAL_ERROR "Asked for unknown SHA1 backend: ${USE_SHA1}")
endif()

#
# Hash backend selection (SHA256)
#

if(USE_SHA256 STREQUAL "Builtin")
	set(UTIL_SRC_SHA256
		hash/builtin.c hash/builtin.h
		hash/rfc6234/sha.h hash/rfc6234/sha224-256.c)
elseif(USE_SHA256 STREQUAL "OpenSSL" OR USE_SHA256 STREQUAL "OpenSSL-Dynamic" OR USE_SHA256 STREQUAL "OpenSSL-FIPS")
	set(UTIL_SRC_SHA256 hash/openssl.c hash/openssl.h)
	add_definitions(-DOPENSSL_API_COMPAT=0x10100000L)
elseif(USE_SHA256 STREQUAL "CommonCrypto")
	set(UTIL_SRC_SHA256 hash/common_crypto.c hash/common_crypto.h)
elseif(USE_SHA256 STREQUAL "mbedTLS")
	set(UTIL_SRC_SHA256 hash/mbedtls.c hash/mbedtls.h)
elseif(USE_SHA256 STREQUAL "Win32")
	set(UTIL_SRC_SHA256 hash/win32.c hash/win32.h)
else()
	message(FATAL_ERROR "Asked for unknown SHA256 backend: ${USE_SHA256}")
endif()

#
# Build the library
#

target_sources(util PRIVATE ${UTIL_SRC} ${UTIL_SRC_OS} ${UTIL_SRC_SHA1} ${UTIL_SRC_SHA256})
ide_split_sources(util)

set(UTIL_INCLUDES
	"${PROJECT_BINARY_DIR}/src/util"
	"${PROJECT_BINARY_DIR}/include"
	"${PROJECT_SOURCE_DIR}/src/util"
	"${PROJECT_SOURCE_DIR}/include")

target_include_directories(util PRIVATE ${UTIL_INCLUDES} ${LIBGIT2_DEPENDENCY_INCLUDES} PUBLIC ${libgit2_SOURCE_DIR}/include)
target_include_directories(util SYSTEM PRIVATE ${LIBGIT2_SYSTEM_INCLUDES})
