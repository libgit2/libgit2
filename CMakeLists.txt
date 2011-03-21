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
INCLUDE_DIRECTORIES(deps/zlib src include)

# Try finding openssl
FIND_PACKAGE(OpenSSL)
IF (OPENSSL_CRYPTO_LIBRARIES)
    SET(SHA1_TYPE "openssl" CACHE STRING "Which SHA1 implementation to use: builtin, ppc, openssl")
ELSEIF ()
    SET(SHA1_TYPE "builtin" CACHE STRING "Which SHA1 implementation to use: builtin, ppc")
ENDIF ()

INCLUDE(FindPkgConfig)

# Show SQLite3 settings in GUI (if they won't be found out)
SET(SQLITE3_INCLUDE_DIRS "" CACHE PATH "SQLite include directory")
SET(SQLITE3_LIBRARIES "" CACHE FILEPATH "SQLite library")

# Are SQLite3 variables already set up? (poor Windows/no pkg-config/no sqlite3.pc)
IF (SQLITE3_INCLUDE_DIRS AND SQLITE3_LIBRARIES)
	SET(SQLITE3_FOUND 1)
ENDIF ()

# Try to find SQLite3 via pkg-config
IF (PKG_CONFIG_FOUND AND NOT SQLITE3_FOUND)
	pkg_check_modules(SQLITE3 sqlite3)
ENDIF ()

# Compile SQLite backend if SQLite3 is available
IF (SQLITE3_FOUND)
	ADD_DEFINITIONS(-DGIT2_SQLITE_BACKEND)
	INCLUDE_DIRECTORIES(${SQLITE3_INCLUDE_DIRS})
ENDIF ()

# Installation paths
SET(INSTALL_BIN bin CACHE PATH "Where to install binaries to.")
SET(INSTALL_LIB lib CACHE PATH "Where to install libraries to.")
SET(INSTALL_INC include CACHE PATH "Where to install headers to.")

# Build options
OPTION (BUILD_SHARED_LIBS "Build Shared Library (OFF for Static)" ON)
OPTION (BUILD_TESTS "Build Tests" ON)
OPTION (THREADSAFE "Build libgit2 as threadsafe" OFF)

# Build Release by default
IF (NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
ENDIF ()

IF (THREADSAFE)
	IF (NOT WIN32)
		find_package(Threads REQUIRED)
	ENDIF()

	ADD_DEFINITIONS(-DGIT_THREADS)
ENDIF()

# Collect sourcefiles
FILE(GLOB SRC src/*.c src/backends/*.c)
FILE(GLOB SRC_ZLIB deps/zlib/*.c)
FILE(GLOB SRC_SHA1 src/block-sha1/*.c)
FILE(GLOB SRC_PLAT src/unix/*.c)
FILE(GLOB SRC_H include/git2/*.h)

# On Windows use specific platform sources
IF (WIN32 AND NOT CYGWIN)
    ADD_DEFINITIONS(-DWIN32 -D_DEBUG -D_LIB)
    FILE(GLOB SRC_PLAT src/win32/*.c)
ENDIF ()

# Specify sha1 implementation
IF (SHA1_TYPE STREQUAL "ppc")
    ADD_DEFINITIONS(-DPPC_SHA1)
    FILE(GLOB SRC_SHA1 src/ppc/*.c)
ELSEIF (SHA1_TYPE STREQUAL "openssl")
    ADD_DEFINITIONS(-DOPENSSL_SHA1)
    SET (SRC_SHA1)
    INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})
    SET (LIB_SHA1 ${OPENSSL_CRYPTO_LIBRARIES})
ENDIF ()

# Compile and link libgit2
ADD_LIBRARY(git2 ${SRC} ${SRC_PLAT} ${SRC_SHA1} ${SRC_ZLIB})
TARGET_LINK_LIBRARIES(git2 ${LIB_SHA1} ${CMAKE_THREAD_LIBS_INIT} ${SQLITE3_LIBRARIES})
SET_TARGET_PROPERTIES(git2 PROPERTIES VERSION ${LIBGIT2_VERSION_STRING})
SET_TARGET_PROPERTIES(git2 PROPERTIES SOVERSION ${LIBGIT2_VERSION_MAJOR})

# Install
INSTALL(TARGETS git2 
    RUNTIME DESTINATION ${INSTALL_BIN}
    LIBRARY DESTINATION ${INSTALL_LIB}
    ARCHIVE DESTINATION ${INSTALL_LIB}
)
INSTALL(DIRECTORY include/git2 DESTINATION ${INSTALL_INC} )
INSTALL(FILES include/git2.h DESTINATION ${INSTALL_INC} )

# Tests
IF (BUILD_TESTS)
    SET(TEST_RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/resources" CACHE PATH "Path to test resources.")
    ADD_DEFINITIONS(-DTEST_RESOURCES=\"${TEST_RESOURCES}\")
    
    ENABLE_TESTING()
    INCLUDE_DIRECTORIES(tests)

	FILE(GLOB SRC_TEST tests/t??-*.c)

	ADD_EXECUTABLE(libgit2_test tests/test_main.c tests/test_lib.c tests/test_helpers.c ${SRC} ${SRC_PLAT} ${SRC_SHA1} ${SRC_TEST} ${SRC_ZLIB})
	TARGET_LINK_LIBRARIES(libgit2_test ${LIB_SHA1} ${CMAKE_THREAD_LIBS_INIT} ${SQLITE3_LIBRARIES})

	ADD_TEST(libgit2_test libgit2_test)
ENDIF ()
