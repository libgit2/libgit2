add_library(git2internal OBJECT)
set_target_properties(git2internal PROPERTIES C_STANDARD 90)


if(DEPRECATE_HARD)
        add_definitions(-DGIT_DEPRECATE_HARD)
endif()

if(DEBUG_POOL)
	set(GIT_DEBUG_POOL 1)
endif()
add_feature_info(debugpool GIT_DEBUG_POOL "debug pool allocator")

if(DEBUG_STRICT_ALLOC)
	set(GIT_DEBUG_STRICT_ALLOC 1)
endif()
add_feature_info(debugalloc GIT_DEBUG_STRICT_ALLOC "debug strict allocators")

if(DEBUG_STRICT_OPEN)
	set(GIT_DEBUG_STRICT_OPEN 1)
endif()
add_feature_info(debugopen GIT_DEBUG_STRICT_OPEN "path validation in open")


include(PkgBuildConfig)
include(SanitizeBool)

# This variable will contain the libraries we need to put into
# libgit2.pc's Requires.private. That is, what we're linking to or
# what someone who's statically linking us needs to link to.
set(LIBGIT2_PC_REQUIRES "")
# This will be set later if we use the system's http-parser library or
# use iconv (OSX) and will be written to the Libs.private field in the
# pc file.
set(LIBGIT2_PC_LIBS "")

set(LIBGIT2_INCLUDES
	"${CMAKE_CURRENT_BINARY_DIR}"
	"${PROJECT_SOURCE_DIR}/src"
	"${PROJECT_SOURCE_DIR}/include")

if(HAVE_FUTIMENS)
	set(GIT_USE_FUTIMENS 1)
endif ()
add_feature_info(futimens GIT_USE_FUTIMENS "futimens support")

check_prototype_definition(qsort_r
	"void qsort_r(void *base, size_t nmemb, size_t size, void *thunk, int (*compar)(void *, const void *, const void *))"
	"" "stdlib.h" GIT_QSORT_R_BSD)

check_prototype_definition(qsort_r
	"void qsort_r(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *, void *), void *arg)"
	"" "stdlib.h" GIT_QSORT_R_GNU)

check_function_exists(qsort_s GIT_QSORT_S)

# Find required dependencies

if(WIN32)
	list(APPEND LIBGIT2_SYSTEM_LIBS ws2_32)
elseif(CMAKE_SYSTEM_NAME MATCHES "(Solaris|SunOS)")
	list(APPEND LIBGIT2_SYSTEM_LIBS socket nsl)
	list(APPEND LIBGIT2_PC_LIBS "-lsocket" "-lnsl")
elseif(CMAKE_SYSTEM_NAME MATCHES "Haiku")
	list(APPEND LIBGIT2_SYSTEM_LIBS network)
	list(APPEND LIBGIT2_PC_LIBS "-lnetwork")
endif()

check_library_exists(rt clock_gettime "time.h" NEED_LIBRT)
if(NEED_LIBRT)
	list(APPEND LIBGIT2_SYSTEM_LIBS rt)
	list(APPEND LIBGIT2_PC_LIBS "-lrt")
endif()

if(USE_THREADS)
	list(APPEND LIBGIT2_SYSTEM_LIBS ${CMAKE_THREAD_LIBS_INIT})
	list(APPEND LIBGIT2_PC_LIBS ${CMAKE_THREAD_LIBS_INIT})
endif()
add_feature_info(threadsafe USE_THREADS "threadsafe support")


if(WIN32 AND EMBED_SSH_PATH)
	file(GLOB SRC_SSH "${EMBED_SSH_PATH}/src/*.c")
	list(SORT SRC_SSH)
	target_sources(git2internal PRIVATE ${SRC_SSH})

	list(APPEND LIBGIT2_SYSTEM_INCLUDES "${EMBED_SSH_PATH}/include")
	file(WRITE "${EMBED_SSH_PATH}/src/libssh2_config.h" "#define HAVE_WINCNG\n#define LIBSSH2_WINCNG\n#include \"../win32/libssh2_config.h\"")
	set(GIT_SSH 1)
endif()

include(SelectHTTPSBackend)
include(SelectHashes)
include(SelectHTTPParser)
include(SelectRegex)
include(SelectSSH)
include(SelectWinHTTP)
include(SelectZlib)


if(USE_SHA1 STREQUAL "CollisionDetection")
	file(GLOB SRC_SHA1 hash/sha1/collisiondetect.* hash/sha1/sha1dc/*)
elseif(USE_SHA1 STREQUAL "OpenSSL")
	file(GLOB SRC_SHA1 hash/sha1/openssl.*)
elseif(USE_SHA1 STREQUAL "CommonCrypto")
	file(GLOB SRC_SHA1 hash/sha1/common_crypto.*)
elseif(USE_SHA1 STREQUAL "mbedTLS")
	file(GLOB SRC_SHA1 hash/sha1/mbedtls.*)
elseif(USE_SHA1 STREQUAL "Win32")
	file(GLOB SRC_SHA1 hash/sha1/win32.*)
elseif(USE_SHA1 STREQUAL "Generic")
	file(GLOB SRC_SHA1 hash/sha1/generic.*)
endif()
list(APPEND SRC_SHA1 "hash/sha1.h")
target_sources(git2internal PRIVATE ${SRC_SHA1})

# Optional external dependency: ntlmclient
if(USE_NTLMCLIENT)
	set(GIT_NTLM 1)
	add_subdirectory("${PROJECT_SOURCE_DIR}/deps/ntlmclient" "${PROJECT_BINARY_DIR}/deps/ntlmclient")
	list(APPEND LIBGIT2_DEPENDENCY_INCLUDES "${PROJECT_SOURCE_DIR}/deps/ntlmclient")
	list(APPEND LIBGIT2_DEPENDENCY_OBJECTS "$<TARGET_OBJECTS:ntlmclient>")
endif()
add_feature_info(ntlmclient GIT_NTLM "NTLM authentication support for Unix")

# Optional external dependency: GSSAPI

include(SelectGSSAPI)

# Optional external dependency: iconv
if(USE_ICONV)
	find_package(Iconv)
endif()
if(ICONV_FOUND)
	set(GIT_USE_ICONV 1)
	list(APPEND LIBGIT2_SYSTEM_INCLUDES ${ICONV_INCLUDE_DIR})
	list(APPEND LIBGIT2_SYSTEM_LIBS ${ICONV_LIBRARIES})
	list(APPEND LIBGIT2_PC_LIBS ${ICONV_LIBRARIES})
endif()
add_feature_info(iconv GIT_USE_ICONV "iconv encoding conversion support")


if(USE_THREADS)
	if(NOT WIN32)
		find_package(Threads REQUIRED)
	endif()

	set(GIT_THREADS 1)
endif()

if(USE_NSEC)
	set(GIT_USE_NSEC 1)
endif()

if(HAVE_STRUCT_STAT_ST_MTIM)
	set(GIT_USE_STAT_MTIM 1)
elseif(HAVE_STRUCT_STAT_ST_MTIMESPEC)
	set(GIT_USE_STAT_MTIMESPEC 1)
elseif(HAVE_STRUCT_STAT_ST_MTIME_NSEC)
	set(GIT_USE_STAT_MTIME_NSEC 1)
endif()

target_compile_definitions(git2internal PRIVATE _FILE_OFFSET_BITS=64)

# Collect sourcefiles
file(GLOB SRC_H
	"${PROJECT_SOURCE_DIR}/include/git2.h"
	"${PROJECT_SOURCE_DIR}/include/git2/*.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys/*.h")
list(SORT SRC_H)
target_sources(git2internal PRIVATE ${SRC_H})

# On Windows use specific platform sources
if(WIN32 AND NOT CYGWIN)
	set(WIN_RC "win32/git2.rc")

	file(GLOB SRC_OS win32/*.c win32/*.h)
	list(SORT SRC_OS)
	target_sources(git2internal PRIVATE ${SRC_OS})
elseif(AMIGA)
	target_compile_definitions(git2internal PRIVATE NO_ADDRINFO NO_READDIR_R NO_MMAP)
else()
	file(GLOB SRC_OS unix/*.c unix/*.h)
	list(SORT SRC_OS)
	target_sources(git2internal PRIVATE ${SRC_OS})
endif()

if(USE_LEAK_CHECKER STREQUAL "valgrind")
	target_compile_definitions(git2internal PRIVATE VALGRIND)
endif()

file(GLOB SRC_GIT2 *.c *.h
	allocators/*.c allocators/*.h
	streams/*.c streams/*.h
	transports/*.c transports/*.h
	xdiff/*.c xdiff/*.h)
list(SORT SRC_GIT2)
target_sources(git2internal PRIVATE ${SRC_GIT2})

if(APPLE)
	# The old Secure Transport API has been deprecated in macOS 10.15.
	set_source_files_properties(streams/stransport.c PROPERTIES COMPILE_FLAGS -Wno-deprecated)
endif()

# the xdiff dependency is not (yet) warning-free, disable warnings as
# errors for the xdiff sources until we've sorted them out
if(MSVC)
	set_source_files_properties(xdiff/xdiffi.c PROPERTIES COMPILE_FLAGS -WX-)
	set_source_files_properties(xdiff/xemit.c PROPERTIES COMPILE_FLAGS -WX-)
	set_source_files_properties(xdiff/xhistogram.c PROPERTIES COMPILE_FLAGS -WX-)
	set_source_files_properties(xdiff/xmerge.c PROPERTIES COMPILE_FLAGS -WX-)
	set_source_files_properties(xdiff/xutils.c PROPERTIES COMPILE_FLAGS -WX-)
	set_source_files_properties(xdiff/xpatience.c PROPERTIES COMPILE_FLAGS -WX-)
else()
	set_source_files_properties(xdiff/xdiffi.c PROPERTIES COMPILE_FLAGS "-Wno-sign-compare -Wno-unused-parameter")
	set_source_files_properties(xdiff/xemit.c PROPERTIES COMPILE_FLAGS "-Wno-sign-compare -Wno-unused-parameter")
	set_source_files_properties(xdiff/xhistogram.c PROPERTIES COMPILE_FLAGS "-Wno-sign-compare")
	set_source_files_properties(xdiff/xutils.c PROPERTIES COMPILE_FLAGS "-Wno-sign-compare")
	set_source_files_properties(xdiff/xpatience.c PROPERTIES COMPILE_FLAGS "-Wno-sign-compare")
endif()

# Determine architecture of the machine
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(GIT_ARCH_64 1)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
	set(GIT_ARCH_32 1)
elseif(CMAKE_SIZEOF_VOID_P)
	message(FATAL_ERROR "Unsupported architecture (pointer size is ${CMAKE_SIZEOF_VOID_P} bytes)")
else()
	message(FATAL_ERROR "Unsupported architecture (CMAKE_SIZEOF_VOID_P is unset)")
endif()

configure_file(features.h.in git2/sys/features.h)

ide_split_sources(git2internal)
list(APPEND LIBGIT2_OBJECTS $<TARGET_OBJECTS:git2internal> ${LIBGIT2_DEPENDENCY_OBJECTS})

target_include_directories(git2internal PRIVATE ${LIBGIT2_INCLUDES} ${LIBGIT2_DEPENDENCY_INCLUDES} PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_include_directories(git2internal SYSTEM PRIVATE ${LIBGIT2_SYSTEM_INCLUDES})

set(LIBGIT2_INCLUDES ${LIBGIT2_INCLUDES} PARENT_SCOPE)
set(LIBGIT2_OBJECTS ${LIBGIT2_OBJECTS} PARENT_SCOPE)
set(LIBGIT2_DEPENDENCY_INCLUDES ${LIBGIT2_DEPENDENCY_INCLUDES} PARENT_SCOPE)
set(LIBGIT2_DEPENDENCY_OBJECTS ${LIBGIT2_DEPENDENCY_OBJECTS} PARENT_SCOPE)
set(LIBGIT2_SYSTEM_INCLUDES ${LIBGIT2_SYSTEM_INCLUDES} PARENT_SCOPE)
set(LIBGIT2_SYSTEM_LIBS ${LIBGIT2_SYSTEM_LIBS} PARENT_SCOPE)

if(XCODE_VERSION)
	# This is required for Xcode to actually link the libgit2 library
	# when using only object libraries.
	file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/dummy.c "")
	list(APPEND LIBGIT2_OBJECTS ${CMAKE_CURRENT_BINARY_DIR}/dummy.c)
endif()

# Compile and link libgit2
add_library(git2 ${WIN_RC} ${LIBGIT2_OBJECTS})
target_link_libraries(git2 ${LIBGIT2_SYSTEM_LIBS})

set_target_properties(git2 PROPERTIES C_STANDARD 90)
set_target_properties(git2 PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set_target_properties(git2 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set_target_properties(git2 PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

# Workaround for Cmake bug #0011240 (see http://public.kitware.com/Bug/view.php?id=11240)
# Win64+MSVC+static libs = linker error
if(MSVC AND GIT_ARCH_64 AND NOT BUILD_SHARED_LIBS)
	set_target_properties(git2 PROPERTIES STATIC_LIBRARY_FLAGS "/MACHINE:x64")
endif()

ide_split_sources(git2)

if(SONAME)
	set_target_properties(git2 PROPERTIES VERSION ${libgit2_VERSION})
	set_target_properties(git2 PROPERTIES SOVERSION "${libgit2_VERSION_MAJOR}.${libgit2_VERSION_MINOR}")
	if(LIBGIT2_FILENAME)
		target_compile_definitions(git2 PRIVATE LIBGIT2_FILENAME=\"${LIBGIT2_FILENAME}\")
		set_target_properties(git2 PROPERTIES OUTPUT_NAME ${LIBGIT2_FILENAME})
	elseif(DEFINED LIBGIT2_PREFIX)
		set_target_properties(git2 PROPERTIES PREFIX "${LIBGIT2_PREFIX}")
	endif()
endif()

pkg_build_config(NAME libgit2
	VERSION ${libgit2_VERSION}
	DESCRIPTION "The git library, take 2"
	LIBS_SELF git2
	PRIVATE_LIBS ${LIBGIT2_PC_LIBS}
	REQUIRES ${LIBGIT2_PC_REQUIRES}
)

if(MSVC_IDE)
	# Precompiled headers
	set_target_properties(git2 PROPERTIES COMPILE_FLAGS "/Yuprecompiled.h /FIprecompiled.h")
	set_source_files_properties(win32/precompiled.c COMPILE_FLAGS "/Ycprecompiled.h")
endif()

# Install
install(TARGETS git2
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/git2 DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${PROJECT_SOURCE_DIR}/include/git2.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
