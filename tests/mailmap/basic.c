#include "clar.h"
#include "clar_libgit2.h"

#include "common.h"
#include "git2/mailmap.h"

static git_mailmap *mailmap = NULL;

const char TEST_MAILMAP[] =
	"Foo bar <foo@bar.com> <foo@baz.com>  \n"
	"Blatantly invalid line\n"
	"Foo bar <foo@bar.com> <foo@bal.com>\n"
	"<email@foo.com> <otheremail@foo.com>\n"
	"<email@foo.com> Other Name <yetanotheremail@foo.com>\n";

void test_mailmap_basic__initialize(void)
{
	cl_git_pass(git_mailmap_parse(&mailmap, TEST_MAILMAP, sizeof(TEST_MAILMAP) - 1));
}

void test_mailmap_basic__cleanup(void)
{
	if (mailmap) {
		git_mailmap_free(mailmap);
		mailmap = NULL;
	}
}

void test_mailmap_basic__entry(void)
{
	const git_mailmap_entry *entry;

	cl_assert(git_mailmap_entry_count(mailmap) == 4);

	entry = git_mailmap_entry_byindex(mailmap, 0);
	cl_assert(entry);
	cl_assert(!entry->replace_name);
	cl_assert(!git__strcmp(entry->replace_email, "foo@baz.com"));

	entry = git_mailmap_entry_byindex(mailmap, 10000);
	cl_assert(!entry);
}

void test_mailmap_basic__lookup_not_found(void)
{
	const git_mailmap_entry *entry = git_mailmap_entry_lookup(
		mailmap,
		"Whoever",
		"doesnotexist@fo.com");
	cl_assert(!entry);
}

void test_mailmap_basic__lookup(void)
{
	const git_mailmap_entry *entry = git_mailmap_entry_lookup(
		mailmap,
		"Typoed the name once",
		"foo@baz.com");
	cl_assert(entry);
	cl_assert(!git__strcmp(entry->real_name, "Foo bar"));
}

void test_mailmap_basic__empty_email_query(void)
{
	const char *name;
	const char *email;
	git_mailmap_resolve(
		&name,
		&email,
		mailmap,
		"Author name",
		"otheremail@foo.com");
	cl_assert(!git__strcmp(name, "Author name"));
	cl_assert(!git__strcmp(email, "email@foo.com"));
}

void test_mailmap_basic__name_matching(void)
{
	const char *name;
	const char *email;
	git_mailmap_resolve(
		&name,
		&email,
		mailmap,
		"Other Name",
		"yetanotheremail@foo.com");
	cl_assert(!git__strcmp(name, "Other Name"));
	cl_assert(!git__strcmp(email, "email@foo.com"));

	git_mailmap_resolve(
		&name,
		&email,
		mailmap,
		"Other Name That Doesn't Match",
		"yetanotheremail@foo.com");
	cl_assert(!git__strcmp(name, "Other Name That Doesn't Match"));
	cl_assert(!git__strcmp(email, "yetanotheremail@foo.com"));
}
