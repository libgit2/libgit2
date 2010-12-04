# CMake build script for the libgit2 project
# Theres nothing wrong with the original Waf build besides dependency on Python which I lack on some systems.
# Peter Drahos 2010
PROJECT(libgit2 C)
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

# Sane defaults
OPTION (BUILD_SHARED_LIBS "Build Shared Library (OFF for Static)" ON)
OPTION (BUILD_TESTS "Build Tests" ON)
IF (NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
ENDIF ()

# Find dependencies
FIND_PACKAGE(zlib REQUIRED)
FIND_PACKAGE(OpenSSL REQUIRED)

INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR} src)

# Collect sourcefiles
FILE(GLOB SRC src/*.c)
FILE(GLOB SRC_GIT src/git/*.c)
FILE(GLOB SRC_SHA src/block-sha1/*.c)
FILE(GLOB SRC_PLAT src/unix/*.c)
FILE(GLOB SRC_H src/git/*.h)

# On Windows use specific platform sources
IF (WIN32)
    ADD_DEFINITIONS(WIN32 _DEBUG _LIB ZLIB_WINAPI)
    FILE(GLOB SRC_PLAT src/win32/*.c)
ENDIF ()

# TODO. PPC Detection for optimized sha1.

# Compile and link libgit2
ADD_LIBRARY(git2 ${SRC} ${SRC_GIT} ${SRC_SHA} ${SRC_PLAT})
TARGET_LINK_LIBRARIES(git2 ${ZLIB_LIBRARY} ${OPENSSL_CRYPTO_LIBRARIES})

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
