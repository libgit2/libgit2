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

static int refcmp_cb(const void *a, const void *b)
{
	const git_reference *refa = (const git_reference *)a;
	const git_reference *refb = (const git_reference *)b;

	return strcmp(refa->name, refb->name);
}

void test_refs_iterator__list(void)
{
	git_reference_iterator *iter;
	git_vector output;
	git_reference *ref;
	int error;
	size_t i;

	cl_git_pass(git_vector_init(&output, 32, &refcmp_cb));
	cl_git_pass(git_reference_iterator_new(&iter, repo));

	do {
		error = git_reference_next(&ref, iter);
		cl_assert(error == 0 || error == GIT_ITEROVER);
		if (error != GIT_ITEROVER) {
			cl_git_pass(git_vector_insert(&output, ref));
		}
	} while (!error);

	git_reference_iterator_free(iter);
	cl_assert_equal_sz(output.length, ARRAY_SIZE(refnames));

	git_vector_sort(&output);

	git_vector_foreach(&output, i, ref) {
		cl_assert_equal_s(ref->name, refnames[i]);
		git_reference_free(ref);
	}

	git_vector_free(&output);
}

void test_refs_iterator__empty(void)
{
	git_reference_iterator *iter;
	git_odb *odb;
	git_reference *ref;
	git_repository *empty;

	cl_git_pass(git_odb_new(&odb));
	cl_git_pass(git_repository_wrap_odb(&empty, odb));

	cl_git_pass(git_reference_iterator_new(&iter, empty));
	cl_assert_equal_i(GIT_ITEROVER, git_reference_next(&ref, iter));

	git_reference_iterator_free(iter);
	git_odb_free(odb);
	git_repository_free(empty);
}
