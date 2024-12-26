# libgit2: the shared library: this CMakeLists.txt compiles the core
# git library functionality.

add_library(libgit2 OBJECT)

set(PUBLIC_H
	"${PROJECT_SOURCE_DIR}/include/git2.h"
	"${PROJECT_SOURCE_DIR}/include/git2/annotated_commit.h"
	"${PROJECT_SOURCE_DIR}/include/git2/apply.h"
	"${PROJECT_SOURCE_DIR}/include/git2/attr.h"
	"${PROJECT_SOURCE_DIR}/include/git2/blame.h"
	"${PROJECT_SOURCE_DIR}/include/git2/blob.h"
	"${PROJECT_SOURCE_DIR}/include/git2/branch.h"
	"${PROJECT_SOURCE_DIR}/include/git2/buffer.h"
	"${PROJECT_SOURCE_DIR}/include/git2/cert.h"
	"${PROJECT_SOURCE_DIR}/include/git2/checkout.h"
	"${PROJECT_SOURCE_DIR}/include/git2/cherrypick.h"
	"${PROJECT_SOURCE_DIR}/include/git2/clone.h"
	"${PROJECT_SOURCE_DIR}/include/git2/commit.h"
	"${PROJECT_SOURCE_DIR}/include/git2/common.h"
	"${PROJECT_SOURCE_DIR}/include/git2/config.h"
	"${PROJECT_SOURCE_DIR}/include/git2/cred_helpers.h"
	"${PROJECT_SOURCE_DIR}/include/git2/credential.h"
	"${PROJECT_SOURCE_DIR}/include/git2/credential_helpers.h"
	"${PROJECT_SOURCE_DIR}/include/git2/deprecated.h"
	"${PROJECT_SOURCE_DIR}/include/git2/describe.h"
	"${PROJECT_SOURCE_DIR}/include/git2/diff.h"
	"${PROJECT_SOURCE_DIR}/include/git2/email.h"
	"${PROJECT_SOURCE_DIR}/include/git2/errors.h"
	"${PROJECT_SOURCE_DIR}/include/git2/experimental.h"
	"${PROJECT_SOURCE_DIR}/include/git2/filter.h"
	"${PROJECT_SOURCE_DIR}/include/git2/global.h"
	"${PROJECT_SOURCE_DIR}/include/git2/graph.h"
	"${PROJECT_SOURCE_DIR}/include/git2/ignore.h"
	"${PROJECT_SOURCE_DIR}/include/git2/index.h"
	"${PROJECT_SOURCE_DIR}/include/git2/indexer.h"
	"${PROJECT_SOURCE_DIR}/include/git2/mailmap.h"
	"${PROJECT_SOURCE_DIR}/include/git2/merge.h"
	"${PROJECT_SOURCE_DIR}/include/git2/message.h"
	"${PROJECT_SOURCE_DIR}/include/git2/net.h"
	"${PROJECT_SOURCE_DIR}/include/git2/notes.h"
	"${PROJECT_SOURCE_DIR}/include/git2/object.h"
	"${PROJECT_SOURCE_DIR}/include/git2/odb.h"
	"${PROJECT_SOURCE_DIR}/include/git2/odb_backend.h"
	"${PROJECT_SOURCE_DIR}/include/git2/oid.h"
	"${PROJECT_SOURCE_DIR}/include/git2/oidarray.h"
	"${PROJECT_SOURCE_DIR}/include/git2/pack.h"
	"${PROJECT_SOURCE_DIR}/include/git2/patch.h"
	"${PROJECT_SOURCE_DIR}/include/git2/pathspec.h"
	"${PROJECT_SOURCE_DIR}/include/git2/proxy.h"
	"${PROJECT_SOURCE_DIR}/include/git2/rebase.h"
	"${PROJECT_SOURCE_DIR}/include/git2/refdb.h"
	"${PROJECT_SOURCE_DIR}/include/git2/reflog.h"
	"${PROJECT_SOURCE_DIR}/include/git2/refs.h"
	"${PROJECT_SOURCE_DIR}/include/git2/refspec.h"
	"${PROJECT_SOURCE_DIR}/include/git2/remote.h"
	"${PROJECT_SOURCE_DIR}/include/git2/repository.h"
	"${PROJECT_SOURCE_DIR}/include/git2/reset.h"
	"${PROJECT_SOURCE_DIR}/include/git2/revert.h"
	"${PROJECT_SOURCE_DIR}/include/git2/revparse.h"
	"${PROJECT_SOURCE_DIR}/include/git2/revwalk.h"
	"${PROJECT_SOURCE_DIR}/include/git2/signature.h"
	"${PROJECT_SOURCE_DIR}/include/git2/stash.h"
	"${PROJECT_SOURCE_DIR}/include/git2/status.h"
	"${PROJECT_SOURCE_DIR}/include/git2/stdint.h"
	"${PROJECT_SOURCE_DIR}/include/git2/strarray.h"
	"${PROJECT_SOURCE_DIR}/include/git2/submodule.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys"
	"${PROJECT_SOURCE_DIR}/include/git2/tag.h"
	"${PROJECT_SOURCE_DIR}/include/git2/trace.h"
	"${PROJECT_SOURCE_DIR}/include/git2/transaction.h"
	"${PROJECT_SOURCE_DIR}/include/git2/transport.h"
	"${PROJECT_SOURCE_DIR}/include/git2/tree.h"
	"${PROJECT_SOURCE_DIR}/include/git2/types.h"
	"${PROJECT_SOURCE_DIR}/include/git2/version.h"
	"${PROJECT_SOURCE_DIR}/include/git2/worktree.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys/alloc.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys/commit.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys/commit_graph.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys/config.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys/cred.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys/credential.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys/diff.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys/email.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys/errors.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys/filter.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys/hashsig.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys/index.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys/mempack.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys/merge.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys/midx.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys/odb_backend.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys/openssl.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys/path.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys/refdb_backend.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys/refs.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys/remote.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys/repository.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys/stream.h"
	"${PROJECT_SOURCE_DIR}/include/git2/sys/transport.h")

set(LIBGIT2_SRC
	annotated_commit.c annotated_commit.h
	apply.c apply.h
	attr.c attr.h
	attr_file.c attr_file.h
	attrcache.c attrcache.h
	blame.c blame.h
	blame_git.c blame_git.h
	blob.c blob.h
	branch.c branch.h
	buf.c buf.h
	cache.c cache.h
	checkout.c checkout.h
	cherrypick.c
	clone.c clone.h
	commit.c commit.h
	commit_graph.c commit_graph.h
	commit_list.c commit_list.h
	common.h
	config.c config.h
	config_backend.h
	config_cache.c
	config_file.c
	config_list.c config_list.h
	config_mem.c
	config_parse.c config_parse.h
	config_snapshot.c
	crlf.c
	delta.c delta.h
	describe.c
	diff.c diff.h
	diff_driver.c diff_driver.h
	diff_file.c diff_file.h
	diff_generate.c diff_generate.h
	diff_parse.c diff_parse.h
	diff_print.c
	diff_stats.c diff_stats.h
	diff_tform.c diff_tform.h
	diff_xdiff.c diff_xdiff.h
	email.c email.h
	fetch.c fetch.h
	fetchhead.c fetchhead.h
	filter.c filter.h
	git2.rc
	grafts.c grafts.h
	graph.c
	hashmap_oid.h
	hashsig.c
	ident.c
	ignore.c ignore.h
	index.c index.h
	index_map.c index_map.h
	indexer.c indexer.h
	iterator.c iterator.h
	libgit2.c
	mailmap.c mailmap.h
	merge.c merge.h
	merge_driver.c merge_driver.h
	merge_file.c
	message.c
	midx.c midx.h
	mwindow.c mwindow.h
	notes.c notes.h
	object.c object.h
	object_api.c
	odb.c odb.h
	odb_loose.c
	odb_mempack.c
	odb_pack.c
	oid.c oid.h
	oidarray.c oidarray.h
	pack-objects.c pack-objects.h
	pack.c pack.h
	parse.c parse.h
	patch.c patch.h
	patch_generate.c patch_generate.h
	patch_parse.c patch_parse.h
	path.c path.h
	pathspec.c pathspec.h
	proxy.c proxy.h
	push.c push.h
	reader.c reader.h
	rebase.c
	refdb.c refdb.h
	refdb_fs.c
	reflog.c reflog.h
	refs.c refs.h
	refspec.c refspec.h
	remote.c remote.h
	repo_template.h
	repository.c repository.h
	reset.c
	revert.c
	revparse.c
	revwalk.c revwalk.h
	settings.c settings.h
	signature.c signature.h
	stash.c
	status.c status.h
	strarray.c strarray.h
	stream.h
	streams/mbedtls.c streams/mbedtls.h
	streams/openssl.c streams/openssl.h
	streams/openssl_dynamic.c streams/openssl_dynamic.h
	streams/openssl_legacy.c streams/openssl_legacy.h
	streams/registry.c streams/registry.h
	streams/schannel.c streams/schannel.h
	streams/socket.c streams/socket.h
	streams/stransport.c streams/stransport.h
	streams/tls.c streams/tls.h
	submodule.c submodule.h
	sysdir.c sysdir.h
	tag.c tag.h
	trace.c trace.h
	trailer.c
	transaction.c transaction.h
	transport.c
	transports/auth.c transports/auth.h
	transports/auth_gssapi.c transports/auth_negotiate.h
	transports/auth_ntlm.h
	transports/auth_ntlmclient.c
	transports/auth_sspi.c
	transports/credential.c
	transports/credential_helpers.c
	transports/git.c
	transports/http.c transports/http.h
	transports/httpclient.c transports/httpclient.h
	transports/httpparser.c transports/httpparser.h
	transports/local.c
	transports/smart.c transports/smart.h
	transports/smart_pkt.c
	transports/smart_protocol.c
	transports/ssh.c
	transports/ssh_exec.c transports/ssh_exec.h
	transports/ssh_libssh2.c transports/ssh_libssh2.h
	transports/winhttp.c
	tree-cache.c tree-cache.h
	tree.c tree.h
	userdiff.h
	worktree.c worktree.h)

target_sources(libgit2 PRIVATE ${PUBLIC_H} ${LIBGIT2_SRC})
ide_split_sources(libgit2)

include(PkgBuildConfig)

if(WIN32 AND NOT CYGWIN)
	# Add resource information on Windows
	set(SRC_RC "git2.rc")
endif()

if(APPLE)
	# The old Secure Transport API has been deprecated in macOS 10.15.
	set_source_files_properties(streams/stransport.c PROPERTIES COMPILE_FLAGS -Wno-deprecated)
endif()

set(LIBGIT2_INCLUDES
	"${PROJECT_BINARY_DIR}/src/util"
	"${PROJECT_BINARY_DIR}/include"
	"${PROJECT_SOURCE_DIR}/src/libgit2"
	"${PROJECT_SOURCE_DIR}/src/util"
	"${PROJECT_SOURCE_DIR}/include")
list(APPEND LIBGIT2_INCLUDES ${LIBGIT2_DEPENDENCY_INCLUDES})

target_include_directories(libgit2 PRIVATE ${LIBGIT2_INCLUDES} ${LIBGIT2_DEPENDENCY_INCLUDES} PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_include_directories(libgit2 SYSTEM PRIVATE ${LIBGIT2_SYSTEM_INCLUDES})

list(APPEND LIBGIT2_OBJECTS $<TARGET_OBJECTS:util> $<TARGET_OBJECTS:libgit2> ${LIBGIT2_DEPENDENCY_OBJECTS})

# Propogate configuration

set(LIBGIT2_INCLUDES ${LIBGIT2_INCLUDES} PARENT_SCOPE)
set(LIBGIT2_OBJECTS ${LIBGIT2_OBJECTS} PARENT_SCOPE)
set(LIBGIT2_DEPENDENCY_INCLUDES ${LIBGIT2_DEPENDENCY_INCLUDES} PARENT_SCOPE)
set(LIBGIT2_DEPENDENCY_OBJECTS ${LIBGIT2_DEPENDENCY_OBJECTS} PARENT_SCOPE)
set(LIBGIT2_SYSTEM_INCLUDES ${LIBGIT2_SYSTEM_INCLUDES} PARENT_SCOPE)
set(LIBGIT2_SYSTEM_LIBS ${LIBGIT2_SYSTEM_LIBS} PARENT_SCOPE)

#
# Compile and link libgit2
#

add_library(libgit2package)

target_sources(libgit2package PRIVATE ${SRC_RC} ${LIBGIT2_OBJECTS})
ide_split_sources(libgit2package)

target_link_libraries(libgit2package ${LIBGIT2_SYSTEM_LIBS})
target_include_directories(libgit2package SYSTEM PRIVATE ${LIBGIT2_INCLUDES})

set_target_properties(libgit2package PROPERTIES C_STANDARD 90)
set_target_properties(libgit2package PROPERTIES C_EXTENSIONS OFF)
set_target_properties(libgit2package PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set_target_properties(libgit2package PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set_target_properties(libgit2package PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

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

if(MSVC)
	# Precompiled headers
	set_target_properties(libgit2package PROPERTIES COMPILE_FLAGS "/Yuprecompiled.h /FIprecompiled.h")
	set_source_files_properties(win32/precompiled.c COMPILE_FLAGS "/Ycprecompiled.h")
endif()

# support experimental features and functionality

configure_file(experimental.h.in "${PROJECT_BINARY_DIR}/include/git2/experimental.h")

# translate filenames in the git2.h so that they match the install directory
# (allows for side-by-side installs of libgit2 and libgit2-experimental.)

FILE(READ "${PROJECT_SOURCE_DIR}/include/git2.h" LIBGIT2_INCLUDE)
STRING(REGEX REPLACE "#include \"git2\/" "#include \"${LIBGIT2_FILENAME}/" LIBGIT2_INCLUDE "${LIBGIT2_INCLUDE}")
FILE(WRITE "${PROJECT_BINARY_DIR}/include/${LIBGIT2_FILENAME}.h" ${LIBGIT2_INCLUDE})

# Install

install(TARGETS libgit2package
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/git2/
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${LIBGIT2_FILENAME}")
install(FILES ${PROJECT_BINARY_DIR}/include/git2/experimental.h
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${LIBGIT2_FILENAME}")
install(FILES "${PROJECT_BINARY_DIR}/include/${LIBGIT2_FILENAME}.h"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
