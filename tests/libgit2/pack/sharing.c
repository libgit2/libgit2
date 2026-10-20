#include "clar_libgit2.h"
#include <git2.h>
#include "mwindow.h"
#include "pack.h"
#include "hashmap.h"

extern git_mwindow_packmap git_mwindow__pack_cache;

void test_pack_sharing__open_two_repos(void)
{
	git_repository *repo1, *repo2;
	git_object *obj1, *obj2;
	git_oid id;
	struct git_pack_file *pack;
	git_hashmap_iter_t iter = GIT_HASHMAP_ITER_INIT;

	cl_git_pass(git_repository_open(&repo1, cl_fixture("testrepo.git")));
	cl_git_pass(git_repository_open(&repo2, cl_fixture("testrepo.git")));

	git_oid_from_string(&id, "a65fedf39aefe402d3bb6e24df4d4f5fe4547750", GIT_OID_SHA1);

	cl_git_pass(git_object_lookup(&obj1, repo1, &id, GIT_OBJECT_ANY));
	cl_git_pass(git_object_lookup(&obj2, repo2, &id, GIT_OBJECT_ANY));

	while (git_mwindow_packmap_iterate(&iter, NULL, &pack, &git_mwindow__pack_cache) == 0)
		cl_assert_equal_i(2, pack->refcount.val);

	cl_assert_equal_i(3, git_mwindow_packmap_size(&git_mwindow__pack_cache));

	git_object_free(obj1);
	git_object_free(obj2);
	git_repository_free(repo1);
	git_repository_free(repo2);

	/* we don't want to keep the packs open after the repos go away */
	cl_assert_equal_i(0, git_mwindow_packmap_size(&git_mwindow__pack_cache));
}

static git_repository *reenter_repo = NULL;
static bool reenter_active = false;

static void *reentrant_malloc(size_t len, const char *file, int line)
{
	GIT_UNUSED(file);
	GIT_UNUSED(line);

	if (reenter_active && reenter_repo && len >= sizeof(struct git_pack_file)) {
		git_repository *repo = reenter_repo;
		reenter_active = false;
		reenter_repo = NULL;
		git_repository_free(repo);
	}

	return malloc(len);
}

static void *reentrant_realloc(void *ptr, size_t size, const char *file, int line)
{
	GIT_UNUSED(file);
	GIT_UNUSED(line);
	return realloc(ptr, size);
}

static void reentrant_free(void *ptr)
{
	free(ptr);
}

void test_pack_sharing__allocator_reentrancy(void)
{
	git_repository *repo1, *repo2;
	git_object *obj1, *obj2;
	git_oid id1, id2;
	git_allocator alloc = { reentrant_malloc, reentrant_realloc, reentrant_free };

	cl_git_pass(git_repository_open(&repo1, cl_fixture("testrepo.git")));
	git_oid_from_string(&id1, "a65fedf39aefe402d3bb6e24df4d4f5fe4547750", GIT_OID_SHA1);
	cl_git_pass(git_object_lookup(&obj1, repo1, &id1, GIT_OBJECT_ANY));
	git_object_free(obj1);

	cl_git_pass(git_repository_open(&repo2, cl_fixture("peeled.git")));

	cl_git_pass(git_libgit2_opts(GIT_OPT_SET_ALLOCATOR, &alloc));
	reenter_repo = repo1;
	reenter_active = true;

	/*
	 * Looking up an object in repo2 will load its packfile via
	 * git_mwindow_get_pack -> git_packfile_alloc -> git__calloc ->
	 * reentrant_malloc, which frees repo1 (simulating a GC finalizer).
	 */
	git_oid_from_string(&id2, "0df1a5865c8abfc09f1f2182e6a31be550e99f07", GIT_OID_SHA1);
	cl_git_pass(git_object_lookup(&obj2, repo2, &id2, GIT_OBJECT_ANY));

	cl_git_pass(git_libgit2_opts(GIT_OPT_SET_ALLOCATOR, NULL));

	git_object_free(obj2);
	git_repository_free(repo2);
}
