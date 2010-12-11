#include "test_lib.h"
#include "test_helpers.h"
#include "commit.h"
#include "person.h"
#include <git2/odb.h>
#include <git2/commit.h>
#include <git2/revwalk.h>

static char *test_commits_broken[] = {

/* empty commit */
"",

/* random garbage */
"asd97sa9du902e9a0jdsuusad09as9du098709aweu8987sd\n",

/* broken endlines 1 */
"tree f6c0dad3c7b3481caa9d73db21f91964894a945b\r\n\
parent 05452d6349abcd67aa396dfb28660d765d8b2a36\r\n\
author Vicent Marti <tanoku@gmail.com> 1273848544 +0200\r\n\
committer Vicent Marti <tanoku@gmail.com> 1273848544 +0200\r\n\
\r\n\
a test commit with broken endlines\r\n",

/* broken endlines 2 */
"tree f6c0dad3c7b3481caa9d73db21f91964894a945b\
parent 05452d6349abcd67aa396dfb28660d765d8b2a36\
author Vicent Marti <tanoku@gmail.com> 1273848544 +0200\
committer Vicent Marti <tanoku@gmail.com> 1273848544 +0200\
\
another test commit with broken endlines",

/* starting endlines */
"\ntree f6c0dad3c7b3481caa9d73db21f91964894a945b\n\
parent 05452d6349abcd67aa396dfb28660d765d8b2a36\n\
author Vicent Marti <tanoku@gmail.com> 1273848544 +0200\n\
committer Vicent Marti <tanoku@gmail.com> 1273848544 +0200\n\
\n\
a test commit with a starting endline\n",

/* corrupted commit 1 */
"tree f6c0dad3c7b3481caa9d73db21f91964894a945b\n\
parent 05452d6349abcd67aa396df",

/* corrupted commit 2 */
"tree f6c0dad3c7b3481caa9d73db21f91964894a945b\n\
parent ",

/* corrupted commit 3 */
"tree f6c0dad3c7b3481caa9d73db21f91964894a945b\n\
parent ",

/* corrupted commit 4 */
"tree f6c0dad3c7b3481caa9d73db21f91964894a945b\n\
par",

};


static char *test_commits_working[] = {
/* simple commit with no message */
"tree 1810dff58d8a660512d4832e740f692884338ccd\n\
author Vicent Marti <tanoku@gmail.com> 1273848544 +0200\n\
committer Vicent Marti <tanoku@gmail.com> 1273848544 +0200\n\
\n",

/* simple commit, no parent */
"tree 1810dff58d8a660512d4832e740f692884338ccd\n\
author Vicent Marti <tanoku@gmail.com> 1273848544 +0200\n\
committer Vicent Marti <tanoku@gmail.com> 1273848544 +0200\n\
\n\
a simple commit which works\n",

/* simple commit, no parent, no newline in message */
"tree 1810dff58d8a660512d4832e740f692884338ccd\n\
author Vicent Marti <tanoku@gmail.com> 1273848544 +0200\n\
committer Vicent Marti <tanoku@gmail.com> 1273848544 +0200\n\
\n\
a simple commit which works",

/* simple commit, 1 parent */
"tree 1810dff58d8a660512d4832e740f692884338ccd\n\
parent e90810b8df3e80c413d903f631643c716887138d\n\
author Vicent Marti <tanoku@gmail.com> 1273848544 +0200\n\
committer Vicent Marti <tanoku@gmail.com> 1273848544 +0200\n\
\n\
a simple commit which works\n",
};

BEGIN_TEST(parse_oid_test)

	git_oid oid;

#define TEST_OID_PASS(string, header) { \
	char *ptr = string;\
	char *ptr_original = ptr;\
	size_t len = strlen(ptr);\
	must_pass(git__parse_oid(&oid, &ptr, ptr + len, header));\
	must_be_true(ptr == ptr_original + len);\
}

#define TEST_OID_FAIL(string, header) { \
	char *ptr = string;\
	size_t len = strlen(ptr);\
	must_fail(git__parse_oid(&oid, &ptr, ptr + len, header));\
}

	TEST_OID_PASS("parent 05452d6349abcd67aa396dfb28660d765d8b2a36\n", "parent ");
	TEST_OID_PASS("tree 05452d6349abcd67aa396dfb28660d765d8b2a36\n", "tree ");
	TEST_OID_PASS("random_heading 05452d6349abcd67aa396dfb28660d765d8b2a36\n", "random_heading ");
	TEST_OID_PASS("stuck_heading05452d6349abcd67aa396dfb28660d765d8b2a36\n", "stuck_heading");
	TEST_OID_PASS("tree 5F4BEFFC0759261D015AA63A3A85613FF2F235DE\n", "tree ");
	TEST_OID_PASS("tree 1A669B8AB81B5EB7D9DB69562D34952A38A9B504\n", "tree ");
	TEST_OID_PASS("tree 5B20DCC6110FCC75D31C6CEDEBD7F43ECA65B503\n", "tree ");
	TEST_OID_PASS("tree 173E7BF00EA5C33447E99E6C1255954A13026BE4\n", "tree ");

	TEST_OID_FAIL("parent 05452d6349abcd67aa396dfb28660d765d8b2a36", "parent ");
	TEST_OID_FAIL("05452d6349abcd67aa396dfb28660d765d8b2a36\n", "tree ");
	TEST_OID_FAIL("parent05452d6349abcd67aa396dfb28660d765d8b2a6a\n", "parent ");
	TEST_OID_FAIL("parent 05452d6349abcd67aa396dfb280d765d8b2a6\n", "parent ");
	TEST_OID_FAIL("tree  05452d6349abcd67aa396dfb28660d765d8b2a36\n", "tree ");
	TEST_OID_FAIL("parent 0545xd6349abcd67aa396dfb28660d765d8b2a36\n", "parent ");
	TEST_OID_FAIL("parent 0545xd6349abcd67aa396dfb28660d765d8b2a36FF\n", "parent ");
	TEST_OID_FAIL("", "tree ");
	TEST_OID_FAIL("", "");

#undef TEST_OID_PASS
#undef TEST_OID_FAIL

END_TEST

BEGIN_TEST(parse_person_test)

#define TEST_PERSON_PASS(_string, _header, _name, _email, _time, _offset) { \
	char *ptr = _string; \
	size_t len = strlen(_string);\
	git_person person = {NULL, NULL, 0}; \
	must_pass(git_person__parse(&person, &ptr, ptr + len, _header));\
	must_be_true(strcmp(_name, person.name) == 0);\
	must_be_true(strcmp(_email, person.email) == 0);\
	must_be_true(_time == person.time);\
	must_be_true(_offset == person.timezone_offset);\
	free(person.name); free(person.email);\
}

#define TEST_PERSON_FAIL(_string, _header) { \
	char *ptr = _string; \
	size_t len = strlen(_string);\
	git_person person = {NULL, NULL, 0}; \
	must_fail(git_person__parse(&person, &ptr, ptr + len, _header));\
	free(person.name); free(person.email);\
}

	TEST_PERSON_PASS(
		"author Vicent Marti <tanoku@gmail.com> 12345 \n",
		"author ",
		"Vicent Marti",
		"tanoku@gmail.com",
		12345,
		0);

	TEST_PERSON_PASS(
		"author Vicent Marti <> 12345 \n",
		"author ",
		"Vicent Marti",
		"",
		12345,
		0);

	TEST_PERSON_PASS(
		"author Vicent Marti <tanoku@gmail.com> 231301 +1020\n",
		"author ",
		"Vicent Marti",
		"tanoku@gmail.com",
		231301,
		620);

	TEST_PERSON_PASS(
		"author Vicent Marti with an outrageously long name \
		which will probably overflow the buffer <tanoku@gmail.com> 12345 \n",
		"author ",
		"Vicent Marti with an outrageously long name \
		which will probably overflow the buffer",
		"tanoku@gmail.com",
		12345,
		0);

	TEST_PERSON_PASS(
		"author Vicent Marti <tanokuwithaveryveryverylongemail\
		whichwillprobablyvoverflowtheemailbuffer@gmail.com> 12345 \n",
		"author ",
		"Vicent Marti",
		"tanokuwithaveryveryverylongemail\
		whichwillprobablyvoverflowtheemailbuffer@gmail.com",
		12345,
		0);

	TEST_PERSON_PASS(
		"committer Vicent Marti <tanoku@gmail.com> 123456 +0000 \n",
		"committer ",
		"Vicent Marti",
		"tanoku@gmail.com",
		123456,
		0);

	TEST_PERSON_PASS(
		"committer Vicent Marti <tanoku@gmail.com> 123456 +0100 \n",
		"committer ",
		"Vicent Marti",
		"tanoku@gmail.com",
		123456,
		60);

	TEST_PERSON_PASS(
		"committer Vicent Marti <tanoku@gmail.com> 123456 -0100 \n",
		"committer ",
		"Vicent Marti",
		"tanoku@gmail.com",
		123456,
		-60);

	TEST_PERSON_FAIL(
		"committer Vicent Marti <tanoku@gmail.com> 123456 -1500 \n",
		"committer ");

	TEST_PERSON_FAIL(
		"committer Vicent Marti <tanoku@gmail.com> 123456 +0163 \n",
		"committer ");

	TEST_PERSON_FAIL(
		"author Vicent Marti <tanoku@gmail.com> 12345 \n",
		"author  ");

	TEST_PERSON_FAIL(
		"author Vicent Marti <tanoku@gmail.com> 12345 \n",
		"committer ");

	TEST_PERSON_FAIL(
		"author Vicent Marti 12345 \n",
		"author ");

	TEST_PERSON_FAIL(
		"author Vicent Marti <broken@email 12345 \n",
		"author ");

	TEST_PERSON_FAIL(
		"author Vicent Marti <tanoku@gmail.com> notime \n",
		"author ");

	TEST_PERSON_FAIL(
		"author Vicent Marti <tanoku@gmail.com>\n",
		"author ");

	TEST_PERSON_FAIL(
		"author ",
		"author ");

#undef TEST_PERSON_PASS
#undef TEST_PERSON_FAIL

END_TEST

/* External declaration for testing the buffer parsing method */
int commit_parse_buffer(git_commit *commit, void *data, size_t len, unsigned int parse_flags);

BEGIN_TEST(parse_buffer_test)
	const int broken_commit_count = sizeof(test_commits_broken) / sizeof(*test_commits_broken);
	const int working_commit_count = sizeof(test_commits_working) / sizeof(*test_commits_working);

	int i;
	git_repository *repo;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	for (i = 0; i < broken_commit_count; ++i) {
		git_commit *commit;
		commit = git__malloc(sizeof(git_commit));
		memset(commit, 0x0, sizeof(git_commit));
		commit->object.repo = repo;

		must_fail(commit_parse_buffer(
					commit,
					test_commits_broken[i],
					strlen(test_commits_broken[i]),
					0x1)
				);

		git_commit__free(commit);
	}

	for (i = 0; i < working_commit_count; ++i) {
		git_commit *commit;

		commit = git__malloc(sizeof(git_commit));
		memset(commit, 0x0, sizeof(git_commit));
		commit->object.repo = repo;

		must_pass(commit_parse_buffer(
					commit,
					test_commits_working[i],
					strlen(test_commits_working[i]),
					0x0)
				);

		git_commit__free(commit);

		commit = git__malloc(sizeof(git_commit));
		memset(commit, 0x0, sizeof(git_commit));
		commit->object.repo = repo;

		must_pass(commit_parse_buffer(
					commit,
					test_commits_working[i],
					strlen(test_commits_working[i]),
					0x1)
				);

		git_commit__free(commit);
	}

	git_repository_free(repo);
END_TEST
