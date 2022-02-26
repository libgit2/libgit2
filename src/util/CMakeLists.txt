# util: a shared library for common utility functions for libgit2 projects

add_library(util OBJECT)
set_target_properties(util PROPERTIES C_STANDARD 90)
set_target_properties(util PROPERTIES C_EXTENSIONS OFF)

set(UTIL_INCLUDES
	"${PROJECT_BINARY_DIR}/src"
	"${PROJECT_SOURCE_DIR}/src/util"
	"${PROJECT_SOURCE_DIR}/include")

file(GLOB UTIL_SRC *.c *.h allocators/*.c allocators/*.h hash/sha1.h)
list(SORT UTIL_SRC)

#
# Platform specific sources
#

if(WIN32 AND NOT CYGWIN)
	file(GLOB UTIL_SRC_OS win32/*.c win32/*.h)
	list(SORT UTIL_SRC_OS)
elseif(NOT AMIGA)
	file(GLOB UTIL_SRC_OS unix/*.c unix/*.h)
	list(SORT UTIL_SRC_OS)
endif()

#
# Hash backend selection
#

if(USE_SHA1 STREQUAL "CollisionDetection")
	file(GLOB UTIL_SRC_HASH hash/sha1/collisiondetect.* hash/sha1/sha1dc/*)
	target_compile_definitions(util PRIVATE SHA1DC_NO_STANDARD_INCLUDES=1)
        target_compile_definitions(util PRIVATE SHA1DC_CUSTOM_INCLUDE_SHA1_C=\"git2_util.h\")
        target_compile_definitions(util PRIVATE SHA1DC_CUSTOM_INCLUDE_UBC_CHECK_C=\"git2_util.h\")
elseif(USE_SHA1 STREQUAL "OpenSSL")
	file(GLOB UTIL_SRC_HASH hash/sha1/openssl.*)
elseif(USE_SHA1 STREQUAL "CommonCrypto")
	file(GLOB UTIL_SRC_HASH hash/sha1/common_crypto.*)
elseif(USE_SHA1 STREQUAL "mbedTLS")
	file(GLOB UTIL_SRC_HASH hash/sha1/mbedtls.*)
elseif(USE_SHA1 STREQUAL "Win32")
	file(GLOB UTIL_SRC_HASH hash/sha1/win32.*)
elseif(USE_SHA1 STREQUAL "Generic")
	file(GLOB UTIL_SRC_HASH hash/sha1/generic.*)
else()
	message(FATAL_ERROR "Asked for unknown SHA1 backend: ${USE_SHA1}")
endif()

list(SORT UTIL_SRC_HASH)

#
# Build the library
#

target_sources(util PRIVATE ${UTIL_SRC} ${UTIL_SRC_OS} ${UTIL_SRC_HASH})
ide_split_sources(util)

target_include_directories(util PRIVATE ${UTIL_INCLUDES} ${LIBGIT2_DEPENDENCY_INCLUDES} PUBLIC ${libgit2_SOURCE_DIR}/include)
target_include_directories(util SYSTEM PRIVATE ${LIBGIT2_SYSTEM_INCLUDES})
