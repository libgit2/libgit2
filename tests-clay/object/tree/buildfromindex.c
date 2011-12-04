#include "clay_libgit2.h"
#include "posix.h"

static git_repository *repo;

static void file_create(const char *filename, const char *content)
{
	int fd;

	fd = p_creat(filename, 0666);
	cl_assert(fd != 0);
	cl_git_pass(p_write(fd, content, strlen(content)));
	cl_git_pass(p_close(fd));
}

void test_object_tree_buildfromindex__initialize(void)
{
	cl_fixture("treebuilder");
	cl_git_pass(git_repository_init(&repo, "treebuilder/", 0));
	cl_git_pass(git_repository_open(&repo, "treebuilder/.git"));
	cl_assert(repo != NULL);
}

void test_object_tree_buildfromindex__cleanup(void)
{
	git_repository_free(repo);
	cl_fixture_cleanup("treebuilder");
}

void test_object_tree_buildfromindex__generate_predictable_object_ids(void)
{
	git_index *index;
	git_oid blob_oid, tree_oid, expected_tree_oid;
	git_index_entry *entry;

	/*
	 * Add a new file to the index
	 */
	cl_git_pass(git_repository_index(&index, repo));

	file_create("treebuilder/test.txt", "test\n");
	cl_git_pass(git_index_add(index, "test.txt", 0));

	entry = git_index_get(index, 0);

	/* $ echo "test" | git hash-object --stdin */
	cl_git_pass(git_oid_fromstr(&blob_oid, "9daeafb9864cf43055ae93beb0afd6c7d144bfa4"));

	cl_assert(git_oid_cmp(&blob_oid, &entry->oid) == 0);

	/*
	 * Build the tree from the index
	 */
	cl_git_pass(git_tree_create_fromindex(&tree_oid, index));

	cl_git_pass(git_oid_fromstr(&expected_tree_oid, "2b297e643c551e76cfa1f93810c50811382f9117"));
	cl_assert(git_oid_cmp(&expected_tree_oid, &tree_oid) == 0);

	git_index_free(index);
}
