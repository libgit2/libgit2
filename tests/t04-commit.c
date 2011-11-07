/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include "test_lib.h"
#include "test_helpers.h"

#include "commit.h"
#include "signature.h"

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

BEGIN_TEST(parse0, "parse the OID line in a commit")

	git_oid oid;

#define TEST_OID_PASS(string, header) { \
	const char *ptr = string;\
	const char *ptr_original = ptr;\
	size_t len = strlen(ptr);\
	must_pass(git_oid__parse(&oid, &ptr, ptr + len, header));\
	must_be_true(ptr == ptr_original + len);\
}

#define TEST_OID_FAIL(string, header) { \
	const char *ptr = string;\
	size_t len = strlen(ptr);\
	must_fail(git_oid__parse(&oid, &ptr, ptr + len, header));\
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

BEGIN_TEST(parse1, "parse the signature line in a commit")

#define TEST_SIGNATURE_PASS(_string, _header, _name, _email, _time, _offset) { \
	const char *ptr = _string; \
	size_t len = strlen(_string);\
	git_signature person = {NULL, NULL, {0, 0}}; \
	must_pass(git_signature__parse(&person, &ptr, ptr + len, _header, '\n'));\
	must_be_true(strcmp(_name, person.name) == 0);\
	must_be_true(strcmp(_email, person.email) == 0);\
	must_be_true(_time == person.when.time);\
	must_be_true(_offset == person.when.offset);\
	git__free(person.name); git__free(person.email);\
}

#define TEST_SIGNATURE_FAIL(_string, _header) { \
	const char *ptr = _string; \
	size_t len = strlen(_string);\
	git_signature person = {NULL, NULL, {0, 0}}; \
	must_fail(git_signature__parse(&person, &ptr, ptr + len, _header, '\n'));\
	git__free(person.name); git__free(person.email);\
}

	TEST_SIGNATURE_PASS(
		"author Vicent Marti <tanoku@gmail.com> 12345 \n",
		"author ",
		"Vicent Marti",
		"tanoku@gmail.com",
		12345,
		0);

	TEST_SIGNATURE_PASS(
		"author Vicent Marti <> 12345 \n",
		"author ",
		"Vicent Marti",
		"",
		12345,
		0);

	TEST_SIGNATURE_PASS(
		"author Vicent Marti <tanoku@gmail.com> 231301 +1020\n",
		"author ",
		"Vicent Marti",
		"tanoku@gmail.com",
		231301,
		620);

	TEST_SIGNATURE_PASS(
		"author Vicent Marti with an outrageously long name \
		which will probably overflow the buffer <tanoku@gmail.com> 12345 \n",
		"author ",
		"Vicent Marti with an outrageously long name \
		which will probably overflow the buffer",
		"tanoku@gmail.com",
		12345,
		0);

	TEST_SIGNATURE_PASS(
		"author Vicent Marti <tanokuwithaveryveryverylongemail\
		whichwillprobablyvoverflowtheemailbuffer@gmail.com> 12345 \n",
		"author ",
		"Vicent Marti",
		"tanokuwithaveryveryverylongemail\
		whichwillprobablyvoverflowtheemailbuffer@gmail.com",
		12345,
		0);

	TEST_SIGNATURE_PASS(
		"committer Vicent Marti <tanoku@gmail.com> 123456 +0000 \n",
		"committer ",
		"Vicent Marti",
		"tanoku@gmail.com",
		123456,
		0);

	TEST_SIGNATURE_PASS(
		"committer Vicent Marti <tanoku@gmail.com> 123456 +0100 \n",
		"committer ",
		"Vicent Marti",
		"tanoku@gmail.com",
		123456,
		60);

	TEST_SIGNATURE_PASS(
		"committer Vicent Marti <tanoku@gmail.com> 123456 -0100 \n",
		"committer ",
		"Vicent Marti",
		"tanoku@gmail.com",
		123456,
		-60);

	/* Parse a signature without an author field */
	TEST_SIGNATURE_PASS(
		"committer <tanoku@gmail.com> 123456 -0100 \n",
		"committer ",
		"",
		"tanoku@gmail.com",
		123456,
		-60);

	/* Parse a signature without an author field */
	TEST_SIGNATURE_PASS(
		"committer  <tanoku@gmail.com> 123456 -0100 \n",
		"committer ",
		"",
		"tanoku@gmail.com",
		123456,
		-60);

	/* Parse a signature with an empty author field */
	TEST_SIGNATURE_PASS(
		"committer   <tanoku@gmail.com> 123456 -0100 \n",
		"committer ",
		"",
		"tanoku@gmail.com",
		123456,
		-60);

	/* Parse a signature with an empty email field */
	TEST_SIGNATURE_PASS(
		"committer Vicent Marti <> 123456 -0100 \n",
		"committer ",
		"Vicent Marti",
		"",
		123456,
		-60);

	/* Parse a signature with an empty email field */
	TEST_SIGNATURE_PASS(
		"committer Vicent Marti < > 123456 -0100 \n",
		"committer ",
		"Vicent Marti",
		"",
		123456,
		-60);

	/* Parse a signature with empty name and email */
	TEST_SIGNATURE_PASS(
		"committer <> 123456 -0100 \n",
		"committer ",
		"",
		"",
		123456,
		-60);

	/* Parse a signature with empty name and email */
	TEST_SIGNATURE_PASS(
		"committer  <> 123456 -0100 \n",
		"committer ",
		"",
		"",
		123456,
		-60);

	/* Parse a signature with empty name and email */
	TEST_SIGNATURE_PASS(
		"committer  < > 123456 -0100 \n",
		"committer ",
		"",
		"",
		123456,
		-60);

	/* Parse an obviously invalid signature */
	TEST_SIGNATURE_PASS(
		"committer foo<@bar> 123456 -0100 \n",
		"committer ",
		"foo",
		"@bar",
		123456,
		-60);

	/* Parse an obviously invalid signature */
	TEST_SIGNATURE_PASS(
		"committer    foo<@bar>123456 -0100 \n",
		"committer ",
		"foo",
		"@bar",
		123456,
		-60);

	/* Parse an obviously invalid signature */
	TEST_SIGNATURE_PASS(
		"committer <>\n",
		"committer ",
		"",
		"",
		0,
		0);

	TEST_SIGNATURE_PASS(
		"committer Vicent Marti <tanoku@gmail.com> 123456 -1500 \n",
		"committer ",
		"Vicent Marti",
		"tanoku@gmail.com",
		0,
		0);

	TEST_SIGNATURE_PASS(
		"committer Vicent Marti <tanoku@gmail.com> 123456 +0163 \n",
		"committer ",
		"Vicent Marti",
		"tanoku@gmail.com",
		0,
		0);

	TEST_SIGNATURE_PASS(
		"author Vicent Marti <tanoku@gmail.com> notime \n",
		"author ",
		"Vicent Marti",
		"tanoku@gmail.com",
		0,
		0);

	TEST_SIGNATURE_PASS(
		"author Vicent Marti <tanoku@gmail.com> 123456 notimezone \n",
		"author ",
		"Vicent Marti",
		"tanoku@gmail.com",
		0,
		0);

	TEST_SIGNATURE_PASS(
		"author Vicent Marti <tanoku@gmail.com> notime +0100\n",
		"author ",
		"Vicent Marti",
		"tanoku@gmail.com",
		0,
		0);

	TEST_SIGNATURE_PASS(
		"author Vicent Marti <tanoku@gmail.com>\n",
		"author ",
		"Vicent Marti",
		"tanoku@gmail.com",
		0,
		0);

	TEST_SIGNATURE_PASS(
		"author A U Thor <author@example.com>,  C O. Miter <comiter@example.com> 1234567890 -0700\n",
		"author ",
		"A U Thor",
		"author@example.com",
		1234567890,
		-420);

	TEST_SIGNATURE_PASS(
		"author A U Thor <author@example.com> and others 1234567890 -0700\n",
		"author ",
		"A U Thor",
		"author@example.com",
		1234567890,
		-420);

	TEST_SIGNATURE_PASS(
		"author A U Thor <author@example.com> and others 1234567890\n",
		"author ",
		"A U Thor",
		"author@example.com",
		1234567890,
		0);

	TEST_SIGNATURE_PASS(
		"author A U Thor> <author@example.com> and others 1234567890\n",
		"author ",
		"A U Thor>",
		"author@example.com",
		1234567890,
		0);

	TEST_SIGNATURE_FAIL(
		"committer Vicent Marti tanoku@gmail.com> 123456 -0100 \n",
		"committer ");

	TEST_SIGNATURE_FAIL(
		"author Vicent Marti <tanoku@gmail.com> 12345 \n",
		"author  ");

	TEST_SIGNATURE_FAIL(
		"author Vicent Marti <tanoku@gmail.com> 12345 \n",
		"committer ");

	TEST_SIGNATURE_FAIL(
		"author Vicent Marti 12345 \n",
		"author ");

	TEST_SIGNATURE_FAIL(
		"author Vicent Marti <broken@email 12345 \n",
		"author ");

	TEST_SIGNATURE_FAIL(
		"committer Vicent Marti ><\n",
		"committer ");

	TEST_SIGNATURE_FAIL(
		"author ",
		"author ");

#undef TEST_SIGNATURE_PASS
#undef TEST_SIGNATURE_FAIL

END_TEST

static int try_build_signature(const char *name, const char *email, git_time_t time, int offset)
{
	git_signature *sign;
	int error = GIT_SUCCESS;

	if ((error =  git_signature_new(&sign, name, email, time, offset)) < GIT_SUCCESS)
		return error;

	git_signature_free((git_signature *)sign);

	return error;
}

BEGIN_TEST(signature0, "creating a signature trims leading and trailing spaces")
	git_signature *sign;
	must_pass(git_signature_new(&sign, "  nulltoken ", "   emeric.fermas@gmail.com     ", 1234567890, 60));
	must_be_true(strcmp(sign->name, "nulltoken") == 0);
	must_be_true(strcmp(sign->email, "emeric.fermas@gmail.com") == 0);
	git_signature_free((git_signature *)sign);
END_TEST

BEGIN_TEST(signature1, "can not create a signature with empty name or email")
	must_pass(try_build_signature("nulltoken", "emeric.fermas@gmail.com", 1234567890, 60));

	must_fail(try_build_signature("", "emeric.fermas@gmail.com", 1234567890, 60));
	must_fail(try_build_signature("   ", "emeric.fermas@gmail.com", 1234567890, 60));
	must_fail(try_build_signature("nulltoken", "", 1234567890, 60));
	must_fail(try_build_signature("nulltoken", "  ", 1234567890, 60));
END_TEST

BEGIN_TEST(signature2, "creating a one character signature")
	git_signature *sign;
	must_pass(git_signature_new(&sign, "x", "foo@bar.baz", 1234567890, 60));
	must_be_true(strcmp(sign->name, "x") == 0);
	must_be_true(strcmp(sign->email, "foo@bar.baz") == 0);
	git_signature_free((git_signature *)sign);
END_TEST

BEGIN_TEST(signature3, "creating a two character signature")
	git_signature *sign;
	must_pass(git_signature_new(&sign, "xx", "x@y.z", 1234567890, 60));
	must_be_true(strcmp(sign->name, "xx") == 0);
	must_be_true(strcmp(sign->email, "x@y.z") == 0);
	git_signature_free((git_signature *)sign);
END_TEST

BEGIN_TEST(signature4, "creating a zero character signature")
	git_signature *sign;
	must_fail(git_signature_new(&sign, "", "x@y.z", 1234567890, 60));
	must_be_true(sign == NULL);
END_TEST


BEGIN_TEST(parse2, "parse a whole commit buffer")
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

		must_fail(git_commit__parse_buffer(
					commit,
					test_commits_broken[i],
					strlen(test_commits_broken[i]))
				);

		git_commit__free(commit);
	}

	for (i = 0; i < working_commit_count; ++i) {
		git_commit *commit;

		commit = git__malloc(sizeof(git_commit));
		memset(commit, 0x0, sizeof(git_commit));
		commit->object.repo = repo;

		must_pass(git_commit__parse_buffer(
					commit,
					test_commits_working[i],
					strlen(test_commits_working[i]))
				);

		git_commit__free(commit);

		commit = git__malloc(sizeof(git_commit));
		memset(commit, 0x0, sizeof(git_commit));
		commit->object.repo = repo;

		must_pass(git_commit__parse_buffer(
					commit,
					test_commits_working[i],
					strlen(test_commits_working[i]))
				);

		git_commit__free(commit);
	}

	git_repository_free(repo);
END_TEST

static const char *commit_ids[] = {
	"a4a7dce85cf63874e984719f4fdd239f5145052f", /* 0 */
	"9fd738e8f7967c078dceed8190330fc8648ee56a", /* 1 */
	"4a202b346bb0fb0db7eff3cffeb3c70babbd2045", /* 2 */
	"c47800c7266a2be04c571c04d5a6614691ea99bd", /* 3 */
	"8496071c1b46c854b31185ea97743be6a8774479", /* 4 */
	"5b5b025afb0b4c913b4c338a42934a3863bf3644", /* 5 */
	"a65fedf39aefe402d3bb6e24df4d4f5fe4547750", /* 6 */
};

BEGIN_TEST(details0, "query the details on a parsed commit")
	const size_t commit_count = sizeof(commit_ids) / sizeof(const char *);

	unsigned int i;
	git_repository *repo;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	for (i = 0; i < commit_count; ++i) {
		git_oid id;
		git_commit *commit;

		const git_signature *author, *committer;
		const char *message;
		git_time_t commit_time;
		unsigned int parents, p;
		git_commit *parent = NULL, *old_parent = NULL;

		git_oid_fromstr(&id, commit_ids[i]);

		must_pass(git_commit_lookup(&commit, repo, &id));

		message = git_commit_message(commit);
		author = git_commit_author(commit);
		committer = git_commit_committer(commit);
		commit_time = git_commit_time(commit);
		parents = git_commit_parentcount(commit);

		must_be_true(strcmp(author->name, "Scott Chacon") == 0);
		must_be_true(strcmp(author->email, "schacon@gmail.com") == 0);
		must_be_true(strcmp(committer->name, "Scott Chacon") == 0);
		must_be_true(strcmp(committer->email, "schacon@gmail.com") == 0);
		must_be_true(message != NULL);
		must_be_true(strchr(message, '\n') != NULL);
		must_be_true(commit_time > 0);
		must_be_true(parents <= 2);
		for (p = 0;p < parents;p++) {
			if (old_parent != NULL)
				git_commit_close(old_parent);

			old_parent = parent;
			must_pass(git_commit_parent(&parent, commit, p));
			must_be_true(parent != NULL);
			must_be_true(git_commit_author(parent) != NULL); // is it really a commit?
		}
		git_commit_close(old_parent);
		git_commit_close(parent);

		must_fail(git_commit_parent(&parent, commit, parents));
		git_commit_close(commit);
	}

	git_repository_free(repo);
END_TEST

#define COMMITTER_NAME "Vicent Marti"
#define COMMITTER_EMAIL "vicent@github.com"
#define COMMIT_MESSAGE "This commit has been created in memory\n\
This is a commit created in memory and it will be written back to disk\n"

static const char *tree_oid = "1810dff58d8a660512d4832e740f692884338ccd";

BEGIN_TEST(write0, "write a new commit object from memory to disk")
	git_repository *repo;
	git_commit *commit;
	git_oid tree_id, parent_id, commit_id;
	git_signature *author, *committer;
	const git_signature *author1, *committer1;
	git_commit *parent;
	git_tree *tree;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	git_oid_fromstr(&tree_id, tree_oid);
	must_pass(git_tree_lookup(&tree, repo, &tree_id));

	git_oid_fromstr(&parent_id, commit_ids[4]);
	must_pass(git_commit_lookup(&parent, repo, &parent_id));

	/* create signatures */
	must_pass(git_signature_new(&committer, COMMITTER_NAME, COMMITTER_EMAIL, 123456789, 60));
	must_pass(git_signature_new(&author, COMMITTER_NAME, COMMITTER_EMAIL, 987654321, 90));

	must_pass(git_commit_create_v(
		&commit_id, /* out id */
		repo,
		NULL, /* do not update the HEAD */
		author,
		committer,
		NULL,
		COMMIT_MESSAGE,
		tree,
		1, parent));

	git_object_close((git_object *)parent);
	git_object_close((git_object *)tree);

	git_signature_free(committer);
	git_signature_free(author);

	must_pass(git_commit_lookup(&commit, repo, &commit_id));

	/* Check attributes were set correctly */
	author1 = git_commit_author(commit);
	must_be_true(author1 != NULL);
	must_be_true(strcmp(author1->name, COMMITTER_NAME) == 0);
	must_be_true(strcmp(author1->email, COMMITTER_EMAIL) == 0);
	must_be_true(author1->when.time == 987654321);
	must_be_true(author1->when.offset == 90);

	committer1 = git_commit_committer(commit);
	must_be_true(committer1 != NULL);
	must_be_true(strcmp(committer1->name, COMMITTER_NAME) == 0);
	must_be_true(strcmp(committer1->email, COMMITTER_EMAIL) == 0);
	must_be_true(committer1->when.time == 123456789);
	must_be_true(committer1->when.offset == 60);

	must_be_true(strcmp(git_commit_message(commit), COMMIT_MESSAGE) == 0);

#ifndef GIT_WIN32
	must_be_true((loose_object_mode(REPOSITORY_FOLDER, (git_object *)commit) & 0777) == GIT_OBJECT_FILE_MODE);
#endif

	must_pass(remove_loose_object(REPOSITORY_FOLDER, (git_object *)commit));

	git_commit_close(commit);
	git_repository_free(repo);
END_TEST

#define ROOT_COMMIT_MESSAGE "This is a root commit\n\
This is a root commit and should be the only one in this branch\n"

BEGIN_TEST(root0, "create a root commit")
	git_repository *repo;
	git_commit *commit;
	git_oid tree_id, commit_id;
	const git_oid *branch_oid;
	git_signature *author, *committer;
	const char *branch_name = "refs/heads/root-commit-branch";
	git_reference *head, *branch;
	char *head_old;
	git_tree *tree;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	git_oid_fromstr(&tree_id, tree_oid);
	must_pass(git_tree_lookup(&tree, repo, &tree_id));

	/* create signatures */
	must_pass(git_signature_new(&committer, COMMITTER_NAME, COMMITTER_EMAIL, 123456789, 60));
	must_pass(git_signature_new(&author, COMMITTER_NAME, COMMITTER_EMAIL, 987654321, 90));

	/* First we need to update HEAD so it points to our non-existant branch */
	must_pass(git_reference_lookup(&head, repo, "HEAD"));
	must_be_true(git_reference_type(head) == GIT_REF_SYMBOLIC);
	head_old = git__strdup(git_reference_target(head));
	must_be_true(head_old != NULL);

	must_pass(git_reference_set_target(head, branch_name));

	must_pass(git_commit_create_v(
		&commit_id, /* out id */
		repo,
		"HEAD",
		author,
		committer,
		NULL,
		ROOT_COMMIT_MESSAGE,
		tree,
		0));

	git_object_close((git_object *)tree);
	git_signature_free(committer);
	git_signature_free(author);

	/*
	 * The fact that creating a commit works has already been
	 * tested. Here we just make sure it's our commit and that it was
	 * written as a root commit.
	 */
	must_pass(git_commit_lookup(&commit, repo, &commit_id));
	must_be_true(git_commit_parentcount(commit) == 0);
	must_pass(git_reference_lookup(&branch, repo, branch_name));
	branch_oid = git_reference_oid(branch);
	must_pass(git_oid_cmp(branch_oid, &commit_id));
	must_be_true(!strcmp(git_commit_message(commit), ROOT_COMMIT_MESSAGE));

	/* Remove the data we just added to the repo */
	must_pass(git_reference_lookup(&head, repo, "HEAD"));
	must_pass(git_reference_set_target(head, head_old));
	must_pass(git_reference_delete(branch));
	must_pass(remove_loose_object(REPOSITORY_FOLDER, (git_object *)commit));
	git__free(head_old);
	git_commit_close(commit);
	git_repository_free(repo);

	git_reference_free(head);
END_TEST

BEGIN_SUITE(commit)
	ADD_TEST(parse0);
	ADD_TEST(parse1);
	ADD_TEST(parse2);
	ADD_TEST(details0);

	ADD_TEST(write0);

	ADD_TEST(root0);

	ADD_TEST(signature0);
	ADD_TEST(signature1);
	ADD_TEST(signature2);
	ADD_TEST(signature3);
	ADD_TEST(signature4);
END_SUITE
