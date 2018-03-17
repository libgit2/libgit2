#include "clar_libgit2.h"
#include "repository.h"
#include "git2/sys/repository.h"
#include "mailmap_helpers.h"

static git_repository *g_repo;
static git_mailmap *g_mailmap;

void test_mailmap_parsing__initialize(void)
{
	g_repo = NULL;
	g_mailmap = NULL;
}

void test_mailmap_parsing__cleanup(void)
{
	cl_git_sandbox_cleanup();
	g_repo = NULL;

	git_mailmap_free(g_mailmap);
	g_mailmap = NULL;
}

static void check_mailmap_entries(
	const git_mailmap *mailmap, const mailmap_entry *entries, size_t entries_size)
{
	const git_mailmap_entry *parsed = NULL;
	size_t idx = 0;

	/* Check that the parsed entries match */
	cl_assert_equal_sz(entries_size, git_mailmap_entry_count(mailmap));
	for (idx = 0; idx < entries_size; ++idx) {
		parsed = git_mailmap_entry_byindex(mailmap, idx);
		cl_assert_equal_s(parsed->real_name, entries[idx].real_name);
		cl_assert_equal_s(parsed->real_email, entries[idx].real_email);
		cl_assert_equal_s(parsed->replace_name, entries[idx].replace_name);
		cl_assert_equal_s(parsed->replace_email, entries[idx].replace_email);
	}
}

static void check_mailmap_resolve(
	const git_mailmap *mailmap, const mailmap_entry *resolved, size_t resolved_size)
{
	const char *resolved_name = NULL;
	const char *resolved_email = NULL;
	size_t idx = 0;

	/* Check that the resolver behaves correctly */
	for (idx = 0; idx < resolved_size; ++idx) {
		git_mailmap_resolve(
			&resolved_name,
			&resolved_email,
			mailmap,
			resolved[idx].replace_name,
			resolved[idx].replace_email);
		cl_assert_equal_s(resolved_name, resolved[idx].real_name);
		cl_assert_equal_s(resolved_email, resolved[idx].real_email);
	}
}

void test_mailmap_parsing__string(void)
{
	cl_check_pass(git_mailmap_parse(
		&g_mailmap,
		string_mailmap,
		strlen(string_mailmap)));

	/* We should have parsed all of the entries */
	check_mailmap_entries(
		g_mailmap,
		entries, ARRAY_SIZE(entries));

	/* Check that resolving the entries works */
	check_mailmap_resolve(
		g_mailmap,
		resolved, ARRAY_SIZE(resolved));
	check_mailmap_resolve(
		g_mailmap,
		resolved_untracked, ARRAY_SIZE(resolved_untracked));
}

void test_mailmap_parsing__fromrepo(void)
{
	g_repo = cl_git_sandbox_init("mailmap");
	cl_check(!git_repository_is_bare(g_repo));

	cl_check_pass(git_mailmap_from_repo(&g_mailmap, g_repo));

	/* We should have parsed all of the entries */
	check_mailmap_entries(
		g_mailmap,
		entries, ARRAY_SIZE(entries));

	/* Check that resolving the entries works */
	check_mailmap_resolve(
		g_mailmap,
		resolved, ARRAY_SIZE(resolved));
	check_mailmap_resolve(
		g_mailmap,
		resolved_untracked, ARRAY_SIZE(resolved_untracked));
}

void test_mailmap_parsing__frombare(void)
{
	g_repo = cl_git_sandbox_init("mailmap/.gitted");
	cl_check_pass(git_repository_set_bare(g_repo));
	cl_check(git_repository_is_bare(g_repo));

	cl_check_pass(git_mailmap_from_repo(&g_mailmap, g_repo));

	/* We should have parsed all of the entries, except for the untracked one */
	check_mailmap_entries(
		g_mailmap,
		entries, ARRAY_SIZE(entries) - 1);

	/* Check that resolving the entries works */
	check_mailmap_resolve(
		g_mailmap,
		resolved, ARRAY_SIZE(resolved));
	check_mailmap_resolve(
		g_mailmap,
		resolved_bare, ARRAY_SIZE(resolved_bare));
}
