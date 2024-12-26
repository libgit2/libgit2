add_executable(git2_cli)

set(CLI_SRC
	cmd.c cmd.h
	cmd_blame.c
	cmd_cat_file.c
	cmd_clone.c
	cmd_config.c
	cmd_hash_object.c
	cmd_help.c
	cmd_index_pack.c
	common.c common.h
	error.h
	main.c
	opt.c opt.h
	opt_usage.c opt_usage.h
	progress.c progress.h
	sighandler.h)

if(WIN32 AND NOT CYGWIN)
	set(CLI_SRC_OS
		win32/precompiled.c win32/precompiled.h
		win32/sighandler.c)
else()
	set(CLI_SRC_OS unix/sighandler.c)
endif()

#
# The CLI currently needs to be statically linked against libgit2 because
# the utility library uses libgit2's thread-local error buffers.  TODO:
# remove this dependency and allow us to dynamically link against libgit2.
#

if(BUILD_CLI STREQUAL "dynamic")
	set(CLI_LIBGIT2_LIBRARY libgit2package)
else()
	set(CLI_LIBGIT2_OBJECTS $<TARGET_OBJECTS:libgit2>)
endif()

target_sources(git2_cli PRIVATE ${CLI_SRC} ${CLI_SRC_OS} ${CLI_OBJECTS}
	$<TARGET_OBJECTS:util>
	${CLI_LIBGIT2_OBJECTS}
	${LIBGIT2_DEPENDENCY_OBJECTS})
ide_split_sources(git2_cli)

set(CLI_INCLUDES
	"${libgit2_BINARY_DIR}/src/util"
	"${libgit2_BINARY_DIR}/include"
	"${libgit2_SOURCE_DIR}/src/util"
	"${libgit2_SOURCE_DIR}/src/cli"
	"${libgit2_SOURCE_DIR}/include"
	"${LIBGIT2_DEPENDENCY_INCLUDES}"
	"${LIBGIT2_SYSTEM_INCLUDES}")
target_include_directories(git2_cli PRIVATE ${CLI_INCLUDES})

#
# Compile and link the CLI
#

target_link_libraries(git2_cli ${CLI_LIBGIT2_LIBRARY} ${LIBGIT2_SYSTEM_LIBS})

set_target_properties(git2_cli PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${libgit2_BINARY_DIR})
set_target_properties(git2_cli PROPERTIES OUTPUT_NAME ${LIBGIT2_FILENAME})

if(MSVC_IDE)
	# Precompiled headers
	set_target_properties(git2_cli PROPERTIES COMPILE_FLAGS "/Yuprecompiled.h /FIprecompiled.h")
	set_source_files_properties(win32/precompiled.c COMPILE_FLAGS "/Ycprecompiled.h")
endif()

install(TARGETS git2_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
