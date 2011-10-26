#include "clay_libgit2.h"
#include "tree.h"
#include "repository.h"

static unsigned int expect_idx;
static git_repository *repo;
static git_tree *atree, *btree;
static git_oid aoid, boid;

static void diff_cmp(const git_tree_diff_data *a, const git_tree_diff_data *b)
{
	cl_assert(a->old_attr - b->old_attr == 0);

	cl_assert(a->new_attr - b->new_attr == 0);

	cl_assert(git_oid_cmp(&a->old_oid, &b->old_oid) == 0);
	cl_assert(git_oid_cmp(&a->new_oid, &b->new_oid) == 0);

	cl_assert(a->status - b->status == 0);

	cl_assert(strcmp(a->path, b->path) == 0);
}

static int diff_cb(const git_tree_diff_data *diff, void *data)
{
	diff_cmp(diff, data);
	return GIT_SUCCESS;
}

void test_object_tree_diff__initialize(void)
{
	cl_git_pass(git_repository_open(&repo, cl_fixture("testrepo.git")));
}

void test_object_tree_diff__cleanup(void)
{
	git_tree_free(atree);
	git_tree_free(btree);
	git_repository_free(repo);
}

void test_object_tree_diff__addition(void)
{
	char *astr = "181037049a54a1eb5fab404658a3a250b44335d7";
	char *bstr = "f60079018b664e4e79329a7ef9559c8d9e0378d1";
	git_tree_diff_data expect;

	memset(&expect, 0x0, sizeof(git_tree_diff_data));
	expect.old_attr = 0;
	expect.new_attr = 0100644;
	git_oid_fromstr(&expect.new_oid, "fa49b077972391ad58037050f2a75f74e3671e92");
	expect.status = GIT_STATUS_ADDED;
	expect.path = "new.txt";

	cl_must_pass(git_oid_fromstr(&aoid, astr));
	cl_must_pass(git_oid_fromstr(&boid, bstr));

	cl_must_pass(git_tree_lookup(&atree, repo, &aoid));
	cl_must_pass(git_tree_lookup(&btree, repo, &boid));

	cl_must_pass(git_tree_diff(atree, btree, diff_cb, &expect));
}

void test_object_tree_diff__deletion(void)
{
	char *astr = "f60079018b664e4e79329a7ef9559c8d9e0378d1";
	char *bstr = "181037049a54a1eb5fab404658a3a250b44335d7";
	git_tree_diff_data expect;

	memset(&expect, 0x0, sizeof(git_tree_diff_data));
	expect.old_attr = 0100644;
	expect.new_attr = 0;
	git_oid_fromstr(&expect.old_oid, "fa49b077972391ad58037050f2a75f74e3671e92");
	expect.status = GIT_STATUS_DELETED;
	expect.path = "new.txt";
	cl_must_pass(git_oid_fromstr(&aoid, astr));
	cl_must_pass(git_oid_fromstr(&boid, bstr));

	cl_must_pass(git_tree_lookup(&atree, repo, &aoid));
	cl_must_pass(git_tree_lookup(&btree, repo, &boid));

	cl_must_pass(git_tree_diff(atree, btree, diff_cb, &expect));
}

void test_object_tree_diff__modification(void)
{
	char *astr = "1810dff58d8a660512d4832e740f692884338ccd";
	char *bstr = "944c0f6e4dfa41595e6eb3ceecdb14f50fe18162";
	git_tree_diff_data expect;

	expect.old_attr = 0100644;
	expect.new_attr = 0100644;
	git_oid_fromstr(&expect.old_oid, "45b983be36b73c0788dc9cbcb76cbb80fc7bb057");
	git_oid_fromstr(&expect.new_oid, "3697d64be941a53d4ae8f6a271e4e3fa56b022cc");
	expect.status = GIT_STATUS_MODIFIED;
	expect.path = "branch_file.txt";

	cl_must_pass(git_oid_fromstr(&aoid, astr));
	cl_must_pass(git_oid_fromstr(&boid, bstr));

	cl_must_pass(git_tree_lookup(&atree, repo, &aoid));
	cl_must_pass(git_tree_lookup(&btree, repo, &boid));

	cl_must_pass(git_tree_diff(atree, btree, diff_cb, &expect));
}

static int diff_more_cb(const git_tree_diff_data *diff, void *data)
{
	git_tree_diff_data *expect = (git_tree_diff_data *) data;
	diff_cmp(diff, &expect[expect_idx++]);

	return GIT_SUCCESS;
}

void test_object_tree_diff__more(void)
{
	char *astr = "814889a078c031f61ed08ab5fa863aea9314344d";
	char *bstr = "75057dd4114e74cca1d750d0aee1647c903cb60a";
	git_tree_diff_data expect[3];

	memset(expect, 0x0, 3 * sizeof(git_tree_diff_data));
	/* M README */
	expect[0].old_attr = 0100644;
	expect[0].new_attr = 0100644;
	git_oid_fromstr(&expect[0].old_oid, "a8233120f6ad708f843d861ce2b7228ec4e3dec6");
	git_oid_fromstr(&expect[0].new_oid, "1385f264afb75a56a5bec74243be9b367ba4ca08");
	expect[0].status = GIT_STATUS_MODIFIED;
	expect[0].path = "README";
	/* A branch_file.txt */
	expect[1].old_attr = 0;
	expect[1].new_attr = 0100644;
	git_oid_fromstr(&expect[1].new_oid, "45b983be36b73c0788dc9cbcb76cbb80fc7bb057");
	expect[1].status = GIT_STATUS_ADDED;
	expect[1].path = "branch_file.txt";
	/* M new.txt */
	expect[2].old_attr = 0100644;
	expect[2].new_attr = 0100644;
	git_oid_fromstr(&expect[2].old_oid, "a71586c1dfe8a71c6cbf6c129f404c5642ff31bd");
	git_oid_fromstr(&expect[2].new_oid, "fa49b077972391ad58037050f2a75f74e3671e92");
	expect[2].status = GIT_STATUS_MODIFIED;
	expect[2].path = "new.txt";

	cl_must_pass(git_oid_fromstr(&aoid, astr));
	cl_must_pass(git_oid_fromstr(&boid, bstr));

	cl_must_pass(git_tree_lookup(&atree, repo, &aoid));
	cl_must_pass(git_tree_lookup(&btree, repo, &boid));

	cl_must_pass(git_tree_diff(atree, btree, diff_more_cb, expect));
}
