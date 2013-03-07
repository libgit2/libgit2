#include "clar_libgit2.h"
#include "refdb.h"
#include "repository.h"
#include "testdb.h"

#define TEST_REPO_PATH "testrepo"

static git_repository *repo;
static git_refdb *refdb;
static git_refdb_backend *refdb_backend;

int unlink_ref(void *payload, git_buf *file)
{
	GIT_UNUSED(payload);
	return p_unlink(git_buf_cstr(file));
}

int empty(void *payload, git_buf *file)
{
	GIT_UNUSED(payload);
	GIT_UNUSED(file);
	return -1;
}

int ref_file_foreach(git_repository *repo, int (* cb)(void *payload, git_buf *filename))
{
	const char *repo_path;
	git_buf repo_refs_dir = GIT_BUF_INIT;
	int error = 0;
	
	repo_path = git_repository_path(repo);

	git_buf_joinpath(&repo_refs_dir, repo_path, "HEAD");
	if (git_path_exists(git_buf_cstr(&repo_refs_dir)) &&
		cb(NULL, &repo_refs_dir) < 0)
		return -1;

	git_buf_joinpath(&repo_refs_dir, repo_path, "refs");
	git_buf_joinpath(&repo_refs_dir, git_buf_cstr(&repo_refs_dir), "heads");
	if (git_path_direach(&repo_refs_dir, cb, NULL) != 0)
		return -1;
	
	git_buf_joinpath(&repo_refs_dir, repo_path, "packed-refs");
	if (git_path_exists(git_buf_cstr(&repo_refs_dir)) &&
		cb(NULL, &repo_refs_dir) < 0)
		return -1;

	git_buf_free(&repo_refs_dir);

	return error;
}

void test_refdb_inmemory__initialize(void)
{
	git_buf repo_refs_dir = GIT_BUF_INIT;

	repo = cl_git_sandbox_init(TEST_REPO_PATH);

	cl_git_pass(git_repository_refdb(&refdb, repo));
	cl_git_pass(refdb_backend_test(&refdb_backend, repo));
	cl_git_pass(git_refdb_set_backend(refdb, refdb_backend));
	
	
	ref_file_foreach(repo, unlink_ref);

	git_buf_free(&repo_refs_dir);
}

void test_refdb_inmemory__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_refdb_inmemory__doesnt_write_ref_file(void)
{
	git_reference *ref;
	git_oid oid;
	
	cl_git_pass(git_oid_fromstr(&oid, "c47800c7266a2be04c571c04d5a6614691ea99bd"));
	cl_git_pass(git_reference_create(&ref, repo, GIT_REFS_HEADS_DIR "test1", &oid, 0));
	
	ref_file_foreach(repo, empty);

	git_reference_free(ref);
}

void test_refdb_inmemory__read(void)
{
	git_reference *write1, *write2, *write3, *read1, *read2, *read3;
	git_oid oid1, oid2, oid3;
	
	cl_git_pass(git_oid_fromstr(&oid1, "c47800c7266a2be04c571c04d5a6614691ea99bd"));
	cl_git_pass(git_reference_create(&write1, repo, GIT_REFS_HEADS_DIR "test1", &oid1, 0));
	
	cl_git_pass(git_oid_fromstr(&oid2, "e90810b8df3e80c413d903f631643c716887138d"));
	cl_git_pass(git_reference_create(&write2, repo, GIT_REFS_HEADS_DIR "test2", &oid2, 0));

	cl_git_pass(git_oid_fromstr(&oid3, "763d71aadf09a7951596c9746c024e7eece7c7af"));
	cl_git_pass(git_reference_create(&write3, repo, GIT_REFS_HEADS_DIR "test3", &oid3, 0));


	cl_git_pass(git_reference_lookup(&read1, repo, GIT_REFS_HEADS_DIR "test1"));
	cl_assert(strcmp(git_reference_name(read1), git_reference_name(write1)) == 0);
	cl_assert(git_oid_cmp(git_reference_target(read1), git_reference_target(write1)) == 0);

	cl_git_pass(git_reference_lookup(&read2, repo, GIT_REFS_HEADS_DIR "test2"));
	cl_assert(strcmp(git_reference_name(read2), git_reference_name(write2)) == 0);
	cl_assert(git_oid_cmp(git_reference_target(read2), git_reference_target(write2)) == 0);

	cl_git_pass(git_reference_lookup(&read3, repo, GIT_REFS_HEADS_DIR "test3"));
	cl_assert(strcmp(git_reference_name(read3), git_reference_name(write3)) == 0);
	cl_assert(git_oid_cmp(git_reference_target(read3), git_reference_target(write3)) == 0);

	git_reference_free(write1);
	git_reference_free(write2);
	git_reference_free(write3);

	git_reference_free(read1);
	git_reference_free(read2);
	git_reference_free(read3);
}

int foreach_test(const char *ref_name, void *payload)
{
	git_reference *ref;
	git_oid expected;
	int *i = payload;
	
	cl_git_pass(git_reference_lookup(&ref, repo, ref_name));

	if (*i == 0)
		cl_git_pass(git_oid_fromstr(&expected, "c47800c7266a2be04c571c04d5a6614691ea99bd"));
	else if (*i == 1)
		cl_git_pass(git_oid_fromstr(&expected, "e90810b8df3e80c413d903f631643c716887138d"));
	else if (*i == 2)
		cl_git_pass(git_oid_fromstr(&expected, "763d71aadf09a7951596c9746c024e7eece7c7af"));

	cl_assert(git_oid_cmp(&expected, &ref->target.oid) == 0);
	
	++(*i);
	
	git_reference_free(ref);

	return 0;
}

void test_refdb_inmemory__foreach(void)
{
	git_reference *write1, *write2, *write3;
	git_oid oid1, oid2, oid3;
	size_t i = 0;
	
	cl_git_pass(git_oid_fromstr(&oid1, "c47800c7266a2be04c571c04d5a6614691ea99bd"));
	cl_git_pass(git_reference_create(&write1, repo, GIT_REFS_HEADS_DIR "test1", &oid1, 0));
	
	cl_git_pass(git_oid_fromstr(&oid2, "e90810b8df3e80c413d903f631643c716887138d"));
	cl_git_pass(git_reference_create(&write2, repo, GIT_REFS_HEADS_DIR "test2", &oid2, 0));
	
	cl_git_pass(git_oid_fromstr(&oid3, "763d71aadf09a7951596c9746c024e7eece7c7af"));
	cl_git_pass(git_reference_create(&write3, repo, GIT_REFS_HEADS_DIR "test3", &oid3, 0));
	
	cl_git_pass(git_reference_foreach(repo, GIT_REF_LISTALL, foreach_test, &i));	
	cl_assert(i == 3);
	
	git_reference_free(write1);
	git_reference_free(write2);
	git_reference_free(write3);
}

int delete_test(const char *ref_name, void *payload)
{
	git_reference *ref;
	git_oid expected;
	int *i = payload;
	
	cl_git_pass(git_reference_lookup(&ref, repo, ref_name));
	
	cl_git_pass(git_oid_fromstr(&expected, "e90810b8df3e80c413d903f631643c716887138d"));	
	cl_assert(git_oid_cmp(&expected, &ref->target.oid) == 0);
	
	++(*i);
	
	git_reference_free(ref);
	
	return 0;
}

void test_refdb_inmemory__delete(void)
{
	git_reference *write1, *write2, *write3;
	git_oid oid1, oid2, oid3;
	size_t i = 0;
	
	cl_git_pass(git_oid_fromstr(&oid1, "c47800c7266a2be04c571c04d5a6614691ea99bd"));
	cl_git_pass(git_reference_create(&write1, repo, GIT_REFS_HEADS_DIR "test1", &oid1, 0));
	
	cl_git_pass(git_oid_fromstr(&oid2, "e90810b8df3e80c413d903f631643c716887138d"));
	cl_git_pass(git_reference_create(&write2, repo, GIT_REFS_HEADS_DIR "test2", &oid2, 0));
	
	cl_git_pass(git_oid_fromstr(&oid3, "763d71aadf09a7951596c9746c024e7eece7c7af"));
	cl_git_pass(git_reference_create(&write3, repo, GIT_REFS_HEADS_DIR "test3", &oid3, 0));
	
	git_reference_delete(write1);
	git_reference_free(write1);
	
	git_reference_delete(write3);
	git_reference_free(write3);
	
	cl_git_pass(git_reference_foreach(repo, GIT_REF_LISTALL, delete_test, &i));
	cl_assert(i == 1);

	git_reference_free(write2);
}
