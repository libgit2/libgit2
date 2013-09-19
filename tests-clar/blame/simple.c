#include "blame_helpers.h"

/*
 * $ git blame -s branch_file.txt
 *    orig line no                        final line no
 * commit   V  author       timestamp                 V
 * c47800c7 1 (Scott Chacon 2010-05-25 11:58:14 -0700 1
 * a65fedf3 2 (Scott Chacon 2011-08-09 19:33:46 -0700 2
 */
void test_blame_simple__trivial_testrepo(void)
{
	git_blame *blame = NULL;
	git_repository *repo;
	cl_git_pass(git_repository_open(&repo, cl_fixture("testrepo/.gitted")));
	cl_git_pass(git_blame_file(&blame, repo, "branch_file.txt", NULL));

	cl_assert_equal_i(2, git_blame_get_hunk_count(blame));
	check_blame_hunk_index(repo, blame, 0, 1, 1, "c47800c7", "branch_file.txt");
	check_blame_hunk_index(repo, blame, 1, 2, 1, "a65fedf3", "branch_file.txt");

	git_blame_free(blame);
	git_repository_free(repo);
}

/*
 * $ git blame -n b.txt
 *    orig line no                          final line no
 * commit    V  author     timestamp                  V
 * da237394  1 (Ben Straub 2013-02-12 15:11:30 -0800  1
 * da237394  2 (Ben Straub 2013-02-12 15:11:30 -0800  2
 * da237394  3 (Ben Straub 2013-02-12 15:11:30 -0800  3
 * da237394  4 (Ben Straub 2013-02-12 15:11:30 -0800  4
 * ^b99f7ac  1 (Ben Straub 2013-02-12 15:10:12 -0800  5
 * 63d671eb  6 (Ben Straub 2013-02-12 15:13:04 -0800  6
 * 63d671eb  7 (Ben Straub 2013-02-12 15:13:04 -0800  7
 * 63d671eb  8 (Ben Straub 2013-02-12 15:13:04 -0800  8
 * 63d671eb  9 (Ben Straub 2013-02-12 15:13:04 -0800  9
 * 63d671eb 10 (Ben Straub 2013-02-12 15:13:04 -0800 10
 * aa06ecca  6 (Ben Straub 2013-02-12 15:14:46 -0800 11
 * aa06ecca  7 (Ben Straub 2013-02-12 15:14:46 -0800 12
 * aa06ecca  8 (Ben Straub 2013-02-12 15:14:46 -0800 13
 * aa06ecca  9 (Ben Straub 2013-02-12 15:14:46 -0800 14
 * aa06ecca 10 (Ben Straub 2013-02-12 15:14:46 -0800 15
 */
void test_blame_simple__trivial_blamerepo(void)
{
	git_blame *blame = NULL;
	git_repository *repo;
	cl_git_pass(git_repository_open(&repo, cl_fixture("blametest.git")));
	cl_git_pass(git_blame_file(&blame, repo, "b.txt", NULL));

	cl_assert_equal_i(4, git_blame_get_hunk_count(blame));
	check_blame_hunk_index(repo, blame, 0,  1, 4, "da237394", "b.txt");
	check_blame_hunk_index(repo, blame, 1,  5, 1, "b99f7ac0", "b.txt");
	check_blame_hunk_index(repo, blame, 2,  6, 5, "63d671eb", "b.txt");
	check_blame_hunk_index(repo, blame, 3, 11, 5, "aa06ecca", "b.txt");

	git_blame_free(blame);
	git_repository_free(repo);
}


/*
 * $ git blame -n 359fc2d -- include/git2.h
 *                     orig line no                                final line no
 * commit   orig path       V  author              timestamp                  V
 * d12299fe src/git.h       1 (Vicent Martí        2010-12-03 22:22:10 +0200  1
 * 359fc2d2 include/git2.h  2 (Edward Thomson      2013-01-08 17:07:25 -0600  2
 * d12299fe src/git.h       5 (Vicent Martí        2010-12-03 22:22:10 +0200  3
 * bb742ede include/git2.h  4 (Vicent Martí        2011-09-19 01:54:32 +0300  4
 * bb742ede include/git2.h  5 (Vicent Martí        2011-09-19 01:54:32 +0300  5
 * d12299fe src/git.h      24 (Vicent Martí        2010-12-03 22:22:10 +0200  6
 * d12299fe src/git.h      25 (Vicent Martí        2010-12-03 22:22:10 +0200  7
 * d12299fe src/git.h      26 (Vicent Martí        2010-12-03 22:22:10 +0200  8
 * d12299fe src/git.h      27 (Vicent Martí        2010-12-03 22:22:10 +0200  9
 * d12299fe src/git.h      28 (Vicent Martí        2010-12-03 22:22:10 +0200 10
 * 96fab093 include/git2.h 11 (Sven Strickroth     2011-10-09 18:37:41 +0200 11
 * 9d1dcca2 src/git2.h     33 (Vicent Martí        2011-02-07 10:35:58 +0200 12
 * 44908fe7 src/git2.h     29 (Vicent Martí        2010-12-06 23:03:16 +0200 13
 * a15c550d include/git2.h 14 (Vicent Martí        2011-11-16 14:09:44 +0100 14
 * 44908fe7 src/git2.h     30 (Vicent Martí        2010-12-06 23:03:16 +0200 15
 * d12299fe src/git.h      32 (Vicent Martí        2010-12-03 22:22:10 +0200 16
 * 44908fe7 src/git2.h     33 (Vicent Martí        2010-12-06 23:03:16 +0200 17
 * d12299fe src/git.h      34 (Vicent Martí        2010-12-03 22:22:10 +0200 18
 * 44908fe7 src/git2.h     35 (Vicent Martí        2010-12-06 23:03:16 +0200 19
 * 638c2ca4 src/git2.h     36 (Vicent Martí        2010-12-18 02:10:25 +0200 20
 * 44908fe7 src/git2.h     36 (Vicent Martí        2010-12-06 23:03:16 +0200 21
 * d12299fe src/git.h      37 (Vicent Martí        2010-12-03 22:22:10 +0200 22
 * 44908fe7 src/git2.h     38 (Vicent Martí        2010-12-06 23:03:16 +0200 23
 * 44908fe7 src/git2.h     39 (Vicent Martí        2010-12-06 23:03:16 +0200 24
 * bf787bd8 include/git2.h 25 (Carlos Martín Nieto 2012-04-08 18:56:50 +0200 25
 * 0984c876 include/git2.h 26 (Scott J. Goldman    2012-11-28 18:27:43 -0800 26
 * 2f8a8ab2 src/git2.h     41 (Vicent Martí        2011-01-29 01:56:25 +0200 27
 * 27df4275 include/git2.h 47 (Michael Schubert    2011-06-28 14:13:12 +0200 28
 * a346992f include/git2.h 28 (Ben Straub          2012-05-10 09:47:14 -0700 29
 * d12299fe src/git.h      40 (Vicent Martí        2010-12-03 22:22:10 +0200 30
 * 44908fe7 src/git2.h     41 (Vicent Martí        2010-12-06 23:03:16 +0200 31
 * 44908fe7 src/git2.h     42 (Vicent Martí        2010-12-06 23:03:16 +0200 32
 * 44908fe7 src/git2.h     43 (Vicent Martí        2010-12-06 23:03:16 +0200 33
 * 44908fe7 src/git2.h     44 (Vicent Martí        2010-12-06 23:03:16 +0200 34
 * 44908fe7 src/git2.h     45 (Vicent Martí        2010-12-06 23:03:16 +0200 35
 * 65b09b1d include/git2.h 33 (Russell Belfer      2012-02-02 18:03:43 -0800 36
 * d12299fe src/git.h      46 (Vicent Martí        2010-12-03 22:22:10 +0200 37
 * 44908fe7 src/git2.h     47 (Vicent Martí        2010-12-06 23:03:16 +0200 38
 * 5d4cd003 include/git2.h 55 (Carlos Martín Nieto 2011-03-28 17:02:45 +0200 39
 * 41fb1ca0 include/git2.h 39 (Philip Kelley       2012-10-29 13:41:14 -0400 40
 * 2dc31040 include/git2.h 56 (Carlos Martín Nieto 2011-06-20 18:58:57 +0200 41
 * 764df57e include/git2.h 40 (Ben Straub          2012-06-15 13:14:43 -0700 42
 * 5280f4e6 include/git2.h 41 (Ben Straub          2012-07-31 19:39:06 -0700 43
 * 613d5eb9 include/git2.h 43 (Philip Kelley       2012-11-28 11:42:37 -0500 44
 * d12299fe src/git.h      48 (Vicent Martí        2010-12-03 22:22:10 +0200 45
 * 111ee3fe include/git2.h 41 (Vicent Martí        2012-07-11 14:37:26 +0200 46
 * f004c4a8 include/git2.h 44 (Russell Belfer      2012-08-21 17:26:39 -0700 47
 * 111ee3fe include/git2.h 42 (Vicent Martí        2012-07-11 14:37:26 +0200 48
 * 9c82357b include/git2.h 58 (Carlos Martín Nieto 2011-06-17 18:13:14 +0200 49
 * d6258deb include/git2.h 61 (Carlos Martín Nieto 2011-06-25 15:10:09 +0200 50
 * b311e313 include/git2.h 63 (Julien Miotte       2011-07-27 18:31:13 +0200 51
 * 3412391d include/git2.h 63 (Carlos Martín Nieto 2011-07-07 11:47:31 +0200 52
 * bfc9ca59 include/git2.h 43 (Russell Belfer      2012-03-28 16:45:36 -0700 53
 * bf477ed4 include/git2.h 44 (Michael Schubert    2012-02-15 00:33:38 +0100 54
 * edebceff include/git2.h 46 (nulltoken           2012-05-01 13:57:45 +0200 55
 * 743a4b3b include/git2.h 48 (nulltoken           2012-06-15 22:24:59 +0200 56
 * 0a32dca5 include/git2.h 54 (Michael Schubert    2012-08-19 22:26:32 +0200 57
 * 590fb68b include/git2.h 55 (nulltoken           2012-10-04 13:47:45 +0200 58
 * bf477ed4 include/git2.h 45 (Michael Schubert    2012-02-15 00:33:38 +0100 59
 * d12299fe src/git.h      49 (Vicent Martí        2010-12-03 22:22:10 +0200 60
 */
void test_blame_simple__trivial_libgit2(void)
{
	git_repository *repo;
	git_blame *blame;
	git_blame_options opts = GIT_BLAME_OPTIONS_INIT;
	git_object *obj;

	cl_git_pass(git_repository_open(&repo, cl_fixture("../..")));

	/* This test can't work on a shallow clone */
	if (git_repository_is_shallow(repo)) {
		git_repository_free(repo);
		return;
	}

	cl_git_pass(git_revparse_single(&obj, repo, "359fc2d"));
	git_oid_cpy(&opts.newest_commit, git_object_id(obj));
	git_object_free(obj);

	cl_git_pass(git_blame_file(&blame, repo, "include/git2.h", &opts));

	check_blame_hunk_index(repo, blame,  0,  1, 1, "d12299fe", "src/git.h");
	check_blame_hunk_index(repo, blame,  1,  2, 1, "359fc2d2", "include/git2.h");
	check_blame_hunk_index(repo, blame,  2,  3, 1, "d12299fe", "src/git.h");
	check_blame_hunk_index(repo, blame,  3,  4, 2, "bb742ede", "include/git2.h");
	check_blame_hunk_index(repo, blame,  4,  6, 5, "d12299fe", "src/git.h");
	check_blame_hunk_index(repo, blame,  5, 11, 1, "96fab093", "include/git2.h");
	check_blame_hunk_index(repo, blame,  6, 12, 1, "9d1dcca2", "src/git2.h");
	check_blame_hunk_index(repo, blame,  7, 13, 1, "44908fe7", "src/git2.h");
	check_blame_hunk_index(repo, blame,  8, 14, 1, "a15c550d", "include/git2.h");
	check_blame_hunk_index(repo, blame,  9, 15, 1, "44908fe7", "src/git2.h");
	check_blame_hunk_index(repo, blame, 10, 16, 1, "d12299fe", "src/git.h");
	check_blame_hunk_index(repo, blame, 11, 17, 1, "44908fe7", "src/git2.h");
	check_blame_hunk_index(repo, blame, 12, 18, 1, "d12299fe", "src/git.h");
	check_blame_hunk_index(repo, blame, 13, 19, 1, "44908fe7", "src/git2.h");
	check_blame_hunk_index(repo, blame, 14, 20, 1, "638c2ca4", "src/git2.h");
	check_blame_hunk_index(repo, blame, 15, 21, 1, "44908fe7", "src/git2.h");
	check_blame_hunk_index(repo, blame, 16, 22, 1, "d12299fe", "src/git.h");
	check_blame_hunk_index(repo, blame, 17, 23, 2, "44908fe7", "src/git2.h");
	check_blame_hunk_index(repo, blame, 18, 25, 1, "bf787bd8", "include/git2.h");
	check_blame_hunk_index(repo, blame, 19, 26, 1, "0984c876", "include/git2.h");
	check_blame_hunk_index(repo, blame, 20, 27, 1, "2f8a8ab2", "src/git2.h");
	check_blame_hunk_index(repo, blame, 21, 28, 1, "27df4275", "include/git2.h");
	check_blame_hunk_index(repo, blame, 22, 29, 1, "a346992f", "include/git2.h");
	check_blame_hunk_index(repo, blame, 23, 30, 1, "d12299fe", "src/git.h");
	check_blame_hunk_index(repo, blame, 24, 31, 5, "44908fe7", "src/git2.h");
	check_blame_hunk_index(repo, blame, 25, 36, 1, "65b09b1d", "include/git2.h");
	check_blame_hunk_index(repo, blame, 26, 37, 1, "d12299fe", "src/git.h");
	check_blame_hunk_index(repo, blame, 27, 38, 1, "44908fe7", "src/git2.h");
	check_blame_hunk_index(repo, blame, 28, 39, 1, "5d4cd003", "include/git2.h");
	check_blame_hunk_index(repo, blame, 29, 40, 1, "41fb1ca0", "include/git2.h");
	check_blame_hunk_index(repo, blame, 30, 41, 1, "2dc31040", "include/git2.h");
	check_blame_hunk_index(repo, blame, 31, 42, 1, "764df57e", "include/git2.h");
	check_blame_hunk_index(repo, blame, 32, 43, 1, "5280f4e6", "include/git2.h");
	check_blame_hunk_index(repo, blame, 33, 44, 1, "613d5eb9", "include/git2.h");
	check_blame_hunk_index(repo, blame, 34, 45, 1, "d12299fe", "src/git.h");
	check_blame_hunk_index(repo, blame, 35, 46, 1, "111ee3fe", "include/git2.h");
	check_blame_hunk_index(repo, blame, 36, 47, 1, "f004c4a8", "include/git2.h");
	check_blame_hunk_index(repo, blame, 37, 48, 1, "111ee3fe", "include/git2.h");
	check_blame_hunk_index(repo, blame, 38, 49, 1, "9c82357b", "include/git2.h");
	check_blame_hunk_index(repo, blame, 39, 50, 1, "d6258deb", "include/git2.h");
	check_blame_hunk_index(repo, blame, 40, 51, 1, "b311e313", "include/git2.h");
	check_blame_hunk_index(repo, blame, 41, 52, 1, "3412391d", "include/git2.h");
	check_blame_hunk_index(repo, blame, 42, 53, 1, "bfc9ca59", "include/git2.h");
	check_blame_hunk_index(repo, blame, 43, 54, 1, "bf477ed4", "include/git2.h");
	check_blame_hunk_index(repo, blame, 44, 55, 1, "edebceff", "include/git2.h");
	check_blame_hunk_index(repo, blame, 45, 56, 1, "743a4b3b", "include/git2.h");
	check_blame_hunk_index(repo, blame, 46, 57, 1, "0a32dca5", "include/git2.h");
	check_blame_hunk_index(repo, blame, 47, 58, 1, "590fb68b", "include/git2.h");
	check_blame_hunk_index(repo, blame, 48, 59, 1, "bf477ed4", "include/git2.h");
	check_blame_hunk_index(repo, blame, 49, 60, 1, "d12299fe", "src/git.h");

	git_blame_free(blame);
	git_repository_free(repo);
}


/*
 * $ git blame -n b.txt -L 8
 *    orig line no                          final line no
 * commit    V  author     timestamp                  V
 * 63d671eb  8 (Ben Straub 2013-02-12 15:13:04 -0800  8
 * 63d671eb  9 (Ben Straub 2013-02-12 15:13:04 -0800  9
 * 63d671eb 10 (Ben Straub 2013-02-12 15:13:04 -0800 10
 * aa06ecca  6 (Ben Straub 2013-02-12 15:14:46 -0800 11
 * aa06ecca  7 (Ben Straub 2013-02-12 15:14:46 -0800 12
 * aa06ecca  8 (Ben Straub 2013-02-12 15:14:46 -0800 13
 * aa06ecca  9 (Ben Straub 2013-02-12 15:14:46 -0800 14
 * aa06ecca 10 (Ben Straub 2013-02-12 15:14:46 -0800 15
 *
 * $ git blame -n b.txt -L ,6
 *    orig line no                          final line no
 * commit    V  author     timestamp                  V
 * da237394  1 (Ben Straub 2013-02-12 15:11:30 -0800  1
 * da237394  2 (Ben Straub 2013-02-12 15:11:30 -0800  2
 * da237394  3 (Ben Straub 2013-02-12 15:11:30 -0800  3
 * da237394  4 (Ben Straub 2013-02-12 15:11:30 -0800  4
 * ^b99f7ac  1 (Ben Straub 2013-02-12 15:10:12 -0800  5
 * 63d671eb  6 (Ben Straub 2013-02-12 15:13:04 -0800  6
 *
 * $ git blame -n b.txt -L 2,7
 *    orig line no                          final line no
 * commit   V  author     timestamp                 V
 * da237394 2 (Ben Straub 2013-02-12 15:11:30 -0800 2
 * da237394 3 (Ben Straub 2013-02-12 15:11:30 -0800 3
 * da237394 4 (Ben Straub 2013-02-12 15:11:30 -0800 4
 * ^b99f7ac 1 (Ben Straub 2013-02-12 15:10:12 -0800 5
 * 63d671eb 6 (Ben Straub 2013-02-12 15:13:04 -0800 6
 * 63d671eb 7 (Ben Straub 2013-02-12 15:13:04 -0800 7
 */
void test_blame_simple__can_restrict_to_lines(void)
{
	git_blame *blame = NULL;
	git_repository *repo;
	git_blame_options opts = GIT_BLAME_OPTIONS_INIT;

	cl_git_pass(git_repository_open(&repo, cl_fixture("blametest.git")));

	opts.min_line = 8;
	cl_git_pass(git_blame_file(&blame, repo, "b.txt", &opts));
	cl_assert_equal_i(2, git_blame_get_hunk_count(blame));
	check_blame_hunk_index(repo, blame, 0,  8, 3, "63d671eb", "b.txt");
	check_blame_hunk_index(repo, blame, 1, 11, 5, "aa06ecca", "b.txt");

	opts.min_line = 0;
	opts.max_line = 6;
	cl_git_pass(git_blame_file(&blame, repo, "b.txt", &opts));
	cl_assert_equal_i(3, git_blame_get_hunk_count(blame));
	check_blame_hunk_index(repo, blame, 0,  1, 4, "da237394", "b.txt");
	check_blame_hunk_index(repo, blame, 1,  5, 1, "b99f7ac0", "b.txt");
	check_blame_hunk_index(repo, blame, 2,  6, 1, "63d671eb", "b.txt");

	opts.min_line = 2;
	opts.max_line = 7;
	cl_git_pass(git_blame_file(&blame, repo, "b.txt", &opts));
	cl_assert_equal_i(3, git_blame_get_hunk_count(blame));
	check_blame_hunk_index(repo, blame, 0,  2, 3, "da237394", "b.txt");
	check_blame_hunk_index(repo, blame, 1,  5, 1, "b99f7ac0", "b.txt");
	check_blame_hunk_index(repo, blame, 2,  6, 2, "63d671eb", "b.txt");

	git_blame_free(blame);
	git_repository_free(repo);
}

/* TODO: no newline at end of file? */
