set(CLI_INCLUDES
	"${libgit2_BINARY_DIR}/src"
	"${libgit2_SOURCE_DIR}/src/util"
	"${libgit2_SOURCE_DIR}/src/cli"
	"${libgit2_SOURCE_DIR}/include")

if(WIN32 AND NOT CYGWIN)
	file(GLOB CLI_SRC_OS win32/*.c)
	list(SORT CLI_SRC_OS)
else()
	file(GLOB CLI_SRC_OS unix/*.c)
	list(SORT CLI_SRC_OS)
endif()

file(GLOB CLI_SRC_C *.c *.h)
list(SORT CLI_SRC_C)

#
# Configure linking against the libgit2 static objects or shared library.
#

if(BUILD_CLI STREQUAL "static")
	set(CLI_LIBGIT2_OBJECTS $<TARGET_OBJECTS:libgit2>)
else()
	set(CLI_LIBGIT2_LIBRARY libgit2package)
endif()

#
# Compile and link the CLI
#

add_executable(git2_cli ${CLI_SRC_C} ${CLI_SRC_OS} ${CLI_OBJECTS}
	$<TARGET_OBJECTS:util>
	${CLI_LIBGIT2_OBJECTS}
	${LIBGIT2_DEPENDENCY_OBJECTS})
target_link_libraries(git2_cli ${CLI_LIBGIT2_LIBRARY} ${LIBGIT2_SYSTEM_LIBS})

set_target_properties(git2_cli PROPERTIES C_STANDARD 90)
set_target_properties(git2_cli PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${libgit2_BINARY_DIR})

ide_split_sources(git2_cli)

target_include_directories(git2_cli PRIVATE ${CLI_INCLUDES})

if(MSVC_IDE)
	# Precompiled headers
	set_target_properties(git2_cli PROPERTIES COMPILE_FLAGS "/Yuprecompiled.h /FIprecompiled.h")
	set_source_files_properties(win32/precompiled.c COMPILE_FLAGS "/Ycprecompiled.h")
endif()

install(TARGETS git2_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
