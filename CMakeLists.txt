# libgit2: the cross-platform, linkable library implementation of git.
# See `README.md` for build instructions.
#
# This top-level CMakeLists.txt sets up configuration options and
# determines which subprojects to build.

cmake_minimum_required(VERSION 3.5.1)

project(libgit2 VERSION "1.9.0" LANGUAGES C)

# Add find modules to the path
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake")

#
# Build options
#

# Experimental features
option(EXPERIMENTAL_SHA256     "Enable experimental SHA256 support (for R&D/testing)"  OFF)

# Optional subsystems
option(BUILD_SHARED_LIBS       "Build Shared Library (OFF for Static)"                  ON)
option(BUILD_TESTS             "Build Tests using the Clar suite"                       ON)
option(BUILD_CLI               "Build the command-line interface"                       ON)
option(BUILD_EXAMPLES          "Build library usage example apps"                      OFF)
option(BUILD_FUZZERS           "Build the fuzz targets"                                OFF)

# Feature enablement and backend selection
   set(USE_THREADS             "" CACHE STRING "Use threads for parallel processing when possible. One of ON, OFF, or a specific provider: pthreads or win32. (Defaults to ON.)")
   set(USE_SSH                 "" CACHE STRING "Enables SSH support and optionally selects provider. One of ON, OFF, or a specific provider: libssh2 or exec. (Defaults to OFF.)")
   set(USE_HTTPS               "" CACHE STRING "Enable HTTPS support and optionally selects the provider. One of ON, OFF, or a specific provider: OpenSSL, OpenSSL-FIPS, OpenSSL-Dynamic, mbedTLS, SecureTransport, Schannel, or WinHTTP. (Defaults to ON.)")
   set(USE_SHA1                "" CACHE STRING "Selects SHA1 provider. One of builtin, HTTPS, or a specific provider. (Defaults to builtin.)")
   set(USE_SHA256              "" CACHE STRING "Selects SHA256 provider. One of Builtin, HTTPS, or a specific provider. (Defaults to HTTPS.)")
   set(USE_HTTP_PARSER         "" CACHE STRING "Selects HTTP Parser support: http-parser, llhttp, or builtin. (Defaults to builtin.)")
   set(USE_AUTH_NTLM           "" CACHE STRING "Enables NTLM authentication support. One of Builtin or win32.")
   set(USE_AUTH_NEGOTIATE      "" CACHE STRING "Enable Negotiate (SPNEGO) authentication support. One of GSSAPI or win32.")
#  set(USE_XDIFF               "" CACHE STRING "Specifies the xdiff implementation; either system or builtin.")
   set(USE_REGEX               "" CACHE STRING "Selects regex provider. One of regcomp_l, pcre2, pcre, regcomp, or builtin.")
   set(USE_COMPRESSION         "" CACHE STRING "Selects compression backend. Either builtin or zlib.")
   set(USE_NSEC                "" CACHE STRING "Enable nanosecond precision timestamps. One of ON, OFF, or a specific provider: mtimespec, mtim, mtime, or win32. (Defaults to ON).")

if(APPLE)
	# Currently only available on macOS for `precomposeUnicode` support
	set(USE_I18N            "" CACHE STRING "Enables internationalization support.")
endif()

# Debugging options
   set(DEBUG_LEAK_CHECKER      "" CACHE STRING "Configure for leak checking test runs. One of valgrind, leaks, or win32. Either valgrind or leaks.")
option(USE_STANDALONE_FUZZERS  "Enable standalone fuzzers (compatible with gcc)"       OFF)
option(DEBUG_POOL              "Enable debug pool allocator"                           OFF)
option(DEBUG_STRICT_ALLOC      "Enable strict allocator behavior"                      OFF)
option(DEBUG_STRICT_OPEN       "Enable path validation in open"                        OFF)

# Output options
option(SONAME                  "Set the (SO)VERSION of the target"                      ON)
   set(LIBGIT2_FILENAME        "git2" CACHE STRING "Name of the produced binary")
option(DEPRECATE_HARD          "Do not include deprecated functions in the library"    OFF)

# Compilation options
# Default to c99 on Android Studio for compatibility; c90 everywhere else
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Android")
   set(CMAKE_C_STANDARD        "99" CACHE STRING "The C standard to compile against")
else()
   set(CMAKE_C_STANDARD        "90" CACHE STRING "The C standard to compile against")
endif()
option(CMAKE_C_EXTENSIONS      "Whether compiler extensions are supported"             OFF)
option(ENABLE_WERROR           "Enable compilation with -Werror"                       OFF)

if(UNIX)
	option(ENABLE_REPRODUCIBLE_BUILDS "Enable reproducible builds"                 OFF)
endif()

if(MSVC)
	# This option must match the settings used in your program, in particular if you
	# are linking statically
	option(STATIC_CRT          "Link the static CRT libraries"                      ON)
endif()

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()


# Modules

include(FeatureSummary)
include(CheckLibraryExists)
include(CheckFunctionExists)
include(CheckSymbolExists)
include(CheckStructHasMember)
include(CheckPrototypeDefinitionSafe)
include(AddCFlagIfSupported)
include(FindPkgLibraries)
include(FindThreads)
include(FindStatNsec)
include(Findfutimens)
include(GNUInstallDirs)
include(IdeSplitSources)
include(EnableWarnings)
include(DefaultCFlags)
include(ExperimentalFeatures)


#
# Subdirectories
#

add_subdirectory(src)

if(BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()

if(BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

if(BUILD_FUZZERS)
	if((BUILD_TESTS OR BUILD_EXAMPLES) AND NOT USE_STANDALONE_FUZZERS)
		message(FATAL_ERROR "Cannot build the fuzzer and the tests or examples together")
	endif()
	add_subdirectory(fuzzers)
endif()


# Export for people who use us as a dependency

if(NOT "${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
	set(LIBGIT2_DEPENDENCY_OBJECTS ${LIBGIT2_DEPENDENCY_OBJECTS} PARENT_SCOPE)
	set(LIBGIT2_SYSTEM_LIBS ${LIBGIT2_SYSTEM_LIBS} PARENT_SCOPE)
endif()


# Summary

feature_summary(WHAT ENABLED_FEATURES DESCRIPTION "Enabled features:")
feature_summary(WHAT DISABLED_FEATURES DESCRIPTION "Disabled features:")

# warn for not using sha1dc

foreach(WARNING ${WARNINGS})
	message(WARNING ${WARNING})
endforeach()
