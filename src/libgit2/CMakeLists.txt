# libgit2: the shared library: this CMakeLists.txt compiles the core
# git library functionality.

add_library(libgit2 OBJECT)
set_target_properties(libgit2 PROPERTIES C_STANDARD 90)
set_target_properties(libgit2 PROPERTIES C_EXTENSIONS OFF)

include(PkgBuildConfig)

set(LIBGIT2_INCLUDES
	"${PROJECT_BINARY_DIR}/src"
	"${PROJECT_SOURCE_DIR}/src/libgit2"
	"${PROJECT_SOURCE_DIR}/src/util"
	"${PROJECT_SOURCE_DIR}/include")

if(WIN32 AND EMBED_SSH_PATH)
	file(GLOB SRC_SSH "${EMBED_SSH_PATH}/src/*.c")
	list(SORT SRC_SSH)
	target_sources(libgit2 PRIVATE ${SRC_SSH})

	list(APPEND LIBGIT2_SYSTEM_INCLUDES "${EMBED_SSH_PATH}/include")
	file(WRITE "${EMBED_SSH_PATH}/src/libssh2_config.h" "#define HAVE_WINCNG\n#define LIBSSH2_WINCNG\n#include \"../win32/libssh2_config.h\"")
	set(GIT_SSH 1)
endif()

# Collect sourcefiles
file(GLOB SRC_H
	"${PROJECT_SOURCE_DIR}/include/git2.h"
	"${PROJECT_SOURCE_DIR}/include/git2/*.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys/*.h")
list(SORT SRC_H)
target_sources(libgit2 PRIVATE ${SRC_H})

file(GLOB SRC_GIT2 *.c *.h
	streams/*.c streams/*.h
	transports/*.c transports/*.h
	xdiff/*.c xdiff/*.h)
list(SORT SRC_GIT2)
target_sources(libgit2 PRIVATE ${SRC_GIT2})

if(WIN32 AND NOT CYGWIN)
	# Add resource information on Windows
	set(SRC_RC "git2.rc")
endif()

if(APPLE)
	# The old Secure Transport API has been deprecated in macOS 10.15.
	set_source_files_properties(streams/stransport.c PROPERTIES COMPILE_FLAGS -Wno-deprecated)
endif()

# the xdiff dependency is not (yet) warning-free, disable warnings
# as errors for the xdiff sources until we've sorted them out
if(MSVC)
	set_source_files_properties(xdiff/xdiffi.c PROPERTIES COMPILE_FLAGS -WX-)
	set_source_files_properties(xdiff/xemit.c PROPERTIES COMPILE_FLAGS -WX-)
	set_source_files_properties(xdiff/xhistogram.c PROPERTIES COMPILE_FLAGS -WX-)
	set_source_files_properties(xdiff/xmerge.c PROPERTIES COMPILE_FLAGS -WX-)
	set_source_files_properties(xdiff/xutils.c PROPERTIES COMPILE_FLAGS -WX-)
	set_source_files_properties(xdiff/xpatience.c PROPERTIES COMPILE_FLAGS -WX-)
else()
	set_source_files_properties(xdiff/xdiffi.c PROPERTIES COMPILE_FLAGS "-Wno-sign-compare -Wno-unused-parameter")
	set_source_files_properties(xdiff/xemit.c PROPERTIES COMPILE_FLAGS "-Wno-sign-compare -Wno-unused-parameter")
	set_source_files_properties(xdiff/xhistogram.c PROPERTIES COMPILE_FLAGS "-Wno-sign-compare")
	set_source_files_properties(xdiff/xutils.c PROPERTIES COMPILE_FLAGS "-Wno-sign-compare")
	set_source_files_properties(xdiff/xpatience.c PROPERTIES COMPILE_FLAGS "-Wno-sign-compare")
endif()

ide_split_sources(libgit2)
list(APPEND LIBGIT2_OBJECTS $<TARGET_OBJECTS:util> $<TARGET_OBJECTS:libgit2> ${LIBGIT2_DEPENDENCY_OBJECTS})

target_include_directories(libgit2 PRIVATE ${LIBGIT2_INCLUDES} ${LIBGIT2_DEPENDENCY_INCLUDES} PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_include_directories(libgit2 SYSTEM PRIVATE ${LIBGIT2_SYSTEM_INCLUDES})

set(LIBGIT2_INCLUDES ${LIBGIT2_INCLUDES} PARENT_SCOPE)
set(LIBGIT2_OBJECTS ${LIBGIT2_OBJECTS} PARENT_SCOPE)
set(LIBGIT2_DEPENDENCY_INCLUDES ${LIBGIT2_DEPENDENCY_INCLUDES} PARENT_SCOPE)
set(LIBGIT2_DEPENDENCY_OBJECTS ${LIBGIT2_DEPENDENCY_OBJECTS} PARENT_SCOPE)
set(LIBGIT2_SYSTEM_INCLUDES ${LIBGIT2_SYSTEM_INCLUDES} PARENT_SCOPE)
set(LIBGIT2_SYSTEM_LIBS ${LIBGIT2_SYSTEM_LIBS} PARENT_SCOPE)

#
# Compile and link libgit2
#

add_library(libgit2package ${SRC_RC} ${LIBGIT2_OBJECTS})
target_link_libraries(libgit2package ${LIBGIT2_SYSTEM_LIBS})

set_target_properties(libgit2package PROPERTIES C_STANDARD 90)
set_target_properties(libgit2package PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set_target_properties(libgit2package PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set_target_properties(libgit2package PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

# Workaround for Cmake bug #0011240 (see http://public.kitware.com/Bug/view.php?id=11240)
# Win64+MSVC+static libs = linker error
if(MSVC AND GIT_ARCH_64 AND NOT BUILD_SHARED_LIBS)
	set_target_properties(libgit2package PROPERTIES STATIC_LIBRARY_FLAGS "/MACHINE:x64")
endif()

ide_split_sources(libgit2package)

if(SONAME)
	set_target_properties(libgit2package PROPERTIES VERSION ${libgit2_VERSION})
	set_target_properties(libgit2package PROPERTIES SOVERSION "${libgit2_VERSION_MAJOR}.${libgit2_VERSION_MINOR}")
	if(LIBGIT2_FILENAME)
		target_compile_definitions(libgit2package PRIVATE LIBGIT2_FILENAME=\"${LIBGIT2_FILENAME}\")
		set_target_properties(libgit2package PROPERTIES OUTPUT_NAME ${LIBGIT2_FILENAME})
	elseif(DEFINED LIBGIT2_PREFIX)
		set_target_properties(libgit2package PROPERTIES PREFIX "${LIBGIT2_PREFIX}")
	endif()
endif()

pkg_build_config(NAME libgit2
	VERSION ${libgit2_VERSION}
	DESCRIPTION "The git library, take 2"
	LIBS_SELF git2
	PRIVATE_LIBS ${LIBGIT2_PC_LIBS}
	REQUIRES ${LIBGIT2_PC_REQUIRES})

if(MSVC_IDE)
	# Precompiled headers
	set_target_properties(libgit2package PROPERTIES COMPILE_FLAGS "/Yuprecompiled.h /FIprecompiled.h")
	set_source_files_properties(win32/precompiled.c COMPILE_FLAGS "/Ycprecompiled.h")
endif()

# Install
install(TARGETS libgit2package
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/git2 DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${PROJECT_SOURCE_DIR}/include/git2.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
