list(APPEND CLI_INCLUDES
	"${COMMON_INCLUDES}"
	"${libgit2_BINARY_DIR}/src"
	"${libgit2_SOURCE_DIR}/src/util"
	"${libgit2_SOURCE_DIR}/src/cli"
	"${libgit2_SOURCE_DIR}/include")

file(GLOB CLI_SRC_H
	"${libgit2_SOURCE_DIR}/include/git2.h"
	"${libgit2_SOURCE_DIR}/include/git2/*.h"
	"${libgit2_SOURCE_DIR}/include/git2/sys/*.h")
list(SORT CLI_SRC_H)

if(WIN32 AND NOT CYGWIN)
	file(GLOB CLI_SRC_OS win32/*.c)
	list(SORT CLI_SRC_OS)
else()
	file(GLOB CLI_SRC_OS unix/*.c)
	list(SORT CLI_SRC_OS)
endif()

file(GLOB CLI_SRC_C *.c *.h)
list(SORT CLI_SRC_C)

add_definitions(-DGIT_DEPRECATE_HARD)

list(APPEND CLI_OBJECTS ${COMMON_OBJECTS})
list(APPEND CLI_OBJECTS $<TARGET_OBJECTS:util>)

add_executable(git2_cli ${CLI_SRC_C} ${CLI_SRC_OS} ${CLI_OBJECTS})
ide_split_sources(git2_cli)
target_link_libraries(git2_cli libgit2client_meta libgit2_meta)

include_directories(git2_cli ${CLI_INCLUDES})

set_target_properties(git2_cli PROPERTIES C_STANDARD 90)
set_target_properties(git2_cli PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${libgit2_BINARY_DIR})

if(MSVC_IDE)
	# Precompiled headers
	set_target_properties(git2_cli PROPERTIES COMPILE_FLAGS "/Yuprecompiled.h /FIprecompiled.h")
	set_source_files_properties(win32/precompiled.c COMPILE_FLAGS "/Ycprecompiled.h")
endif()

install(TARGETS git2_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
