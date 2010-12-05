# CMake build script for the libgit2 project
# Theres nothing wrong with the original Waf build besides dependency on Python which I lack on some systems.
# Peter Drahos 2010
PROJECT(libgit2 C)
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

# Find required dependencies
FIND_PACKAGE(zlib REQUIRED)

# Try finding openssl
FIND_PACKAGE(OpenSSL)
IF (OPENSSL_CRYPTO_LIBRARIES)
    SET(SHA1_TYPE "openssl" CACHE STRING "Which SHA1 implementation to use: builtin, ppc, openssl")
ELSEIF ()
    SET(SHA1_TYPE "builtin" CACHE STRING "Which SHA1 implementation to use: builtin, ppc")
ENDIF ()

# Sane defaults and options
OPTION (BUILD_SHARED_LIBS "Build Shared Library (OFF for Static)" ON)
OPTION (BUILD_TESTS "Build Tests" ON)
IF (NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
ENDIF ()


INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIR}  src)

# Collect sourcefiles
FILE(GLOB SRC src/*.c)
FILE(GLOB SRC_SHA1 src/block-sha1/*.c)
FILE(GLOB SRC_PLAT src/unix/*.c)
FILE(GLOB SRC_H src/git/*.h)

# On Windows use specific platform sources
IF (WIN32)
    ADD_DEFINITIONS(WIN32 _DEBUG _LIB ZLIB_WINAPI)
    FILE(GLOB SRC_PLAT src/win32/*.c)
ENDIF ()

# sha1 implementation
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
TARGET_LINK_LIBRARIES(git2 ${ZLIB_LIBRARY} ${LIB_SHA1})

# Install
INSTALL(TARGETS git2 
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
INSTALL(FILES ${SRC_H} DESTINATION include/git)

# Tests
IF (BUILD_TESTS)
    # TODO. The Waf build generates *.toc files for each test which complicates things.
ENDIF ()
