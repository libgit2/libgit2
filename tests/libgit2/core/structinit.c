#include "clar_libgit2.h"
#include <git2/sys/commit_graph.h>
#include <git2/sys/config.h>
#include <git2/sys/filter.h>
#include <git2/sys/odb_backend.h>
#include <git2/sys/refdb_backend.h>
#include <git2/sys/transport.h>
#include <git2/sys/midx.h>
#include <git2/sys/repository.h>

#define STRINGIFY(s) #s

/* Checks two conditions for the specified structure:
 *     1. That the initializers for the latest version produces the same
 *        in-memory representation.
 *     2. That the function-based initializer supports all versions from 1...n,
 *        where n is the latest version (often represented by GIT_*_VERSION).
 *
 * Parameters:
 *     structname: The name of the structure to test, e.g. git_blame_options.
 *     structver: The latest version of the specified structure.
 *     macroinit: The macro that initializes the latest version of the structure.
 *     funcinitname: The function that initializes the structure. Must have the
 *                   signature "int (structname* instance, int version)".
 */
#define CHECK_MACRO_FUNC_INIT_EQUAL(structname, structver, macroinit, funcinitname) \
do { \
	structname structname##_macro_latest = macroinit; \
	structname structname##_func_latest; \
	int structname##_curr_ver = structver - 1; \
	memset(&structname##_func_latest, 0, sizeof(structname##_func_latest)); \
	cl_git_pass(funcinitname(&structname##_func_latest, structver)); \
	options_cmp(&structname##_macro_latest, &structname##_func_latest, \
		sizeof(structname), STRINGIFY(structname)); \
	\
	while (structname##_curr_ver > 0) \
	{ \
		structname macro; \
		cl_git_pass(funcinitname(&macro, structname##_curr_ver)); \
		structname##_curr_ver--; \
	}\
} while(0)

static void options_cmp(void *one, void *two, size_t size, const char *name)
{
	size_t i;

	for (i = 0; i < size; i++) {
		if (((char *)one)[i] != ((char *)two)[i]) {
			char desc[1024];

			p_snprintf(desc, 1024, "Difference in %s at byte %" PRIuZ ": macro=%u / func=%u",
				name, i, ((char *)one)[i], ((char *)two)[i]);
			clar__fail(__FILE__, __func__, __LINE__,
				"Difference between macro and function options initializer",
				desc, 0);
			return;
		}
	}
}

void test_core_structinit__compare(void)
{
	/* These tests assume that they can memcmp() two structures that were
	 * initialized with the same static initializer.  Eg,
	 * git_blame_options = GIT_BLAME_OPTIONS_INIT;
	 *
	 * This assumption fails when there is padding between structure members,
	 * which is not guaranteed to be initialized to anything sane at all.
	 *
	 * Assume most compilers, in a debug build, will clear that memory for
	 * us or set it to sentinel markers.  Etc.
	 */
#if !defined(DEBUG) && !defined(_DEBUG)
	clar__skip();
#endif

	/* apply */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_apply_options, GIT_APPLY_OPTIONS_VERSION, \
		GIT_APPLY_OPTIONS_INIT, git_apply_options_init);

	/* attr */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_attr_options, GIT_ATTR_OPTIONS_VERSION, \
		GIT_ATTR_OPTIONS_INIT, git_attr_options_init);

	/* blame */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_blame_options, GIT_BLAME_OPTIONS_VERSION, \
		GIT_BLAME_OPTIONS_INIT, git_blame_options_init);

	/* blob_filter_options */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_blob_filter_options, GIT_BLOB_FILTER_OPTIONS_VERSION, \
		GIT_BLOB_FILTER_OPTIONS_INIT, git_blob_filter_options_init);

	/* checkout */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_checkout_options, GIT_CHECKOUT_OPTIONS_VERSION, \
		GIT_CHECKOUT_OPTIONS_INIT, git_checkout_options_init);

	/* cherrypick */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_cherrypick_options, GIT_CHERRYPICK_OPTIONS_VERSION, \
		GIT_CHERRYPICK_OPTIONS_INIT, git_cherrypick_options_init);

	/* clone */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_clone_options, GIT_CLONE_OPTIONS_VERSION, \
		GIT_CLONE_OPTIONS_INIT, git_clone_options_init);

	/* commit_create */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_commit_create_options, \
		GIT_COMMIT_CREATE_OPTIONS_VERSION, \
		GIT_COMMIT_CREATE_OPTIONS_INIT, \
		git_commit_create_options_init);

	/* commit_create_ext */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_commit_create_ext_options, \
		GIT_COMMIT_CREATE_EXT_OPTIONS_VERSION, \
		GIT_COMMIT_CREATE_EXT_OPTIONS_INIT, \
		git_commit_create_ext_options_init);

	/* commit_graph_open */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_commit_graph_open_options, \
		GIT_COMMIT_GRAPH_OPEN_OPTIONS_VERSION, \
		GIT_COMMIT_GRAPH_OPEN_OPTIONS_INIT, \
		git_commit_graph_open_options_init);

	/* commit_graph_writer */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_commit_graph_writer_options, \
		GIT_COMMIT_GRAPH_WRITER_OPTIONS_VERSION, \
		GIT_COMMIT_GRAPH_WRITER_OPTIONS_INIT, \
		git_commit_graph_writer_options_init);

	/* describe */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_describe_options, GIT_DESCRIBE_OPTIONS_VERSION, \
		GIT_DESCRIBE_OPTIONS_INIT, git_describe_options_init);

	/* describe_format */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_describe_format_options, \
		GIT_DESCRIBE_FORMAT_OPTIONS_VERSION, \
		GIT_DESCRIBE_FORMAT_OPTIONS_INIT, \
		git_describe_format_options_init);

	/* diff */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_diff_options, GIT_DIFF_OPTIONS_VERSION, \
		GIT_DIFF_OPTIONS_INIT, git_diff_options_init);

	/* diff_find */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_diff_find_options, GIT_DIFF_FIND_OPTIONS_VERSION, \
		GIT_DIFF_FIND_OPTIONS_INIT, git_diff_find_options_init);

	/* email_create */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_email_create_options, \
		GIT_EMAIL_CREATE_OPTIONS_VERSION, \
		GIT_EMAIL_CREATE_OPTIONS_INIT, \
		git_email_create_options_init);

	/* filter */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_filter, GIT_FILTER_VERSION, \
		GIT_FILTER_INIT, git_filter_init);

	/* filter_options_init */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_filter_options, GIT_FILTER_OPTIONS_VERSION, \
		GIT_FILTER_OPTIONS_INIT, git_filter_options_init);

	/* index */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_index_options, GIT_INDEX_OPTIONS_VERSION, \
		GIT_INDEX_OPTIONS_INIT, git_index_options_init);

	/* indexer */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_indexer_options, GIT_INDEXER_OPTIONS_VERSION, \
		GIT_INDEXER_OPTIONS_INIT, git_indexer_options_init);

	/* merge_file_input */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_merge_file_input, GIT_MERGE_FILE_INPUT_VERSION, \
		GIT_MERGE_FILE_INPUT_INIT, git_merge_file_input_init);

	/* merge_file */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_merge_file_options, GIT_MERGE_FILE_OPTIONS_VERSION, \
		GIT_MERGE_FILE_OPTIONS_INIT, git_merge_file_options_init);

	/* merge_tree */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_merge_options, GIT_MERGE_OPTIONS_VERSION, \
		GIT_MERGE_OPTIONS_INIT, git_merge_options_init);

	/* object_id */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_object_id_options, GIT_OBJECT_ID_OPTIONS_VERSION, \
		GIT_OBJECT_ID_OPTIONS_INIT, git_object_id_options_init);

	/* odb */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_odb_options, GIT_ODB_OPTIONS_VERSION, \
		GIT_ODB_OPTIONS_INIT, git_odb_options_init);

	/* odb_backend_pack */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_odb_backend_pack_options, \
		GIT_ODB_BACKEND_PACK_OPTIONS_VERSION, \
		GIT_ODB_BACKEND_PACK_OPTIONS_INIT, \
		git_odb_backend_pack_options_init);

	/* odb_backend_loose */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_odb_backend_loose_options, \
		GIT_ODB_BACKEND_LOOSE_OPTIONS_VERSION, \
		GIT_ODB_BACKEND_LOOSE_OPTIONS_INIT, \
		git_odb_backend_loose_options_init);

	/* proxy */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_proxy_options, GIT_PROXY_OPTIONS_VERSION, \
		GIT_PROXY_OPTIONS_INIT, git_proxy_options_init);

	/* rebase */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_rebase_options, GIT_REBASE_OPTIONS_VERSION, \
		GIT_REBASE_OPTIONS_INIT, git_rebase_options_init);

	/* remote_create */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_remote_create_options, \
		GIT_REMOTE_CREATE_OPTIONS_VERSION, \
		GIT_REMOTE_CREATE_OPTIONS_INIT, \
		git_remote_create_options_init);

	/* fetch */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_fetch_options, GIT_FETCH_OPTIONS_VERSION, \
		GIT_FETCH_OPTIONS_INIT, git_fetch_options_init);

	/* push */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_push_options, GIT_PUSH_OPTIONS_VERSION, \
		GIT_PUSH_OPTIONS_INIT, git_push_options_init);

	/* remote_connect */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_remote_connect_options, \
		GIT_REMOTE_CONNECT_OPTIONS_VERSION, \
		GIT_REMOTE_CONNECT_OPTIONS_INIT, \
		git_remote_connect_options_init);

	/* remote */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_remote_callbacks, GIT_REMOTE_CALLBACKS_VERSION, \
		GIT_REMOTE_CALLBACKS_INIT, git_remote_init_callbacks);

	/* repository_init */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_repository_init_options, \
		GIT_REPOSITORY_INIT_OPTIONS_VERSION, \
		GIT_REPOSITORY_INIT_OPTIONS_INIT, \
		git_repository_init_options_init);

	/* revert */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_revert_options, GIT_REVERT_OPTIONS_VERSION, \
		GIT_REVERT_OPTIONS_INIT, git_revert_options_init);

	/* stash apply */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_stash_apply_options, GIT_STASH_APPLY_OPTIONS_VERSION, \
		GIT_STASH_APPLY_OPTIONS_INIT, git_stash_apply_options_init);

	/* stash save */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_stash_save_options, GIT_STASH_SAVE_OPTIONS_VERSION, \
		GIT_STASH_SAVE_OPTIONS_INIT, git_stash_save_options_init);

	/* status */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_status_options, GIT_STATUS_OPTIONS_VERSION, \
		GIT_STATUS_OPTIONS_INIT, git_status_options_init);

	/* transport */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_transport, GIT_TRANSPORT_VERSION, \
		GIT_TRANSPORT_INIT, git_transport_init);

	/* config_backend */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_config_backend, GIT_CONFIG_BACKEND_VERSION, \
		GIT_CONFIG_BACKEND_INIT, git_config_init_backend);

	/* config_backend_memory */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_config_backend_memory_options, \
		GIT_CONFIG_BACKEND_MEMORY_OPTIONS_VERSION, \
		GIT_CONFIG_BACKEND_MEMORY_OPTIONS_INIT, \
		git_config_backend_memory_options_init);

	/* odb_backend */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_odb_backend, GIT_ODB_BACKEND_VERSION, \
		GIT_ODB_BACKEND_INIT, git_odb_init_backend);

	/* refdb_backend */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_refdb_backend, \
		GIT_REFDB_BACKEND_VERSION, \
		GIT_REFDB_BACKEND_INIT, \
		git_refdb_init_backend);

	/* submodule update */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_submodule_update_options, \
		GIT_SUBMODULE_UPDATE_OPTIONS_VERSION, \
		GIT_SUBMODULE_UPDATE_OPTIONS_INIT, \
		git_submodule_update_options_init);

	/* worktree add */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_worktree_add_options, \
		GIT_WORKTREE_ADD_OPTIONS_VERSION, \
		GIT_WORKTREE_ADD_OPTIONS_INIT, \
		git_worktree_add_options_init);

	/* worktree prune */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_worktree_prune_options, \
		GIT_WORKTREE_PRUNE_OPTIONS_VERSION, \
		GIT_WORKTREE_PRUNE_OPTIONS_INIT, \
		git_worktree_prune_options_init);

	/* midx_writer */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_midx_writer_options, \
		GIT_MIDX_WRITER_OPTIONS_VERSION, \
		GIT_MIDX_WRITER_OPTIONS_INIT, \
		git_midx_writer_options_init);

	/* repository new */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_repository_new_options, \
		GIT_REPOSITORY_NEW_OPTIONS_VERSION, \
		GIT_REPOSITORY_NEW_OPTIONS_INIT, \
		git_repository_new_options_init);

	/* worktree prune */
	CHECK_MACRO_FUNC_INIT_EQUAL( \
		git_worktree_prune_options, \
		GIT_WORKTREE_PRUNE_OPTIONS_VERSION, \
		GIT_WORKTREE_PRUNE_OPTIONS_INIT, \
		git_worktree_prune_options_init);
}
