#include "clar_libgit2.h"
#include "git2/sys/commit.h"

static git_remote *_remote;
static git_repository *_repo, *_dummy;

void test_network_remote_tag__initialize(void)
{
	cl_fixture_sandbox("testrepo.git");
	git_repository_open(&_repo, "testrepo.git");

	/* We need a repository to have a remote */
	cl_git_pass(git_repository_init(&_dummy, "dummytag.git", true));
	cl_git_pass(git_remote_create(&_remote, _dummy, "origin", cl_git_path_url("testrepo.git")));
}

void test_network_remote_tag__cleanup(void)
{
	git_remote_free(_remote);
	_remote = NULL;

	git_repository_free(_repo);
	_repo = NULL;

	git_repository_free(_dummy);
	_dummy = NULL;

	cl_fixture_cleanup("testrepo.git");
	cl_fixture_cleanup("dummytag.git");
}

/*
 * Create one commit, one tree, one blob.
 * Create two tags: one for the commit, one for the blob.
 */
static void create_commit_with_tags(git_reference **out, git_oid *out_commit_tag_id, git_oid *out_blob_tag_id, git_repository *repo)
{
	git_treebuilder *treebuilder;
	git_oid blob_id, tree_id, commit_id;
	git_signature *sig;
	git_object *target;

	cl_git_pass(git_treebuilder_new(&treebuilder, repo, NULL));

	cl_git_pass(git_blob_create_from_buffer(&blob_id, repo, "", 0));
	cl_git_pass(git_treebuilder_insert(NULL, treebuilder, "README.md", &blob_id, 0100644));
	cl_git_pass(git_treebuilder_write(&tree_id, treebuilder));

	cl_git_pass(git_signature_now(&sig, "Pusher Joe", "pjoe"));
	cl_git_pass(git_commit_create_from_ids(&commit_id, repo, NULL, sig, sig,
					       NULL, "Tree with tags\n", &tree_id, 0, NULL));
	cl_git_pass(git_reference_create(out, repo, "refs/heads/tree-with-tags", &commit_id, true, "commit yo"));

	cl_git_pass(git_object_lookup(&target, repo, &commit_id, GIT_OBJECT_COMMIT));
	cl_git_pass(git_tag_create_lightweight(out_commit_tag_id, repo, "tagged-commit", target, true));
	git_object_free(target);

	cl_git_pass(git_object_lookup(&target, repo, &blob_id, GIT_OBJECT_BLOB));
	cl_git_pass(git_tag_create_lightweight(out_blob_tag_id, repo, "tagged-blob", target, true));
	git_object_free(target);

	git_treebuilder_free(treebuilder);
	git_signature_free(sig);
}

void test_network_remote_tag__push_different_tag_types(void)
{
	git_push_options opts = GIT_PUSH_OPTIONS_INIT;
	git_reference *ref;
	git_oid commit_tag_id, blob_tag_id;
	char* refspec_tree = "refs/heads/tree-with-tags";
	char* refspec_tagged_commit = "refs/tags/tagged-commit";
	char* refspec_tagged_blob = "refs/tags/tagged-blob";
	const git_strarray refspecs_tree = { &refspec_tree, 1 };
	const git_strarray refspecs_tagged_commit = { &refspec_tagged_commit, 1 };
	const git_strarray refspecs_tagged_blob = { &refspec_tagged_blob, 1 };

	create_commit_with_tags(&ref, &commit_tag_id, &blob_tag_id, _dummy);

	/* Push tree */
	cl_git_pass(git_remote_push(_remote, &refspecs_tree, &opts));
	git_reference_free(ref);
	cl_git_pass(git_reference_lookup(&ref, _repo, "refs/heads/tree-with-tags"));
	git_reference_free(ref);

	/* Push tagged commit */
	cl_git_pass(git_remote_push(_remote, &refspecs_tagged_commit, &opts));
	cl_git_pass(git_reference_name_to_id(&commit_tag_id, _repo, "refs/tags/tagged-commit"));

	/* Push tagged blob */
	cl_git_pass(git_remote_push(_remote, &refspecs_tagged_blob, &opts));
	cl_git_pass(git_reference_name_to_id(&blob_tag_id, _repo, "refs/tags/tagged-blob"));
}
