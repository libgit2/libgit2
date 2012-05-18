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

# Find required dependencies
INCLUDE_DIRECTORIES(src include deps/http-parser)

FILE(GLOB SRC_HTTP deps/http-parser/*.c)

IF (NOT WIN32)
	FIND_PACKAGE(ZLIB)
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
SET(INSTALL_BIN bin CACHE PATH "Where to install binaries to.")
SET(INSTALL_LIB lib CACHE PATH "Where to install libraries to.")
SET(INSTALL_INC include CACHE PATH "Where to install headers to.")

# Build options
OPTION (BUILD_SHARED_LIBS "Build Shared Library (OFF for Static)" ON)
OPTION (THREADSAFE "Build libgit2 as threadsafe" OFF)
OPTION (BUILD_CLAR "Build Tests using the Clar suite" ON)
OPTION (TAGS "Generate tags" OFF)
OPTION (PROFILE "Generate profiling information" OFF)

# Platform specific compilation flags
IF (MSVC)
	# Not using __stdcall with the CRT causes problems
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
	SET(CMAKE_C_FLAGS "-O2 -g -D_GNU_SOURCE -fvisibility=hidden -Wall -Wextra -Wno-missing-field-initializers -Wstrict-aliasing=2 -Wstrict-prototypes -Wmissing-prototypes ${CMAKE_C_FLAGS}")
	SET(CMAKE_C_FLAGS_DEBUG "-O0 -g")
	IF (NOT MINGW) # MinGW always does PIC and complains if we tell it to
		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
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
ELSE()
	FILE(GLOB SRC src/*.c src/transports/*.c src/xdiff/*.c src/unix/*.c)
ENDIF ()

# Compile and link libgit2
ADD_LIBRARY(git2 ${SRC} ${SRC_ZLIB} ${SRC_HTTP} ${SRC_REGEX} ${WIN_RC})

IF (WIN32)
	TARGET_LINK_LIBRARIES(git2 ws2_32)
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "(Solaris|SunOS)")
	TARGET_LINK_LIBRARIES(git2 socket nsl)
ENDIF ()

TARGET_LINK_LIBRARIES(git2 ${CMAKE_THREAD_LIBS_INIT})
SET_TARGET_PROPERTIES(git2 PROPERTIES VERSION ${LIBGIT2_VERSION_STRING})
SET_TARGET_PROPERTIES(git2 PROPERTIES SOVERSION ${LIBGIT2_VERSION_MAJOR})
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/libgit2.pc.in ${CMAKE_CURRENT_BINARY_DIR}/libgit2.pc @ONLY)

# Install
INSTALL(TARGETS git2
	RUNTIME DESTINATION ${INSTALL_BIN}
	LIBRARY DESTINATION ${INSTALL_LIB}
	ARCHIVE DESTINATION ${INSTALL_LIB}
)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/libgit2.pc DESTINATION ${INSTALL_LIB}/pkgconfig )
INSTALL(DIRECTORY include/git2 DESTINATION ${INSTALL_INC} )
INSTALL(FILES include/git2.h DESTINATION ${INSTALL_INC} )

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
	ADD_EXECUTABLE(libgit2_clar ${SRC} ${CLAR_PATH}/clar_main.c ${SRC_TEST} ${SRC_ZLIB} ${SRC_HTTP} ${SRC_REGEX})
	TARGET_LINK_LIBRARIES(libgit2_clar ${CMAKE_THREAD_LIBS_INIT})
	IF (WIN32)
		TARGET_LINK_LIBRARIES(libgit2_clar ws2_32)
	ELSEIF (CMAKE_SYSTEM_NAME MATCHES "(Solaris|SunOS)")
		TARGET_LINK_LIBRARIES(libgit2_clar socket nsl)
	ENDIF ()

	ENABLE_TESTING()
	ADD_TEST(libgit2_clar libgit2_clar)
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
