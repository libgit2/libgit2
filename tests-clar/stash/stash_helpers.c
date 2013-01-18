#include "clar_libgit2.h"
#include "fileops.h"
#include "stash_helpers.h"

void commit_staged_files(
	git_oid *commit_oid,
	git_index *index,
	git_signature *signature)
{
	git_tree *tree;
	git_oid tree_oid;
	git_repository *repo;

	repo = git_index_owner(index);

	cl_git_pass(git_index_write_tree(&tree_oid, index));

	cl_git_pass(git_tree_lookup(&tree, repo, &tree_oid));

	cl_git_pass(git_commit_create_v(
		commit_oid,
		repo,
		"HEAD",
		signature,
		signature,
		NULL,
		"Initial commit",
		tree,
		0));

	git_tree_free(tree);
}

void setup_stash(git_repository *repo, git_signature *signature)
{
	git_oid commit_oid;
	git_index *index;

	cl_git_pass(git_repository_index(&index, repo));

	cl_git_mkfile("stash/what", "hello\n");		/* ce013625030ba8dba906f756967f9e9ca394464a */
	cl_git_mkfile("stash/how", "small\n");		/* ac790413e2d7a26c3767e78c57bb28716686eebc */
	cl_git_mkfile("stash/who", "world\n");		/* cc628ccd10742baea8241c5924df992b5c019f71 */
	cl_git_mkfile("stash/when", "now\n");		/* b6ed15e81e2593d7bb6265eb4a991d29dc3e628b */
	cl_git_mkfile("stash/just.ignore", "me\n");	/* 78925fb1236b98b37a35e9723033e627f97aa88b */

	cl_git_mkfile("stash/.gitignore", "*.ignore\n");

	cl_git_pass(git_index_add_bypath(index, "what"));
	cl_git_pass(git_index_add_bypath(index, "how"));
	cl_git_pass(git_index_add_bypath(index, "who"));
	cl_git_pass(git_index_add_bypath(index, ".gitignore"));
	cl_git_pass(git_index_write(index));

	commit_staged_files(&commit_oid, index, signature);

	cl_git_rewritefile("stash/what", "goodbye\n");			/* dd7e1c6f0fefe118f0b63d9f10908c460aa317a6 */
	cl_git_rewritefile("stash/how", "not so small and\n");	/* e6d64adb2c7f3eb8feb493b556cc8070dca379a3 */
	cl_git_rewritefile("stash/who", "funky world\n");		/* a0400d4954659306a976567af43125a0b1aa8595 */

	cl_git_pass(git_index_add_bypath(index, "what"));
	cl_git_pass(git_index_add_bypath(index, "how"));
	cl_git_pass(git_index_write(index));

	cl_git_rewritefile("stash/what", "see you later\n");	/* bc99dc98b3eba0e9157e94769cd4d49cb49de449 */

	git_index_free(index);
}
