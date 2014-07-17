#include "clar_libgit2.h"
#include "git2/rebase.h"
#include "posix.h"

#include <fcntl.h>

static git_repository *repo;
static git_signature *signature;

// Fixture setup and teardown
void test_rebase_merge__initialize(void)
{
	repo = cl_git_sandbox_init("rebase");
}

void test_rebase_merge__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_rebase_merge__next(void)
{
	git_reference *branch_ref, *upstream_ref;
	git_merge_head *branch_head, *upstream_head;
	git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
	git_status_list *status_list;
	const git_status_entry *status_entry;
	git_oid file1_id;

	checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;

	cl_git_pass(git_reference_lookup(&branch_ref, repo, "refs/heads/beef"));
	cl_git_pass(git_reference_lookup(&upstream_ref, repo, "refs/heads/master"));

	cl_git_pass(git_merge_head_from_ref(&branch_head, repo, branch_ref));
	cl_git_pass(git_merge_head_from_ref(&upstream_head, repo, upstream_ref));

	cl_git_pass(git_rebase(repo, branch_head, upstream_head, NULL, signature, NULL));

	cl_git_pass(git_rebase_next(repo, &checkout_opts));

	cl_assert_equal_file("da9c51a23d02d931a486f45ad18cda05cf5d2b94\n", 41, "rebase/.git/rebase-merge/current");
	cl_assert_equal_file("1\n", 2, "rebase/.git/rebase-merge/msgnum");

	cl_git_pass(git_status_list_new(&status_list, repo, NULL));
	cl_assert_equal_i(1, git_status_list_entrycount(status_list));
	cl_assert(status_entry = git_status_byindex(status_list, 0));

	cl_assert_equal_s("beef.txt", status_entry->head_to_index->new_file.path);

	git_oid_fromstr(&file1_id, "8d95ea62e621f1d38d230d9e7d206e41096d76af");
	cl_assert_equal_oid(&file1_id, &status_entry->head_to_index->new_file.id);

	git_status_list_free(status_list);
	git_merge_head_free(branch_head);
	git_merge_head_free(upstream_head);
	git_reference_free(branch_ref);
	git_reference_free(upstream_ref);
}

void test_rebase_merge__next_with_conflicts(void)
{
	git_reference *branch_ref, *upstream_ref;
	git_merge_head *branch_head, *upstream_head;
	git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
	git_status_list *status_list;
	const git_status_entry *status_entry;

	const char *expected_merge =
"ASPARAGUS SOUP.\n"
"\n"
"<<<<<<< master\n"
"TAKE FOUR LARGE BUNCHES of asparagus, scrape it nicely, cut off one inch\n"
"OF THE TOPS, and lay them in water, chop the stalks and put them on the\n"
"FIRE WITH A PIECE OF BACON, a large onion cut up, and pepper and salt;\n"
"ADD TWO QUARTS OF WATER, boil them till the stalks are quite soft, then\n"
"PULP THEM THROUGH A SIEVE, and strain the water to it, which must be put\n"
"=======\n"
"Take four large bunches of asparagus, scrape it nicely, CUT OFF ONE INCH\n"
"of the tops, and lay them in water, chop the stalks and PUT THEM ON THE\n"
"fire with a piece of bacon, a large onion cut up, and pepper and salt;\n"
"add two quarts of water, boil them till the stalks are quite soft, then\n"
"pulp them through a sieve, and strain the water to it, which must be put\n"
">>>>>>> Conflicting modification 1 to asparagus\n"
"back in the pot; put into it a chicken cut up, with the tops of\n"
"asparagus which had been laid by, boil it until these last articles are\n"
"sufficiently done, thicken with flour, butter and milk, and serve it up.\n";

	checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;

	cl_git_pass(git_reference_lookup(&branch_ref, repo, "refs/heads/asparagus"));
	cl_git_pass(git_reference_lookup(&upstream_ref, repo, "refs/heads/master"));

	cl_git_pass(git_merge_head_from_ref(&branch_head, repo, branch_ref));
	cl_git_pass(git_merge_head_from_ref(&upstream_head, repo, upstream_ref));

	cl_git_pass(git_rebase(repo, branch_head, upstream_head, NULL, signature, NULL));

	cl_git_pass(git_rebase_next(repo, &checkout_opts));

	cl_assert_equal_file("33f915f9e4dbd9f4b24430e48731a59b45b15500\n", 41, "rebase/.git/rebase-merge/current");
	cl_assert_equal_file("1\n", 2, "rebase/.git/rebase-merge/msgnum");

	cl_git_pass(git_status_list_new(&status_list, repo, NULL));
	cl_assert_equal_i(1, git_status_list_entrycount(status_list));
	cl_assert(status_entry = git_status_byindex(status_list, 0));

	cl_assert_equal_s("asparagus.txt", status_entry->head_to_index->new_file.path);

	cl_assert_equal_file(expected_merge, strlen(expected_merge), "rebase/asparagus.txt");

	git_status_list_free(status_list);
	git_merge_head_free(branch_head);
	git_merge_head_free(upstream_head);
	git_reference_free(branch_ref);
	git_reference_free(upstream_ref);
}

