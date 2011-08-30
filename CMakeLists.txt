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

FILE(STRINGS "include/git2.h" GIT2_HEADER REGEX "^#define LIBGIT2_VERSION \"[^\"]*\"$")

STRING(REGEX REPLACE "^.*LIBGIT2_VERSION \"([0-9]+).*$" "\\1" LIBGIT2_VERSION_MAJOR "${GIT2_HEADER}")
STRING(REGEX REPLACE "^.*LIBGIT2_VERSION \"[0-9]+\\.([0-9]+).*$" "\\1" LIBGIT2_VERSION_MINOR  "${GIT2_HEADER}")
STRING(REGEX REPLACE "^.*LIBGIT2_VERSION \"[0-9]+\\.[0-9]+\\.([0-9]+).*$" "\\1" LIBGIT2_VERSION_REV "${GIT2_HEADER}")
SET(LIBGIT2_VERSION_STRING "${LIBGIT2_VERSION_MAJOR}.${LIBGIT2_VERSION_MINOR}.${LIBGIT2_VERSION_REV}")

# Find required dependencies
INCLUDE_DIRECTORIES(src include)
IF (NOT WIN32)
	FIND_PACKAGE(ZLIB)
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
OPTION (BUILD_TESTS "Build Tests" ON)
OPTION (THREADSAFE "Build libgit2 as threadsafe" OFF)
OPTION (STDCALL "Buildl libgit2 with the __stdcall convention (Windows)" ON)

# Platform specific compilation flags
IF (MSVC)
	SET(CMAKE_C_FLAGS "/W4 /WX /nologo /Zi")
	IF (STDCALL)
	  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Gz")
	ENDIF ()
	# TODO: bring back /RTC1 /RTCc
	SET(CMAKE_C_FLAGS_DEBUG "/Od /DEBUG /MTd")
	SET(CMAKE_C_FLAGS_RELEASE "/MT /O2")
ELSE ()
	SET(CMAKE_C_FLAGS "-O2 -g -Wall -Wextra")
	IF (NOT MINGW) # MinGW always does PIC and complains if we tell it to
		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
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
    ADD_DEFINITIONS(-DWIN32 -D_DEBUG -D_LIB)
	FILE(GLOB SRC src/*.c src/win32/*.c)
ELSE()
	FILE(GLOB SRC src/*.c src/unix/*.c)
ENDIF ()

# Compile and link libgit2
ADD_LIBRARY(git2 ${SRC} ${SRC_ZLIB})

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
IF (BUILD_TESTS)
	SET(TEST_RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/resources" CACHE PATH "Path to test resources.")
    ADD_DEFINITIONS(-DTEST_RESOURCES=\"${TEST_RESOURCES}\")

    INCLUDE_DIRECTORIES(tests)
	FILE(GLOB SRC_TEST tests/t??-*.c)

	ADD_EXECUTABLE(libgit2_test tests/test_main.c tests/test_lib.c tests/test_helpers.c ${SRC} ${SRC_TEST} ${SRC_ZLIB})
	TARGET_LINK_LIBRARIES(libgit2_test ${CMAKE_THREAD_LIBS_INIT})
	IF (WIN32)
		TARGET_LINK_LIBRARIES(libgit2_test ws2_32)
	ELSEIF (CMAKE_SYSTEM_NAME MATCHES "(Solaris|SunOS)")
		TARGET_LINK_LIBRARIES(libgit2_test socket nsl)
	ENDIF ()

	ENABLE_TESTING()
	ADD_TEST(libgit2_test libgit2_test)
ENDIF ()
