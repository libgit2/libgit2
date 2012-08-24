#include "clar_libgit2.h"

static git_repository *_repo;

void test_revwalk_mergebase__initialize(void)
{
	cl_git_pass(git_repository_open(&_repo, cl_fixture("testrepo.git")));
}

void test_revwalk_mergebase__cleanup(void)
{
	git_repository_free(_repo);
}

void test_revwalk_mergebase__single1(void)
{
	git_oid result, one, two, expected;

	git_oid_fromstr(&one, "c47800c7266a2be04c571c04d5a6614691ea99bd ");
	git_oid_fromstr(&two, "9fd738e8f7967c078dceed8190330fc8648ee56a");
	git_oid_fromstr(&expected, "5b5b025afb0b4c913b4c338a42934a3863bf3644");

	cl_git_pass(git_merge_base(&result, _repo, &one, &two));
	cl_assert(git_oid_cmp(&result, &expected) == 0);
}

void test_revwalk_mergebase__single2(void)
{
	git_oid result, one, two, expected;

	git_oid_fromstr(&one, "763d71aadf09a7951596c9746c024e7eece7c7af");
	git_oid_fromstr(&two, "a65fedf39aefe402d3bb6e24df4d4f5fe4547750");
	git_oid_fromstr(&expected, "c47800c7266a2be04c571c04d5a6614691ea99bd");

	cl_git_pass(git_merge_base(&result, _repo, &one, &two));
	cl_assert(git_oid_cmp(&result, &expected) == 0);
}

void test_revwalk_mergebase__merged_branch(void)
{
	git_oid result, one, two, expected;

	git_oid_fromstr(&one, "a65fedf39aefe402d3bb6e24df4d4f5fe4547750");
	git_oid_fromstr(&two, "9fd738e8f7967c078dceed8190330fc8648ee56a");
	git_oid_fromstr(&expected, "9fd738e8f7967c078dceed8190330fc8648ee56a");

	cl_git_pass(git_merge_base(&result, _repo, &one, &two));
	cl_assert(git_oid_cmp(&result, &expected) == 0);

	cl_git_pass(git_merge_base(&result, _repo, &two, &one));
	cl_assert(git_oid_cmp(&result, &expected) == 0);
}

void test_revwalk_mergebase__no_common_ancestor_returns_ENOTFOUND(void)
{
	git_oid result, one, two, expected;
	int error;

	git_oid_fromstr(&one, "763d71aadf09a7951596c9746c024e7eece7c7af");
	git_oid_fromstr(&two, "e90810b8df3e80c413d903f631643c716887138d");
	git_oid_fromstr(&expected, "c47800c7266a2be04c571c04d5a6614691ea99bd");

	error = git_merge_base(&result, _repo, &one, &two);
	cl_git_fail(error);

	cl_assert_equal_i(GIT_ENOTFOUND, error);
}

/*
 * $ git log --graph --all
 * * commit 763d71aadf09a7951596c9746c024e7eece7c7af
 * | Author: nulltoken <emeric.fermas@gmail.com>
 * | Date:   Sun Oct 9 12:54:47 2011 +0200
 * |
 * |     Add some files into subdirectories
 * |
 * | * commit a65fedf39aefe402d3bb6e24df4d4f5fe4547750
 * | | Author: Scott Chacon <schacon@gmail.com>
 * | | Date:   Tue Aug 9 19:33:46 2011 -0700
 * | |
 * | *   commit be3563ae3f795b2b4353bcce3a527ad0a4f7f644
 * | |\  Merge: 9fd738e c47800c
 * | |/  Author: Scott Chacon <schacon@gmail.com>
 * |/|   Date:   Tue May 25 11:58:27 2010 -0700
 * | |
 * | |       Merge branch 'br2'
 * | |
 * | | * commit e90810b8df3e80c413d903f631643c716887138d
 * | | | Author: Vicent Marti <tanoku@gmail.com>
 * | | | Date:   Thu Aug 5 18:42:20 2010 +0200
 * | | |
 * | | |     Test commit 2
 * | | |
 * | | * commit 6dcf9bf7541ee10456529833502442f385010c3d
 * | |   Author: Vicent Marti <tanoku@gmail.com>
 * | |   Date:   Thu Aug 5 18:41:33 2010 +0200
 * | |
 * | |       Test commit 1
 * | |
 * | | *   commit a4a7dce85cf63874e984719f4fdd239f5145052f
 * | | |\  Merge: c47800c 9fd738e
 * | |/ /  Author: Scott Chacon <schacon@gmail.com>
 * |/| /   Date:   Tue May 25 12:00:23 2010 -0700
 * | |/
 * | |         Merge branch 'master' into br2
 * | |
 * | * commit 9fd738e8f7967c078dceed8190330fc8648ee56a
 * | | Author: Scott Chacon <schacon@gmail.com>
 * | | Date:   Mon May 24 10:19:19 2010 -0700
 * | |
 * | |     a fourth commit
 * | |
 * | * commit 4a202b346bb0fb0db7eff3cffeb3c70babbd2045
 * | | Author: Scott Chacon <schacon@gmail.com>
 * | | Date:   Mon May 24 10:19:04 2010 -0700
 * | |
 * | |     a third commit
 * | |
 * * | commit c47800c7266a2be04c571c04d5a6614691ea99bd
 * |/  Author: Scott Chacon <schacon@gmail.com>
 * |   Date:   Tue May 25 11:58:14 2010 -0700
 * |
 * |       branch commit one
 * |
 * * commit 5b5b025afb0b4c913b4c338a42934a3863bf3644
 * | Author: Scott Chacon <schacon@gmail.com>
 * | Date:   Tue May 11 13:38:42 2010 -0700
 * |
 * |     another commit
 * |
 * * commit 8496071c1b46c854b31185ea97743be6a8774479
 *   Author: Scott Chacon <schacon@gmail.com>
 *   Date:   Sat May 8 16:13:06 2010 -0700
 * 
 *       testing
 * 
 * * commit 41bc8c69075bbdb46c5c6f0566cc8cc5b46e8bd9
 * | Author: Scott Chacon <schacon@gmail.com>
 * | Date:   Tue May 11 13:40:41 2010 -0700
 * |
 * |     packed commit two
 * |
 * * commit 5001298e0c09ad9c34e4249bc5801c75e9754fa5
 *   Author: Scott Chacon <schacon@gmail.com>
 *   Date:   Tue May 11 13:40:23 2010 -0700
 * 
 *       packed commit one
 */
