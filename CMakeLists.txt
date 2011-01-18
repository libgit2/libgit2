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

# Find required dependencies
FIND_PACKAGE(ZLIB REQUIRED)
INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIR} src)

# Try finding openssl
FIND_PACKAGE(OpenSSL)
IF (OPENSSL_CRYPTO_LIBRARIES)
    SET(SHA1_TYPE "openssl" CACHE STRING "Which SHA1 implementation to use: builtin, ppc, openssl")
ELSEIF ()
    SET(SHA1_TYPE "builtin" CACHE STRING "Which SHA1 implementation to use: builtin, ppc")
ENDIF ()

# Installation paths
SET(INSTALL_BIN bin CACHE PATH "Where to install binaries to.")
SET(INSTALL_LIB lib CACHE PATH "Where to install libraries to.")
SET(INSTALL_INC include CACHE PATH "Where to install headers to.")

# Build options
OPTION (BUILD_SHARED_LIBS "Build Shared Library (OFF for Static)" ON)
OPTION (BUILD_TESTS "Build Tests" ON)
OPTION (BACKTRACE "Use GCC backtrace in tests (Not available on Cygwin/MinGW)" OFF)

# Build Release by default
IF (NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
ENDIF ()

# Collect sourcefiles
FILE(GLOB SRC src/*.c)
FILE(GLOB SRC_SHA1 src/block-sha1/*.c)
FILE(GLOB SRC_PLAT src/unix/*.c)
FILE(GLOB SRC_H src/git/*.h)

# On Windows use specific platform sources
IF (WIN32 AND NOT CYGWIN)
    ADD_DEFINITIONS(-DWIN32 -D_DEBUG -D_LIB -DZLIB_WINAPI)
    FILE(GLOB SRC_PLAT src/win32/*.c)
	IF (MINGW)
		SET(PTHREAD_LIBRARY pthread)
	ENDIF ()
ENDIF ()

# When desired build with backtrace
IF (BACKTRACE)
	ADD_DEFINITIONS(-DBACKTRACE)
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
ADD_LIBRARY(git2 ${SRC} ${SRC_PLAT} ${SRC_SHA1})
SET_TARGET_PROPERTIES(git2 PROPERTIES VERSION 0.0.1 SOVERSION 0)
TARGET_LINK_LIBRARIES(git2 ${ZLIB_LIBRARY} ${LIB_SHA1} ${PTHREAD_LIBRARY})

# Install
INSTALL(TARGETS git2 
    RUNTIME DESTINATION ${INSTALL_BIN}
    LIBRARY DESTINATION ${INSTALL_LIB}
    ARCHIVE DESTINATION ${INSTALL_LIB}
)
INSTALL(DIRECTORY src/git2 DESTINATION ${INSTALL_INC} )
INSTALL(FILES src/git2.h DESTINATION ${INSTALL_INC} )

# Tests
IF (BUILD_TESTS)
    SET(TEST_RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/resources" CACHE PATH "Path to test resources.")
    ADD_DEFINITIONS(-DTEST_RESOURCES=\"${TEST_RESOURCES}\")
    
    ENABLE_TESTING()
    # Find and build all tests
    INCLUDE_DIRECTORIES(tests)
    FILE (GLOB TESTS tests/t????-*.c)

    FOREACH (TEST_SRC ${TESTS})
        # Match the source to get test name
        STRING(REGEX MATCH "(t....-.*).c$" TEST_NAME ${TEST_SRC})
        SET(TEST_NAME ${CMAKE_MATCH_1}) # This is the actual matched variable, got to love CMake macro language

        # Generate TOC by finding BEGIN_TEST entries in test source.
        FILE(READ ${TEST_SRC} TEST_CODE)
        STRING(REGEX MATCHALL "BEGIN_TEST\\([^\\)]*\\)\n" TEST_TOC ${TEST_CODE})
        FILE(WRITE tests/${TEST_NAME}.toc ${TEST_TOC}) # If desired this might be moved to the build dir
        
        # Compile
        ADD_EXECUTABLE(${TEST_NAME} tests/test_main.c tests/test_lib.c tests/test_helpers.c ${TEST_SRC})
        TARGET_LINK_LIBRARIES(${TEST_NAME} git2)
        SET_TARGET_PROPERTIES(${TEST_NAME} PROPERTIES COMPILE_DEFINITIONS TEST_TOC=\"${TEST_NAME}.toc\")
        
        # Add CTest
        ADD_TEST(${TEST_NAME} ${TEST_NAME})
    ENDFOREACH ()
ENDIF ()
