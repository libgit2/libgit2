#include "clar_libgit2.h"
#include "refs.h"
#include "vector.h"

static git_repository *repo;

void test_refs_iterator__initialize(void)
{
	cl_git_pass(git_repository_open(&repo, cl_fixture("testrepo.git")));
}

void test_refs_iterator__cleanup(void)
{
	git_repository_free(repo);
}

static const char *refnames[] = {
	"refs/heads/br2",
	"refs/heads/cannot-fetch",
	"refs/heads/chomped",
	"refs/heads/haacked",
	"refs/heads/master",
	"refs/heads/not-good",
	"refs/heads/packed",
	"refs/heads/packed-test",
	"refs/heads/subtrees",
	"refs/heads/test",
	"refs/heads/track-local",
	"refs/heads/trailing",
	"refs/notes/fanout",
	"refs/remotes/test/master",
	"refs/tags/annotated_tag_to_blob",
	"refs/tags/e90810b",
	"refs/tags/hard_tag",
	"refs/tags/point_to_blob",
	"refs/tags/taggerless",
	"refs/tags/test",
	"refs/tags/wrapped_tag",
};

void test_refs_iterator__list(void)
{
	git_reference_iterator *iter;
	git_vector output;
	char *refname;
	int error;
	size_t i;

	cl_git_pass(git_vector_init(&output, 32, git__strcmp_cb));
	cl_git_pass(git_reference_iterator_new(&iter, repo));

	do {
		const char *name;
		error = git_reference_next(&name, iter);
		cl_assert(error == 0 || error == GIT_ITEROVER);
		if (error != GIT_ITEROVER) {
			char *dup = git__strdup(name);
			cl_assert(dup != NULL);
			cl_git_pass(git_vector_insert(&output, dup));
		}
	} while (!error);

	cl_assert_equal_i(output.length, ARRAY_SIZE(refnames));

	git_vector_sort(&output);
	git_vector_foreach(&output, i, refname) {
		cl_assert_equal_s(refname, refnames[i]);
	}

	git_reference_iterator_free(iter);

	git_vector_foreach(&output, i, refname) {
		git__free(refname);
	}
	git_vector_free(&output);
}

void test_refs_iterator__empty(void)
{
	git_reference_iterator *iter;
	git_odb *odb;
	const char *name;
	git_repository *empty;

	cl_git_pass(git_odb_new(&odb));
	cl_git_pass(git_repository_wrap_odb(&empty, odb));

	cl_git_pass(git_reference_iterator_new(&iter, empty));
	cl_assert_equal_i(GIT_ITEROVER, git_reference_next(&name, iter));

	git_reference_iterator_free(iter);
	git_odb_free(odb);
	git_repository_free(empty);
}
