include(PkgBuildConfig)

list(APPEND LIBGIT2CLIENT_INCLUDES
	"${COMMON_INCLUDES}"
	"${libgit2_BINARY_DIR}/src"
	"${libgit2_SOURCE_DIR}/src/util"
	"${libgit2_SOURCE_DIR}/src/libgit2client"
	"${libgit2_SOURCE_DIR}/include")

file(GLOB LIBGIT2CLIENT_SRC_H
	"${libgit2_SOURCE_DIR}/include/git2.h"
	"${libgit2_SOURCE_DIR}/include/git2/*.h"
	"${libgit2_SOURCE_DIR}/include/git2/sys/*.h")
list(SORT LIBGIT2CLIENT_SRC_H)

if(WIN32 AND NOT CYGWIN)
	if(MSVC)
		set(LIBGIT2CLIENT_WIN_RC "win32/git2.rc")
	endif()

	file(GLOB LIBGIT2CLIENT_SRC_OS win32/*.c win32/*.h)
	list(SORT LIBGIT2CLIENT_SRC_OS)
else()
	file(GLOB LIBGIT2CLIENT_SRC_OS unix/*.c unix/*.h)
	list(SORT LIBGIT2CLIENT_SRC_OS)
endif()

file(GLOB LIBGIT2CLIENT_SRC_C *.c *.h)
list(SORT LIBGIT2CLIENT_SRC_C)

set(LIBGIT2CLIENT_SRC ${LIBGIT2CLIENT_SRC_H} ${LIBGIT2CLIENT_SRC_C} ${LIBGIT2CLIENT_SRC_OS})

add_definitions(-DGIT_DEPRECATE_HARD)

add_library(libgit2client OBJECT ${LIBGIT2CLIENT_SRC})
set_target_properties(libgit2client PROPERTIES C_STANDARD 90)
ide_split_sources(libgit2client)
list(APPEND LIBGIT2CLIENT_OBJECTS ${COMMON_OBJECTS})
list(APPEND LIBGIT2CLIENT_OBJECTS $<TARGET_OBJECTS:libgit2client>)
list(APPEND LIBGIT2CLIENT_OBJECTS $<TARGET_OBJECTS:util>)

target_include_directories(libgit2client PRIVATE ${LIBGIT2CLIENT_INCLUDES} PUBLIC ${libgit2_SOURCE_DIR}/include)
target_include_directories(libgit2client SYSTEM PRIVATE ${LIBGIT2CLIENT_SYSTEM_INCLUDES})

if(XCODE_VERSION)
	# This is required for Xcode to actually link the libgit2 library
	# when using only object libraries.
	file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/dummy.c "")
	list(APPEND LIBGIT2CLIENT_OBJECTS ${CMAKE_CURRENT_BINARY_DIR}/dummy.c)
endif()

# Compile and link libgit2
add_library(libgit2client_meta ${WIN_RC} ${LIBGIT2CLIENT_OBJECTS})
target_link_libraries(libgit2client_meta ${LIBGIT2CLIENT_LIBS})
target_link_libraries(libgit2client_meta libgit2_meta)

set_target_properties(libgit2client_meta PROPERTIES C_STANDARD 90)
set_target_properties(libgit2client_meta PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${libgit2_BINARY_DIR})
set_target_properties(libgit2client_meta PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${libgit2_BINARY_DIR})
set_target_properties(libgit2client_meta PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${libgit2_BINARY_DIR})

# Workaround for Cmake bug #0011240 (see http://public.kitware.com/Bug/view.php?id=11240)
# Win64+MSVC+static libs = linker error
if(MSVC AND GIT_ARCH_64 AND NOT BUILD_SHARED_LIBS)
	set_target_properties(libgit2client_meta PROPERTIES STATIC_LIBRARY_FLAGS "/MACHINE:x64")
endif()

ide_split_sources(libgit2client_meta)

if(SONAME)
	set_target_properties(libgit2client_meta PROPERTIES VERSION ${LIBGIT2_VERSION_STRING})
	set_target_properties(libgit2client_meta PROPERTIES SOVERSION ${LIBGIT2_SOVERSION})
	set_target_properties(libgit2client_meta PROPERTIES OUTPUT_NAME git2client)
endif()

pkg_build_config(NAME libgit2client
	VERSION ${LIBGIT2_VERSION_STRING}
	DESCRIPTION "A library to support building Git clients."
	LIBS_SELF libgit2client_meta
	PRIVATE_LIBS ${LIBGIT2CLIENT_PC_LIBS}
	REQUIRES ${LIBGIT2CLIENT_PC_REQUIRES})

if(MSVC_IDE)
	# Precompiled headers
	set_target_properties(libgit2client_meta PROPERTIES COMPILE_FLAGS "/Yuprecompiled.h /FIprecompiled.h")
	set_source_files_properties(win32/precompiled.c COMPILE_FLAGS "/Ycprecompiled.h")
endif()

# Install
install(TARGETS libgit2client_meta
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(DIRECTORY ${libgit2_SOURCE_DIR}/include/git2client DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${libgit2_SOURCE_DIR}/include/git2client.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# export these variables for the test framework; it links our object
# files directly (instead of linking to libgit2.so) so that it can
# test private and internal functions.
set_property(GLOBAL PROPERTY libgit2client_objects ${LIBGIT2CLIENT_OBJECTS})
set_property(GLOBAL PROPERTY libgit2client_includes ${LIBGIT2CLIENT_INCLUDES})
set_property(GLOBAL PROPERTY libgit2client_system_includes ${LIBGIT2CLIENT_SYSTEM_INCLUDES})
set_property(GLOBAL PROPERTY libgit2client_libs ${LIBGIT2CLIENT_LIBS})
