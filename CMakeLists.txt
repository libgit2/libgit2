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

# Build options
#
OPTION( BUILD_SHARED_LIBS	"Build Shared Library (OFF for Static)"	ON  )
OPTION( THREADSAFE			"Build libgit2 as threadsafe"			OFF )
OPTION( BUILD_CLAR			"Build Tests using the Clar suite"		ON  )
OPTION( BUILD_EXAMPLES		"Build library usage example apps"		OFF )
OPTION( TAGS				"Generate tags"							OFF )
OPTION( PROFILE				"Generate profiling information"		OFF )
IF(MSVC)
	# This option is only availalbe when building with MSVC. By default,
	# libgit2 is build using the stdcall calling convention, as that's what
	# the CLR expects by default and how the Windows API is built.
	#
	# If you are writing a C or C++ program and want to link to libgit2, you
	# have to either:
	# - Add /Gz to the compiler options of _your_ program / library.
	# - Turn this off by invoking CMake with the "-DSTDCALL=Off" argument.
	#
	OPTION( STDCALL			"Build libgit2 with the __stdcall convention"	ON  )
ENDIF()

# Installation paths
#
SET(BIN_INSTALL_DIR bin CACHE PATH "Where to install binaries to.")
SET(LIB_INSTALL_DIR lib CACHE PATH "Where to install libraries to.")
SET(INCLUDE_INSTALL_DIR include CACHE PATH "Where to install headers to.")

FUNCTION(TARGET_OS_LIBRARIES target)
	IF(WIN32)
		TARGET_LINK_LIBRARIES(${target} ws2_32)
	ELSEIF(CMAKE_SYSTEM_NAME MATCHES "(Solaris|SunOS)")
		TARGET_LINK_LIBRARIES(${target} socket nsl)
	ENDIF ()
	IF(THREADSAFE)
		TARGET_LINK_LIBRARIES(${target} ${CMAKE_THREAD_LIBS_INIT})
	ENDIF()
ENDFUNCTION()

# For the MSVC IDE, this function splits up the source files like windows
# explorer does. This is esp. useful with the libgit2_clar project, were
# usually 2 or more files share the same name.  Sadly, this file grouping
# is a per-directory option in cmake and not per-target, resulting in
# empty virtual folders "tests-clar" for the git2.dll
FUNCTION(MSVC_SPLIT_SOURCES target)
	IF(MSVC_IDE)
		GET_TARGET_PROPERTY(sources ${target} SOURCES)
		FOREACH(source ${sources})
			IF(source MATCHES ".*/")
				STRING(REPLACE ${CMAKE_CURRENT_SOURCE_DIR}/ "" rel ${source})
				IF(rel)
					STRING(REGEX REPLACE "/([^/]*)$" "" rel ${rel})
					IF(rel)
						STRING(REPLACE "/" "\\\\" rel ${rel})
						SOURCE_GROUP(${rel} FILES ${source})
					ENDIF()
				ENDIF()
			ENDIF()
		ENDFOREACH()
	ENDIF()
ENDFUNCTION()

FILE(STRINGS "include/git2/version.h" GIT2_HEADER REGEX "^#define LIBGIT2_VERSION \"[^\"]*\"$")

STRING(REGEX REPLACE "^.*LIBGIT2_VERSION \"([0-9]+).*$" "\\1" LIBGIT2_VERSION_MAJOR "${GIT2_HEADER}")
STRING(REGEX REPLACE "^.*LIBGIT2_VERSION \"[0-9]+\\.([0-9]+).*$" "\\1" LIBGIT2_VERSION_MINOR  "${GIT2_HEADER}")
STRING(REGEX REPLACE "^.*LIBGIT2_VERSION \"[0-9]+\\.[0-9]+\\.([0-9]+).*$" "\\1" LIBGIT2_VERSION_REV "${GIT2_HEADER}")
SET(LIBGIT2_VERSION_STRING "${LIBGIT2_VERSION_MAJOR}.${LIBGIT2_VERSION_MINOR}.${LIBGIT2_VERSION_REV}")

# Find required dependencies
INCLUDE_DIRECTORIES(src include)

IF (WIN32 AND NOT MINGW)
	ADD_DEFINITIONS(-DGIT_WINHTTP)
ELSE ()
	IF (NOT AMIGA)
		FIND_PACKAGE(OpenSSL)
	ENDIF ()
	FILE(GLOB SRC_HTTP deps/http-parser/*.c)
	INCLUDE_DIRECTORIES(deps/http-parser)
ENDIF()

# Specify sha1 implementation
IF (WIN32 AND NOT MINGW AND NOT SHA1_TYPE STREQUAL "builtin")
    ADD_DEFINITIONS(-DWIN32_SHA1)
    FILE(GLOB SRC_SHA1 src/hash/hash_win32.c)
ELSEIF (OPENSSL_FOUND AND NOT SHA1_TYPE STREQUAL "builtin")
	ADD_DEFINITIONS(-DOPENSSL_SHA1)
ELSE()
	FILE(GLOB SRC_SHA1 src/hash/hash_generic.c)
ENDIF()

# Include POSIX regex when it is required
IF(WIN32 OR AMIGA)
	INCLUDE_DIRECTORIES(deps/regex)
	SET(SRC_REGEX deps/regex/regex.c)
ENDIF()

# Optional external dependency: zlib
IF(NOT ZLIB_LIBRARY)
	# It's optional, but FIND_PACKAGE gives a warning that looks more like an
	# error.
	FIND_PACKAGE(ZLIB QUIET)
ENDIF()
IF (ZLIB_FOUND)
	INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIRS})
	LINK_LIBRARIES(${ZLIB_LIBRARIES})
ELSE()
	MESSAGE( "zlib was not found; using bundled 3rd-party sources." )
	INCLUDE_DIRECTORIES(deps/zlib)
	ADD_DEFINITIONS(-DNO_VIZ -DSTDC -DNO_GZIP)
	FILE(GLOB SRC_ZLIB deps/zlib/*.c)
ENDIF()

# Platform specific compilation flags
IF (MSVC)

    STRING(REPLACE "/Zm1000" " " CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

	# /GF - String pooling
	# /MP - Parallel build
	SET(CMAKE_C_FLAGS "/GF /MP /nologo ${CMAKE_C_FLAGS}")

	IF (STDCALL)
		# /Gz - stdcall calling convention
		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Gz")
	ENDIF ()

	# /Zi - Create debugging information
	# /Od - Disable optimization
	# /D_DEBUG - #define _DEBUG
	# /MTd - Statically link the multithreaded debug version of the CRT
	# /RTC1 - Run time checks
	SET(CMAKE_C_FLAGS_DEBUG "/Zi /Od /D_DEBUG /MTd /RTC1")

	# /MT - Statically link the multithreaded release version of the CRT
	# /O2 - Optimize for speed
	# /Oy - Enable frame pointer omission (FPO) (otherwise CMake will automatically turn it off)
	# /GL - Link time code generation (whole program optimization)
	# /Gy - Function-level linking
	SET(CMAKE_C_FLAGS_RELEASE "/MT /O2 /Oy /GL /Gy")

	# /Oy- - Disable frame pointer omission (FPO)
	SET(CMAKE_C_FLAGS_RELWITHDEBINFO "/Zi /MT /O2 /Oy- /GL /Gy")

	# /O1 - Optimize for size
	SET(CMAKE_C_FLAGS_MINSIZEREL "/MT /O1 /Oy /GL /Gy")

	# /DYNAMICBASE - Address space load randomization (ASLR)
	# /NXCOMPAT - Data execution prevention (DEP)
	# /LARGEADDRESSAWARE - >2GB user address space on x86
	# /VERSION - Embed version information in PE header
	SET(CMAKE_EXE_LINKER_FLAGS "/DYNAMICBASE /NXCOMPAT /LARGEADDRESSAWARE /VERSION:${LIBGIT2_VERSION_MAJOR}.${LIBGIT2_VERSION_MINOR}")

	# /DEBUG - Create a PDB
	# /LTCG - Link time code generation (whole program optimization)
	# /OPT:REF /OPT:ICF - Fold out duplicate code at link step
	# /INCREMENTAL:NO - Required to use /LTCG
	# /DEBUGTYPE:cv,fixup - Additional data embedded in the PDB (requires /INCREMENTAL:NO, so not on for Debug)
	SET(CMAKE_EXE_LINKER_FLAGS_DEBUG "/DEBUG")
	SET(CMAKE_EXE_LINKER_FLAGS_RELEASE "/RELEASE /LTCG /OPT:REF /OPT:ICF /INCREMENTAL:NO")
	SET(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "/DEBUG /RELEASE /LTCG /OPT:REF /OPT:ICF /INCREMENTAL:NO /DEBUGTYPE:cv,fixup")
	SET(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "/RELEASE /LTCG /OPT:REF /OPT:ICF /INCREMENTAL:NO")

	# Same linker settings for DLL as EXE
	SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
	SET(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG}")
	SET(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
	SET(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO}")
	SET(CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL "${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL}")

	SET(WIN_RC "src/win32/git2.rc")

   # Precompiled headers

ELSE ()
	SET(CMAKE_C_FLAGS "-D_GNU_SOURCE -Wall -Wextra -Wno-missing-field-initializers -Wstrict-aliasing=2 -Wstrict-prototypes ${CMAKE_C_FLAGS}")

	IF (WIN32 AND NOT CYGWIN)
		SET(CMAKE_C_FLAGS_DEBUG "-D_DEBUG")
	ENDIF ()

	IF (MINGW) # MinGW always does PIC and complains if we tell it to
		STRING(REGEX REPLACE "-fPIC" "" CMAKE_SHARED_LIBRARY_C_FLAGS "${CMAKE_SHARED_LIBRARY_C_FLAGS}")
	ELSEIF (BUILD_SHARED_LIBS)
		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden -fPIC")
	ENDIF ()
	IF (APPLE) # Apple deprecated OpenSSL
		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-deprecated-declarations")
	ENDIF ()
	IF (PROFILE)
		SET(CMAKE_C_FLAGS "-pg ${CMAKE_C_FLAGS}")
		SET(CMAKE_EXE_LINKER_FLAGS "-pg ${CMAKE_EXE_LINKER_FLAGS}")
	ENDIF ()
ENDIF()

IF( NOT CMAKE_CONFIGURATION_TYPES )
	# Build Debug by default
	IF (NOT CMAKE_BUILD_TYPE)
		SET(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
	ENDIF ()
ELSE()
	# Using a multi-configuration generator eg MSVC or Xcode
	# that uses CMAKE_CONFIGURATION_TYPES and not CMAKE_BUILD_TYPE
ENDIF()

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
	ADD_DEFINITIONS(-DWIN32 -D_WIN32_WINNT=0x0501)
	FILE(GLOB SRC_OS src/win32/*.c)
ELSEIF (AMIGA)
	ADD_DEFINITIONS(-DNO_ADDRINFO -DNO_READDIR_R)
	FILE(GLOB SRC_OS src/amiga/*.c)
ELSE()
	FILE(GLOB SRC_OS src/unix/*.c)
ENDIF()
FILE(GLOB SRC_GIT2 src/*.c src/transports/*.c src/xdiff/*.c)

# Compile and link libgit2
ADD_LIBRARY(git2 ${SRC_GIT2} ${SRC_OS} ${SRC_ZLIB} ${SRC_HTTP} ${SRC_REGEX} ${SRC_SHA1} ${WIN_RC})
TARGET_LINK_LIBRARIES(git2 ${SSL_LIBRARIES})
TARGET_OS_LIBRARIES(git2)

MSVC_SPLIT_SOURCES(git2)

SET_TARGET_PROPERTIES(git2 PROPERTIES VERSION ${LIBGIT2_VERSION_STRING})
SET_TARGET_PROPERTIES(git2 PROPERTIES SOVERSION ${LIBGIT2_VERSION_MAJOR})
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/libgit2.pc.in ${CMAKE_CURRENT_BINARY_DIR}/libgit2.pc @ONLY)

IF (MSVC_IDE)
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
	FILE(GLOB_RECURSE SRC_TEST ${CLAR_PATH}/*/*.c)
	SET(SRC_CLAR "${CLAR_PATH}/main.c" "${CLAR_PATH}/clar_libgit2.c" "${CLAR_PATH}/clar.c")

	ADD_CUSTOM_COMMAND(
		OUTPUT ${CLAR_PATH}/clar.suite
		COMMAND ${PYTHON_EXECUTABLE} generate.py -xonline .
		DEPENDS ${SRC_TEST}
		WORKING_DIRECTORY ${CLAR_PATH}
	)

	SET_SOURCE_FILES_PROPERTIES(
		${CLAR_PATH}/clar.c
		PROPERTIES OBJECT_DEPENDS ${CLAR_PATH}/clar.suite)

	ADD_EXECUTABLE(libgit2_clar ${SRC_GIT2} ${SRC_OS} ${SRC_CLAR} ${SRC_TEST} ${SRC_ZLIB} ${SRC_HTTP} ${SRC_REGEX} ${SRC_SHA1})

	TARGET_LINK_LIBRARIES(libgit2_clar ${SSL_LIBRARIES})
	TARGET_OS_LIBRARIES(libgit2_clar)
	MSVC_SPLIT_SOURCES(libgit2_clar)

	IF (MSVC_IDE)
		# Precompiled headers
		SET_TARGET_PROPERTIES(libgit2_clar PROPERTIES COMPILE_FLAGS "/Yuprecompiled.h /FIprecompiled.h")
	ENDIF ()

	ENABLE_TESTING()
	ADD_TEST(libgit2_clar libgit2_clar -ionline)
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
	IF(WIN32)
		TARGET_LINK_LIBRARIES(cgit2 git2)
	ELSE()
		TARGET_LINK_LIBRARIES(cgit2 git2 pthread)
	ENDIF()

	ADD_EXECUTABLE(git-diff examples/diff.c)
	TARGET_LINK_LIBRARIES(git-diff git2)

	ADD_EXECUTABLE(git-general examples/general.c)
	TARGET_LINK_LIBRARIES(git-general git2)

	ADD_EXECUTABLE(git-showindex examples/showindex.c)
	TARGET_LINK_LIBRARIES(git-showindex git2)
ENDIF ()
