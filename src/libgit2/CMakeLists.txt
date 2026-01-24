# libgit2: the shared library: this CMakeLists.txt compiles the core
# git library functionality.

add_library(libgit2 OBJECT)

include(PkgBuildConfig)
include(CMakePackageConfigHelpers)

set(LIBGIT2_INCLUDES
	"${PROJECT_BINARY_DIR}/src/util"
	"${PROJECT_BINARY_DIR}/include"
	"${PROJECT_SOURCE_DIR}/src/libgit2"
	"${PROJECT_SOURCE_DIR}/src/util"
	"${PROJECT_SOURCE_DIR}/include")

# Collect sourcefiles
file(GLOB SRC_H
	"${PROJECT_SOURCE_DIR}/include/git2.h"
	"${PROJECT_SOURCE_DIR}/include/git2/*.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys/*.h")
list(SORT SRC_H)
target_sources(libgit2 PRIVATE ${SRC_H})

file(GLOB SRC_GIT2 *.c *.h
	streams/*.c streams/*.h
	transports/*.c transports/*.h)
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

ide_split_sources(libgit2)
list(APPEND LIBGIT2_OBJECTS $<TARGET_OBJECTS:util> $<TARGET_OBJECTS:libgit2> ${LIBGIT2_DEPENDENCY_OBJECTS})
list(APPEND LIBGIT2_INCLUDES ${LIBGIT2_DEPENDENCY_INCLUDES})

# Build should look at headers in source tree but installed target should point to the possibly mutated
# includes which will be generated
target_include_directories(libgit2 PRIVATE ${LIBGIT2_INCLUDES} ${LIBGIT2_DEPENDENCY_INCLUDES}
		PUBLIC
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>)
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
target_include_directories(libgit2package SYSTEM PRIVATE ${LIBGIT2_INCLUDES})
target_include_directories(libgit2package INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

set_target_properties(libgit2package PROPERTIES C_STANDARD 90)
set_target_properties(libgit2package PROPERTIES C_EXTENSIONS OFF)
set_target_properties(libgit2package PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set_target_properties(libgit2package PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set_target_properties(libgit2package PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

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

pkg_build_config(NAME "lib${LIBGIT2_FILENAME}"
	VERSION ${libgit2_VERSION}
	DESCRIPTION "The git library, take 2"
	LIBS_SELF ${LIBGIT2_FILENAME}
	PRIVATE_LIBS ${LIBGIT2_PC_LIBS}
	REQUIRES ${LIBGIT2_PC_REQUIRES})

if(MSVC_IDE)
	# Precompiled headers
	set_target_properties(libgit2package PROPERTIES COMPILE_FLAGS "/Yuprecompiled.h /FIprecompiled.h")
	set_source_files_properties(win32/precompiled.c COMPILE_FLAGS "/Ycprecompiled.h")
endif()

# support experimental features and functionality

configure_file(experimental.h.in "${PROJECT_BINARY_DIR}/include/git2/experimental.h")

# translate filenames in the headers so that they match the install directory
# (allows for side-by-side installs of libgit2 and libgit2-experimental.)

foreach(HEADER_SOURCE ${SRC_H})
  # Compute desired path in the binary dir
  FILE(RELATIVE_PATH HEADER_RELATIVE ${PROJECT_SOURCE_DIR} ${HEADER_SOURCE})
  STRING(REPLACE "git2/" "git2-experimental/" HEADER_RELATIVE ${HEADER_RELATIVE})
  SET(HEADER_OUT ${PROJECT_BINARY_DIR}/${HEADER_RELATIVE})

  FILE(READ "${HEADER_SOURCE}" HEADER_CONTENTS)
  STRING(REGEX REPLACE "#include \"git2\/" "#include \"${LIBGIT2_FILENAME}/" HEADER_CONTENTS "${HEADER_CONTENTS}")
  FILE(WRITE "${HEADER_OUT}" "${HEADER_CONTENTS}")
endforeach()

# Rename git2.h after the regex & move
FILE(RENAME "${PROJECT_BINARY_DIR}/include/git2.h" "${PROJECT_BINARY_DIR}/include/${LIBGIT2_FILENAME}.h")

# cmake package targets

set(LIBGIT2_TARGETS_EXPORT_NAME "${PROJECT_NAME}Targets")

write_basic_package_version_file(
	"${PROJECT_BINARY_DIR}/cmake/${PROJECT_NAME}ConfigVersion.cmake"
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY SameMajorVersion)

configure_file(config.cmake.in
	"${PROJECT_BINARY_DIR}/cmake/${PROJECT_NAME}Config.cmake"
	@ONLY)

install(FILES
	"${PROJECT_BINARY_DIR}/cmake/${PROJECT_NAME}Config.cmake"
	"${PROJECT_BINARY_DIR}/cmake/${PROJECT_NAME}ConfigVersion.cmake"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")
install(
	EXPORT ${LIBGIT2_TARGETS_EXPORT_NAME}
	NAMESPACE "${PROJECT_NAME}::"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

# Install

install(TARGETS libgit2package
	EXPORT ${LIBGIT2_TARGETS_EXPORT_NAME}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY ${PROJECT_BINARY_DIR}/include/${LIBGIT2_FILENAME}/
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${LIBGIT2_FILENAME}")
install(FILES "${PROJECT_BINARY_DIR}/include/${LIBGIT2_FILENAME}.h"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
