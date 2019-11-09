INCLUDE(PkgBuildConfig)
INCLUDE(LinkDependency)

# This variable will contain the libraries we need to put into
# libgit2.pc's Requires.private. That is, what we're linking to or
# what someone who's statically linking us needs to link to.
SET(LIBGIT2_PC_REQUIRES "")
# This will be set later if we use the system's http-parser library or
# use iconv (OSX) and will be written to the Libs.private field in the
# pc file.
SET(LIBGIT2_PC_LIBS "")

###
### Setup of library targets
###

# This is a workaround for build systems which do not allow
# creation of targets without any source files.
FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/dummy.c "")
SET(DUMMY ${CMAKE_CURRENT_BINARY_DIR}/dummy.c)

ADD_LIBRARY(git2internal OBJECT ${DUMMY})
SET_TARGET_PROPERTIES(git2internal PROPERTIES C_STANDARD 90)

ADD_LIBRARY(git2 ${DUMMY} $<TARGET_OBJECTS:git2internal>)
SET_TARGET_PROPERTIES(git2 PROPERTIES C_STANDARD 90)
SET_TARGET_PROPERTIES(git2 PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${libgit2_BINARY_DIR})
SET_TARGET_PROPERTIES(git2 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${libgit2_BINARY_DIR})
SET_TARGET_PROPERTIES(git2 PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${libgit2_BINARY_DIR})

###
### Source files
###

FILE(GLOB LIBGIT2_SOURCES
	"${libgit2_SOURCE_DIR}/include/git2.h"
	"${libgit2_SOURCE_DIR}/include/git2/*.h"
	"${libgit2_SOURCE_DIR}/include/git2/sys/*.h"
	"${libgit2_BINARY_DIR}/include/git2/sys/features.h"
	"${libgit2_BINARY_DIR}/include/git2/version.h"
	*.c *.h
	allocators/*.c allocators/*.h
	streams/*.c streams/*.h
	transports/*.c transports/*.h
	xdiff/*.c xdiff/*.h)

# On Windows use specific platform sources
IF(WIN32 AND NOT CYGWIN)
	IF(MSVC)
		TARGET_SOURCES(git2 PRIVATE win32/git2.rc)
		# the xdiff dependency is not (yet) warning-free, disable warnings as
		# errors for the xdiff sources until we've sorted them out
		SET_SOURCE_FILES_PROPERTIES(xdiff/xdiffi.c PROPERTIES COMPILE_FLAGS -WX-)
		SET_SOURCE_FILES_PROPERTIES(xdiff/xutils.c PROPERTIES COMPILE_FLAGS -WX-)
		# Precompiled headers
		SET_TARGET_PROPERTIES(git2internal PROPERTIES COMPILE_FLAGS "/Yuprecompiled.h /FIprecompiled.h")
		SET_SOURCE_FILES_PROPERTIES(win32/precompiled.c COMPILE_FLAGS "/Ycprecompiled.h")
	ENDIF()

	FILE(GLOB LIBGIT2_PLATFORM_SOURCES win32/*.c win32/*.h)
ELSE()
	FILE(GLOB LIBGIT2_PLATFORM_SOURCES unix/*.c unix/*.h)
ENDIF()

TARGET_SOURCES(git2internal PRIVATE ${LIBGIT2_SOURCES} ${LIBGIT2_PLATFORM_SOURCES})
IDE_SPLIT_SOURCES(git2internal)
IDE_SPLIT_SOURCES(git2)

###
### Check for various features and platform quirks
###

IF(SONAME)
	IF(LIBGIT2_FILENAME)
		ADD_DEFINITIONS(-DLIBGIT2_FILENAME=\"${LIBGIT2_FILENAME}\")
		SET_TARGET_PROPERTIES(git2 PROPERTIES OUTPUT_NAME ${LIBGIT2_FILENAME})
	ELSEIF(DEFINED LIBGIT2_PREFIX)
		SET_TARGET_PROPERTIES(git2 PROPERTIES PREFIX "${LIBGIT2_PREFIX}")
	ENDIF()
ENDIF()

IF(DEBUG_POOL)
	SET(GIT_DEBUG_POOL 1)
ENDIF()
ADD_FEATURE_INFO(debugpool GIT_DEBUG_POOL "debug pool allocator")

IF(ENABLE_TRACE STREQUAL "ON")
	SET(GIT_TRACE 1)
ENDIF()
ADD_FEATURE_INFO(tracing GIT_TRACE "tracing support")

CHECK_FUNCTION_EXISTS(futimens HAVE_FUTIMENS)
IF(HAVE_FUTIMENS)
	SET(GIT_USE_FUTIMENS 1)
ENDIF ()

CHECK_PROTOTYPE_DEFINITION(qsort_r
	"void qsort_r(void *base, size_t nmemb, size_t size, void *thunk, int (*compar)(void *, const void *, const void *))"
	"" "stdlib.h" HAVE_QSORT_R_BSD)
IF(HAVE_QSORT_R_BSD)
	ADD_DEFINITIONS(-DHAVE_QSORT_R_BSD)
ENDIF()

CHECK_PROTOTYPE_DEFINITION(qsort_r
	"void qsort_r(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *, void *), void *arg)"
	"" "stdlib.h" HAVE_QSORT_R_GNU)
IF(HAVE_QSORT_R_GNU)
	ADD_DEFINITIONS(-DHAVE_QSORT_R_GNU)
ENDIF()

CHECK_FUNCTION_EXISTS(qsort_s HAVE_QSORT_S)
IF(HAVE_QSORT_S)
	ADD_DEFINITIONS(-DHAVE_QSORT_S)
ENDIF ()

IF(USE_NSEC)
	SET(GIT_USE_NSEC 1)
ENDIF()

IF(HAVE_STRUCT_STAT_ST_MTIM)
	SET(GIT_USE_STAT_MTIM 1)
ELSEIF(HAVE_STRUCT_STAT_ST_MTIMESPEC)
	SET(GIT_USE_STAT_MTIMESPEC 1)
ELSEIF(HAVE_STRUCT_STAT_ST_MTIME_NSEC)
	SET(GIT_USE_STAT_MTIME_NSEC 1)
ENDIF()

ADD_DEFINITIONS(-D_FILE_OFFSET_BITS=64)

IF(AMIGA)
	ADD_DEFINITIONS(-DNO_ADDRINFO -DNO_READDIR_R -DNO_MMAP)
ENDIF()

IF(VALGRIND)
	ADD_DEFINITIONS(-DNO_MMAP)
ENDIF()

# Determine architecture of the machine
IF (CMAKE_SIZEOF_VOID_P EQUAL 8)
	SET(GIT_ARCH_64 1)
ELSEIF (CMAKE_SIZEOF_VOID_P EQUAL 4)
	SET(GIT_ARCH_32 1)
ELSEIF (CMAKE_SIZEOF_VOID_P)
	MESSAGE(FATAL_ERROR "Unsupported architecture (pointer size is ${CMAKE_SIZEOF_VOID_P} bytes)")
ELSE()
	MESSAGE(FATAL_ERROR "Unsupported architecture (CMAKE_SIZEOF_VOID_P is unset)")
ENDIF()

IF(THREADSAFE)
	IF(NOT WIN32)
		FIND_PACKAGE(Threads REQUIRED)
	ENDIF()
	SET(GIT_THREADS 1)
ENDIF()
ADD_FEATURE_INFO(threadsafe THREADSAFE "threadsafe support")

###
### Detect and link against dependencies
###

IF(WIN32)
	TARGET_LINK_LIBRARIES(git2 PUBLIC ws2_32)
ELSEIF(CMAKE_SYSTEM_NAME MATCHES "(Solaris|SunOS)")
	TARGET_LINK_LIBRARIES(git2 PUBLIC socket nsl)
	LIST(APPEND LIBGIT2_PC_LIBS -lsocket -lnsl)
ELSEIF(CMAKE_SYSTEM_NAME MATCHES "Haiku")
	TARGET_LINK_LIBRARIES(git2 PUBLIC network)
	LIST(APPEND LIBGIT2_PC_LIBS -lnetwork)
ENDIF()

CHECK_LIBRARY_EXISTS(rt clock_gettime "time.h" NEED_LIBRT)
IF(NEED_LIBRT)
	TARGET_LINK_LIBRARIES(git2 PUBLIC rt)
	LIST(APPEND LIBGIT2_PC_LIBS -lrt)
ENDIF()

IF(THREADSAFE)
	TARGET_LINK_LIBRARIES(git2 PUBLIC ${CMAKE_THREAD_LIBS_INIT})
	LIST(APPEND LIBGIT2_PC_LIBS ${CMAKE_THREAD_LIBS_INIT})
ENDIF()

LINK_DEPENDENCY(git2 PUBLIC WinHTTP)
LINK_DEPENDENCY(git2 PUBLIC HttpsBackend)
LINK_DEPENDENCY(git2 PUBLIC HashBackend)
LINK_DEPENDENCY(git2 PUBLIC RegexBackend)
LINK_DEPENDENCY(git2 PUBLIC HttpParser)
LINK_DEPENDENCY(git2 PUBLIC Zlib)
LINK_DEPENDENCY(git2 PUBLIC SshBackend)
LINK_DEPENDENCY(git2 PUBLIC NtlmClient)
LINK_DEPENDENCY(git2 PUBLIC GssBackend)
LINK_DEPENDENCY(git2 PUBLIC Iconv)

CONFIGURE_FILE(features.h.in ${libgit2_BINARY_DIR}/include/git2/sys/features.h)
CONFIGURE_FILE(version.h.in  ${libgit2_BINARY_DIR}/include/git2/version.h)

TARGET_INCLUDE_DIRECTORIES(git2 PUBLIC "${libgit2_SOURCE_DIR}/include"
				       "${libgit2_BINARY_DIR}/include")
# This is required as CMake v3.5 isn't yet able to link object
# libraries directly. This can be fixed as soon as we enforce
# CMake v3.13 as a minimum, in which case we'd link all library
# dependencies against git2internal and then do a final link of
# git2internal into git2.
TARGET_INCLUDE_DIRECTORIES(git2internal PUBLIC "${libgit2_SOURCE_DIR}/src"
					       $<TARGET_PROPERTY:git2,INCLUDE_DIRECTORIES>)

###
### Install
###

PKG_BUILD_CONFIG(NAME libgit2
	VERSION ${PROJECT_VERSION}
	DESCRIPTION "The git library, take 2"
	LIBS_SELF git2
	PRIVATE_LIBS ${LIBGIT2_PC_LIBS}
	REQUIRES ${LIBGIT2_PC_REQUIRES}
)

INSTALL(TARGETS git2
	RUNTIME DESTINATION ${BIN_INSTALL_DIR}
	LIBRARY DESTINATION ${LIB_INSTALL_DIR}
	ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
)
INSTALL(DIRECTORY ${libgit2_SOURCE_DIR}/include/git2 DESTINATION ${INCLUDE_INSTALL_DIR})
INSTALL(DIRECTORY ${libgit2_BINARY_DIR}/include/git2 DESTINATION ${INCLUDE_INSTALL_DIR})
INSTALL(FILES ${libgit2_SOURCE_DIR}/include/git2.h DESTINATION ${INCLUDE_INSTALL_DIR})
