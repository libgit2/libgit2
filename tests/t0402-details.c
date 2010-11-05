#include "test_lib.h"
#include "test_helpers.h"
#include "commit.h"
#include "person.h"

#include <git/odb.h>
#include <git/commit.h>
#include <git/revwalk.h>

static const char *commit_ids[] = {
	"a4a7dce85cf63874e984719f4fdd239f5145052f", /* 0 */
	"9fd738e8f7967c078dceed8190330fc8648ee56a", /* 1 */
	"4a202b346bb0fb0db7eff3cffeb3c70babbd2045", /* 2 */
	"c47800c7266a2be04c571c04d5a6614691ea99bd", /* 3 */
	"8496071c1b46c854b31185ea97743be6a8774479", /* 4 */
	"5b5b025afb0b4c913b4c338a42934a3863bf3644", /* 5 */
};

BEGIN_TEST(query_details_test)
	const size_t commit_count = sizeof(commit_ids) / sizeof(const char *);

	unsigned int i;
	git_repository *repo;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));
	
	for (i = 0; i < commit_count; ++i) {
		git_oid id;
		git_commit *commit;

		const git_person *author, *committer;
		const char *message, *message_short;
		time_t commit_time;

		git_oid_mkstr(&id, commit_ids[i]);

		must_pass(git_commit_lookup(&commit, repo, &id));

		message = git_commit_message(commit);
		message_short = git_commit_message_short(commit);
		author = git_commit_author(commit);
		committer = git_commit_committer(commit);
		commit_time = git_commit_time(commit);

		must_be_true(strcmp(author->name, "Scott Chacon") == 0);
		must_be_true(strcmp(author->email, "schacon@gmail.com") == 0);
		must_be_true(strcmp(committer->name, "Scott Chacon") == 0);
		must_be_true(strcmp(committer->email, "schacon@gmail.com") == 0);
		must_be_true(strchr(message, '\n') != NULL);
		must_be_true(strchr(message_short, '\n') == NULL);
		must_be_true(commit_time > 0);
	}

	git_repository_free(repo);
END_TEST
