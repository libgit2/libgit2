# CMake build script for the libgit2 project
#
# Building (out of source build):
# > mkdir build && cd build
# > cmake .. [-DSETTINGS=VALUE]
# > cmake --build .
#
# Testing:
# > ctest -V
#
# Install:
# > cmake --build . --target install

PROJECT(libgit2 C)
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

FILE(STRINGS "include/git2/version.h" GIT2_HEADER REGEX "^#define LIBGIT2_VERSION \"[^\"]*\"$")

STRING(REGEX REPLACE "^.*LIBGIT2_VERSION \"([0-9]+).*$" "\\1" LIBGIT2_VERSION_MAJOR "${GIT2_HEADER}")
STRING(REGEX REPLACE "^.*LIBGIT2_VERSION \"[0-9]+\\.([0-9]+).*$" "\\1" LIBGIT2_VERSION_MINOR  "${GIT2_HEADER}")
STRING(REGEX REPLACE "^.*LIBGIT2_VERSION \"[0-9]+\\.[0-9]+\\.([0-9]+).*$" "\\1" LIBGIT2_VERSION_REV "${GIT2_HEADER}")
SET(LIBGIT2_VERSION_STRING "${LIBGIT2_VERSION_MAJOR}.${LIBGIT2_VERSION_MINOR}.${LIBGIT2_VERSION_REV}")

IF (AMIGA)
	# Default AmigaOS to use the PowerPC SHA1
	SET(SHA1_TYPE "ppc")
ENDIF()

# Find required dependencies
INCLUDE_DIRECTORIES(src include deps/http-parser)

IF (WIN32 AND NOT MINGW)
	ADD_DEFINITIONS(-DGIT_WINHTTP)
ELSE ()
	FIND_PACKAGE(OpenSSL)
	FILE(GLOB SRC_HTTP deps/http-parser/*.c)
ENDIF()

# Specify sha1 implementation
IF (SHA1_TYPE STREQUAL "ppc")
	ADD_DEFINITIONS(-DPPC_SHA1)
	FILE(GLOB SRC_SHA1 src/ppc/*.c src/ppc/*.S)
ELSEIF (OPENSSL_FOUND) # libcrypto's implementation is faster than ours
	ADD_DEFINITIONS(-DOPENSSL_SHA)
ELSE ()
	FILE(GLOB SRC_SHA1 src/sha1/*.c)
ENDIF()

IF (NOT WIN32)
	FIND_PACKAGE(ZLIB)
	IF (CMAKE_SYSTEM_NAME STREQUAL "AmigaOS")
		INCLUDE_DIRECTORIES(deps/regex)
		SET(SRC_REGEX deps/regex/regex.c)
	ENDIF()
ELSE()
	# Windows doesn't understand POSIX regex on its own
	INCLUDE_DIRECTORIES(deps/regex)
	SET(SRC_REGEX deps/regex/regex.c)
ENDIF()

IF (ZLIB_FOUND)
	INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIRS})
	LINK_LIBRARIES(${ZLIB_LIBRARIES})
ELSE (ZLIB_FOUND)
	INCLUDE_DIRECTORIES(deps/zlib)
	ADD_DEFINITIONS(-DNO_VIZ -DSTDC -DNO_GZIP)
	FILE(GLOB SRC_ZLIB deps/zlib/*.c)
ENDIF()

# Installation paths
SET(BIN_INSTALL_DIR bin CACHE PATH "Where to install binaries to.")
SET(LIB_INSTALL_DIR lib CACHE PATH "Where to install libraries to.")
SET(INCLUDE_INSTALL_DIR include CACHE PATH "Where to install headers to.")

# Build options
OPTION (BUILD_SHARED_LIBS "Build Shared Library (OFF for Static)" ON)
OPTION (THREADSAFE "Build libgit2 as threadsafe" OFF)
OPTION (BUILD_CLAR "Build Tests using the Clar suite" ON)
OPTION (BUILD_EXAMPLES "Build library usage example apps" OFF)
OPTION (TAGS "Generate tags" OFF)
OPTION (PROFILE "Generate profiling information" OFF)

# Platform specific compilation flags
IF (MSVC)
	# Default to stdcall, as that's what the CLR expects and how the Windows API is built
	OPTION (STDCALL "Buildl libgit2 with the __stdcall convention" ON)

	SET(CMAKE_C_FLAGS "/W4 /MP /nologo /Zi ${CMAKE_C_FLAGS}")
	IF (STDCALL)
	  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Gz")
	ENDIF ()
	SET(CMAKE_C_FLAGS_DEBUG "/Od /DEBUG /MTd /RTC1 /RTCs /RTCu")
	SET(CMAKE_C_FLAGS_RELEASE "/MT /O2")
	SET(WIN_RC "src/win32/git2.rc")

   # Precompiled headers
ELSE ()
	SET(CMAKE_C_FLAGS_DEBUG "-O0 -g ${CMAKE_C_FLAGS}")
	SET(CMAKE_C_FLAGS "-O2 -g -D_GNU_SOURCE -Wall -Wextra -Wno-missing-field-initializers -Wstrict-aliasing=2 -Wstrict-prototypes -Wmissing-prototypes ${CMAKE_C_FLAGS}")
	IF (MINGW) # MinGW always does PIC and complains if we tell it to
		STRING(REGEX REPLACE "-fPIC" "" CMAKE_SHARED_LIBRARY_C_FLAGS "${CMAKE_SHARED_LIBRARY_C_FLAGS}")
	ELSE ()
		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden -fPIC")
	ENDIF ()
	IF (PROFILE)
		SET(CMAKE_C_FLAGS "-pg ${CMAKE_C_FLAGS}")
		SET(CMAKE_EXE_LINKER_FLAGS "-pg ${CMAKE_EXE_LINKER_FLAGS}")
	ENDIF ()
ENDIF()

# Build Debug by default
IF (NOT CMAKE_BUILD_TYPE)
	SET(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
ENDIF ()

IF (OPENSSL_FOUND)
  ADD_DEFINITIONS(-DGIT_SSL)
  INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})
  SET(SSL_LIBRARIES ${OPENSSL_LIBRARIES})
ENDIF()

IF (THREADSAFE)
	IF (NOT WIN32)
		find_package(Threads REQUIRED)
	ENDIF()

	ADD_DEFINITIONS(-DGIT_THREADS)
ENDIF()

ADD_DEFINITIONS(-D_FILE_OFFSET_BITS=64)

# Collect sourcefiles
FILE(GLOB SRC_H include/git2/*.h)

# On Windows use specific platform sources
IF (WIN32 AND NOT CYGWIN)
	ADD_DEFINITIONS(-DWIN32 -D_DEBUG -D_WIN32_WINNT=0x0501)
	FILE(GLOB SRC src/*.c src/transports/*.c src/xdiff/*.c src/win32/*.c src/compat/*.c)
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "(Solaris|SunOS)")
	FILE(GLOB SRC src/*.c src/transports/*.c src/xdiff/*.c src/unix/*.c src/compat/*.c)
ELSEIF (AMIGA)
	ADD_DEFINITIONS(-DNO_ADDRINFO -DNO_READDIR_R)
	FILE(GLOB SRC src/*.c src/transports/*.c src/xdiff/*.c src/amiga/*.c src/compat/*.c)
ELSE()
	FILE(GLOB SRC src/*.c src/transports/*.c src/xdiff/*.c src/unix/*.c)
ENDIF ()

# Compile and link libgit2
ADD_LIBRARY(git2 ${SRC} ${SRC_ZLIB} ${SRC_HTTP} ${SRC_REGEX} ${SRC_SHA1} ${WIN_RC})

IF (WIN32)
	TARGET_LINK_LIBRARIES(git2 ws2_32)
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "(Solaris|SunOS)")
	TARGET_LINK_LIBRARIES(git2 socket nsl)
ENDIF ()

TARGET_LINK_LIBRARIES(git2 ${CMAKE_THREAD_LIBS_INIT} ${SSL_LIBRARIES})
SET_TARGET_PROPERTIES(git2 PROPERTIES VERSION ${LIBGIT2_VERSION_STRING})
SET_TARGET_PROPERTIES(git2 PROPERTIES SOVERSION ${LIBGIT2_VERSION_MAJOR})
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/libgit2.pc.in ${CMAKE_CURRENT_BINARY_DIR}/libgit2.pc @ONLY)

IF (MSVC)
   # Precompiled headers
   SET_TARGET_PROPERTIES(git2 PROPERTIES COMPILE_FLAGS "/Yuprecompiled.h /FIprecompiled.h")
   SET_SOURCE_FILES_PROPERTIES(src/win32/precompiled.c COMPILE_FLAGS "/Ycprecompiled.h")
ENDIF ()

# Install
INSTALL(TARGETS git2
	RUNTIME DESTINATION ${BIN_INSTALL_DIR}
	LIBRARY DESTINATION ${LIB_INSTALL_DIR}
	ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/libgit2.pc DESTINATION ${LIB_INSTALL_DIR}/pkgconfig )
INSTALL(DIRECTORY include/git2 DESTINATION ${INCLUDE_INSTALL_DIR} )
INSTALL(FILES include/git2.h DESTINATION ${INCLUDE_INSTALL_DIR} )

# Tests
IF (BUILD_CLAR)

	FIND_PACKAGE(PythonInterp REQUIRED)

	SET(CLAR_FIXTURES "${CMAKE_CURRENT_SOURCE_DIR}/tests-clar/resources/")
	SET(CLAR_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tests-clar")
	SET(CLAR_RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests-clar/resources" CACHE PATH "Path to test resources.")
	ADD_DEFINITIONS(-DCLAR_FIXTURE_PATH=\"${CLAR_FIXTURES}\")
	ADD_DEFINITIONS(-DCLAR_RESOURCES=\"${TEST_RESOURCES}\")

	INCLUDE_DIRECTORIES(${CLAR_PATH})
	FILE(GLOB_RECURSE SRC_TEST ${CLAR_PATH}/*/*.c ${CLAR_PATH}/clar_helpers.c ${CLAR_PATH}/testlib.c)

	ADD_CUSTOM_COMMAND(
		OUTPUT ${CLAR_PATH}/clar_main.c ${CLAR_PATH}/clar.h
		COMMAND ${PYTHON_EXECUTABLE} clar -vtap .
		DEPENDS ${CLAR_PATH}/clar ${SRC_TEST}
		WORKING_DIRECTORY ${CLAR_PATH}
	)
	ADD_EXECUTABLE(libgit2_clar ${SRC} ${CLAR_PATH}/clar_main.c ${SRC_TEST} ${SRC_ZLIB} ${SRC_HTTP} ${SRC_REGEX} ${SRC_SHA1})
	TARGET_LINK_LIBRARIES(libgit2_clar ${CMAKE_THREAD_LIBS_INIT} ${SSL_LIBRARIES})

        IF (MSVC)
           # Precompiled headers
           SET_TARGET_PROPERTIES(libgit2_clar PROPERTIES COMPILE_FLAGS "/Yuprecompiled.h /FIprecompiled.h")
        ENDIF ()

	IF (WIN32)
		TARGET_LINK_LIBRARIES(libgit2_clar ws2_32)
	ELSEIF (CMAKE_SYSTEM_NAME MATCHES "(Solaris|SunOS)")
		TARGET_LINK_LIBRARIES(libgit2_clar socket nsl)
	ENDIF ()

	ENABLE_TESTING()
	ADD_TEST(libgit2_clar libgit2_clar -iall)
ENDIF ()

IF (TAGS)
	FIND_PROGRAM(CTAGS ctags)
	IF (NOT CTAGS)
		message(FATAL_ERROR "Could not find ctags command")
	ENDIF ()

	FILE(GLOB_RECURSE SRC_ALL *.[ch])

	ADD_CUSTOM_COMMAND(
		OUTPUT tags
		COMMAND ${CTAGS} -a ${SRC_ALL}
		DEPENDS ${SRC_ALL}
	)
	ADD_CUSTOM_TARGET(
		do_tags ALL
		DEPENDS tags
	)
ENDIF ()

IF (BUILD_EXAMPLES)
	FILE(GLOB_RECURSE EXAMPLE_SRC examples/network/*.c)
	ADD_EXECUTABLE(cgit2 ${EXAMPLE_SRC})
	TARGET_LINK_LIBRARIES(cgit2 git2 pthread)

	ADD_EXECUTABLE(git-diff examples/diff.c)
	TARGET_LINK_LIBRARIES(git-diff git2)

	ADD_EXECUTABLE(git-general examples/general.c)
	TARGET_LINK_LIBRARIES(git-general git2)

	ADD_EXECUTABLE(git-showindex examples/showindex.c)
	TARGET_LINK_LIBRARIES(git-showindex git2)
ENDIF ()
