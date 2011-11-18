#include "clay_libgit2.h"
#include "posix.h"

static void file_create(const char *filename, const char *content)
{
	int fd;

	fd = p_creat(filename, 0666);
	cl_assert(fd != 0);
	cl_git_pass(p_write(fd, content, strlen(content)));
	cl_git_pass(p_close(fd))
}

void test_index_rename__single_file(void)
{
	git_repository *repo;
	git_index *index;
	int position;
	git_oid expected;
	git_index_entry *entry;

	p_mkdir("rename", 0700);

	cl_git_pass(git_repository_init(&repo, "./rename", 0));
	cl_git_pass(git_repository_index(&index, repo));

	cl_assert(git_index_entrycount(index) == 0);

	file_create("./rename/lame.name.txt", "new_file\n");

	/* This should add a new blob to the object database in 'd4/fa8600b4f37d7516bef4816ae2c64dbf029e3a' */
	cl_git_pass(git_index_add(index, "lame.name.txt", 0));
	cl_assert(git_index_entrycount(index) == 1);

	cl_git_pass(git_oid_fromstr(&expected, "d4fa8600b4f37d7516bef4816ae2c64dbf029e3a"));

	position = git_index_find(index, "lame.name.txt");

	entry = git_index_get(index, position);
	cl_assert(git_oid_cmp(&expected, &entry->oid) == 0);

	/* This removes the entry from the index, but not from the object database */
	cl_git_pass(git_index_remove(index, position));
	cl_assert(git_index_entrycount(index) == 0);

	p_rename("./rename/lame.name.txt", "./rename/fancy.name.txt");

	cl_git_pass(git_index_add(index, "fancy.name.txt", 0));
	cl_assert(git_index_entrycount(index) == 1);

	position = git_index_find(index, "fancy.name.txt");

	entry = git_index_get(index, position);
	cl_assert(git_oid_cmp(&expected, &entry->oid) == 0);

	git_index_free(index);
	git_repository_free(repo);

	cl_fixture_cleanup("rename");
}
