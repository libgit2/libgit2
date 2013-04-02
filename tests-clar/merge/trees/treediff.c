#include "clar_libgit2.h"
#include "git2/tree.h"
#include "merge.h"
#include "../merge_helpers.h"
#include "diff.h"
#include "hashsig.h"

static git_repository *repo;

#define TEST_REPO_PATH "merge-resolve"

#define TREE_OID_ANCESTOR		"0d52e3a556e189ba0948ae56780918011c1b167d"
#define TREE_OID_MASTER			"1f81433e3161efbf250576c58fede7f6b836f3d3"
#define TREE_OID_BRANCH			"eea9286df54245fea72c5b557291470eb825f38f"

#define TREE_OID_DF_ANCESTOR	"b8a3a806d3950e8c0a03a34f234a92eff0e2c68d"
#define TREE_OID_DF_SIDE1		"ee1d6f164893c1866a323f072eeed36b855656be"
#define TREE_OID_DF_SIDE2		"6178885b38fe96e825ac0f492c0a941f288b37f6"

void test_merge_trees_treediff__initialize(void)
{
	repo = cl_git_sandbox_init(TEST_REPO_PATH);
}

void test_merge_trees_treediff__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

struct treediff_cb_data {
    struct merge_index_conflict_data *conflict_data;
    size_t conflict_data_len;

    size_t idx;
};

static void test_find_differences(
    const char *ancestor_oidstr,
    const char *ours_oidstr,
    const char *theirs_oidstr,
    struct merge_index_conflict_data *treediff_conflict_data,
    size_t treediff_conflict_data_len)
{
    git_merge_diff_list *merge_diff_list = git_merge_diff_list__alloc(repo);
    git_oid ancestor_oid, ours_oid, theirs_oid;
    git_tree *ancestor_tree, *ours_tree, *theirs_tree;
    struct treediff_cb_data treediff_cb_data = {0};

    cl_git_pass(git_oid_fromstr(&ancestor_oid, ancestor_oidstr));
    cl_git_pass(git_oid_fromstr(&ours_oid, ours_oidstr));
    cl_git_pass(git_oid_fromstr(&theirs_oid, theirs_oidstr));
    
    cl_git_pass(git_tree_lookup(&ancestor_tree, repo, &ancestor_oid));
    cl_git_pass(git_tree_lookup(&ours_tree, repo, &ours_oid));
    cl_git_pass(git_tree_lookup(&theirs_tree, repo, &theirs_oid));
    
	cl_git_pass(git_merge_diff_list__find_differences(merge_diff_list, ancestor_tree, ours_tree, theirs_tree));

	/*
	dump_merge_index(merge_index);
	 */
	
    cl_assert(treediff_conflict_data_len == merge_diff_list->conflicts.length);
    
    treediff_cb_data.conflict_data = treediff_conflict_data;
	treediff_cb_data.conflict_data_len = treediff_conflict_data_len;

	cl_assert(merge_test_merge_conflicts(&merge_diff_list->conflicts, treediff_conflict_data, treediff_conflict_data_len));

    git_tree_free(ancestor_tree);
    git_tree_free(ours_tree);
    git_tree_free(theirs_tree);
    
	git_merge_diff_list__free(merge_diff_list);
}

void test_merge_trees_treediff__simple(void)
{
    struct merge_index_conflict_data treediff_conflict_data[] = {
		{
			{ 0, "", 0, "", GIT_DELTA_UNMODIFIED },
			{ 0100644, "233c0919c998ed110a4b6ff36f353aec8b713487", 0, "added-in-master.txt", GIT_DELTA_ADDED },
			{ 0, "", 0, "", GIT_DELTA_UNMODIFIED },
			GIT_MERGE_DIFF_NONE
		},

        {
			{ 0100644, "6212c31dab5e482247d7977e4f0dd3601decf13b", 0, "automergeable.txt", GIT_DELTA_UNMODIFIED },
			{ 0100644, "ee3fa1b8c00aff7fe02065fdb50864bb0d932ccf", 0, "automergeable.txt", GIT_DELTA_MODIFIED },
			{ 0100644, "058541fc37114bfc1dddf6bd6bffc7fae5c2e6fe", 0, "automergeable.txt", GIT_DELTA_MODIFIED },
			GIT_MERGE_DIFF_BOTH_MODIFIED
		},
		
		{
			{ 0100644, "ab6c44a2e84492ad4b41bb6bac87353e9d02ac8b", 0, "changed-in-branch.txt", GIT_DELTA_UNMODIFIED },
			{ 0100644, "ab6c44a2e84492ad4b41bb6bac87353e9d02ac8b", 0, "changed-in-branch.txt", GIT_DELTA_UNMODIFIED },
			{ 0100644, "4eb04c9e79e88f6640d01ff5b25ca2a60764f216", 0, "changed-in-branch.txt", GIT_DELTA_MODIFIED },
			GIT_MERGE_DIFF_NONE
		},
		
		{
			{ 0100644, "ab6c44a2e84492ad4b41bb6bac87353e9d02ac8b", 0, "changed-in-master.txt", GIT_DELTA_UNMODIFIED },
			{ 0100644, "11deab00b2d3a6f5a3073988ac050c2d7b6655e2", 0, "changed-in-master.txt", GIT_DELTA_MODIFIED },
			{ 0100644, "ab6c44a2e84492ad4b41bb6bac87353e9d02ac8b", 0, "changed-in-master.txt", GIT_DELTA_UNMODIFIED },
			GIT_MERGE_DIFF_NONE
        },
		
		{
			{ 0100644, "d427e0b2e138501a3d15cc376077a3631e15bd46", 0, "conflicting.txt", GIT_DELTA_UNMODIFIED },
			{ 0100644, "4e886e602529caa9ab11d71f86634bd1b6e0de10", 0, "conflicting.txt", GIT_DELTA_MODIFIED },
			{ 0100644, "2bd0a343aeef7a2cf0d158478966a6e587ff3863", 0, "conflicting.txt", GIT_DELTA_MODIFIED },
			GIT_MERGE_DIFF_BOTH_MODIFIED
        },
		
		{
			{ 0100644, "dfe3f22baa1f6fce5447901c3086bae368de6bdd", 0, "removed-in-branch.txt", GIT_DELTA_UNMODIFIED },
			{ 0100644, "dfe3f22baa1f6fce5447901c3086bae368de6bdd", 0, "removed-in-branch.txt", GIT_DELTA_UNMODIFIED },
			{ 0, "", 0, "", GIT_DELTA_DELETED },
			GIT_MERGE_DIFF_NONE
        },
		
		{
			{ 0100644, "5c3b68a71fc4fa5d362fd3875e53137c6a5ab7a5", 0, "removed-in-master.txt", GIT_DELTA_UNMODIFIED },
			{ 0, "", 0, "", GIT_DELTA_DELETED },
			{ 0100644, "5c3b68a71fc4fa5d362fd3875e53137c6a5ab7a5", 0, "removed-in-master.txt", GIT_DELTA_UNMODIFIED },
			GIT_MERGE_DIFF_NONE
		},
    };
    
    test_find_differences(TREE_OID_ANCESTOR, TREE_OID_MASTER, TREE_OID_BRANCH, treediff_conflict_data, 7);
}

void test_merge_trees_treediff__df_conflicts(void)
{
    struct merge_index_conflict_data treediff_conflict_data[] = {
		{
			{ 0, "", 0, "", GIT_DELTA_UNMODIFIED },
			{ 0100644, "49130a28ef567af9a6a6104c38773fedfa5f9742", 0, "dir-10", GIT_DELTA_ADDED },
			{ 0100644, "6c06dcd163587c2cc18be44857e0b71116382aeb", 0, "dir-10", GIT_DELTA_ADDED },
			GIT_MERGE_DIFF_BOTH_ADDED,
		},

		{
			{ 0100644, "242591eb280ee9eeb2ce63524b9a8b9bc4cb515d", 0, "dir-10/file.txt", GIT_DELTA_UNMODIFIED },
			{ 0, "", 0, "", GIT_DELTA_DELETED },
			{ 0, "", 0, "", GIT_DELTA_DELETED },
			GIT_MERGE_DIFF_BOTH_DELETED,
		},
		
		{
			{ 0, "", 0, "", GIT_DELTA_UNMODIFIED },
			{ 0, "", 0, "", GIT_DELTA_UNMODIFIED },
			{ 0100644, "43aafd43bea779ec74317dc361f45ae3f532a505", 0, "dir-6", GIT_DELTA_ADDED },
			GIT_MERGE_DIFF_NONE,
		},

		{
			{ 0100644, "cf8c5cc8a85a1ff5a4ba51e0bc7cf5665669924d", 0, "dir-6/file.txt", GIT_DELTA_UNMODIFIED },
			{ 0100644, "cf8c5cc8a85a1ff5a4ba51e0bc7cf5665669924d", 0, "dir-6/file.txt", GIT_DELTA_UNMODIFIED },
			{ 0, "", 0, "", GIT_DELTA_DELETED },
			GIT_MERGE_DIFF_NONE,
		},
		
		{
			{ 0, "", 0, "", GIT_DELTA_UNMODIFIED },
			{ 0, "", 0, "", GIT_DELTA_UNMODIFIED },
			{ 0100644, "a031a28ae70e33a641ce4b8a8f6317f1ab79dee4", 0, "dir-7", GIT_DELTA_ADDED },
			GIT_MERGE_DIFF_DIRECTORY_FILE,
		},

		{
			{ 0100644, "5012fd565b1393bdfda1805d4ec38ce6619e1fd1", 0, "dir-7/file.txt", GIT_DELTA_UNMODIFIED },
			{ 0100644, "a5563304ddf6caba25cb50323a2ea6f7dbfcadca", 0, "dir-7/file.txt", GIT_DELTA_MODIFIED },
			{ 0, "", 0, "", GIT_DELTA_DELETED },
			GIT_MERGE_DIFF_DF_CHILD,
		},
		
		{
			{ 0, "", 0, "", GIT_DELTA_UNMODIFIED },
			{ 0100644, "e9ad6ec3e38364a3d07feda7c4197d4d845c53b5", 0, "dir-8", GIT_DELTA_ADDED },
			{ 0, "", 0, "", GIT_DELTA_UNMODIFIED },
			GIT_MERGE_DIFF_NONE,
		},

		{
			{ 0100644, "f20c9063fa0bda9a397c96947a7b687305c49753", 0, "dir-8/file.txt", GIT_DELTA_UNMODIFIED },
			{ 0, "", 0, "", GIT_DELTA_DELETED },
			{ 0100644, "f20c9063fa0bda9a397c96947a7b687305c49753", 0, "dir-8/file.txt", GIT_DELTA_UNMODIFIED },
			GIT_MERGE_DIFF_NONE,
		},
		
		{
			{ 0, "", 0, "", GIT_DELTA_UNMODIFIED },
			{ 0100644, "3ef4d30382ca33fdeba9fda895a99e0891ba37aa", 0, "dir-9", GIT_DELTA_ADDED },
			{ 0, "", 0, "", GIT_DELTA_UNMODIFIED },
			GIT_MERGE_DIFF_DIRECTORY_FILE,
		},

		{
			{ 0100644, "fc4c636d6515e9e261f9260dbcf3cc6eca97ea08", 0, "dir-9/file.txt", GIT_DELTA_UNMODIFIED },
			{ 0, "", 0, "", GIT_DELTA_DELETED },
			{ 0100644, "76ab0e2868197ec158ddd6c78d8a0d2fd73d38f9", 0, "dir-9/file.txt", GIT_DELTA_MODIFIED },
			GIT_MERGE_DIFF_DF_CHILD,
		},

		{
			{ 0100644, "1e4ff029aee68d0d69ef9eb6efa6cbf1ec732f99", 0, "file-1",  GIT_DELTA_UNMODIFIED },
			{ 0100644, "1e4ff029aee68d0d69ef9eb6efa6cbf1ec732f99", 0, "file-1",  GIT_DELTA_UNMODIFIED },
			{ 0, "", 0, "", GIT_DELTA_DELETED },
			GIT_MERGE_DIFF_NONE,
		},

		{
			{ 0, "", 0, "", GIT_DELTA_UNMODIFIED },
			{ 0, "", 0, "", GIT_DELTA_UNMODIFIED },
			{ 0100644, "5c2411f8075f48a6b2fdb85ebc0d371747c4df15", 0, "file-1/new", GIT_DELTA_ADDED },
			GIT_MERGE_DIFF_NONE,
		},

		{
			{ 0100644, "a39a620dae5bc8b4e771cd4d251b7d080401a21e", 0, "file-2", GIT_DELTA_UNMODIFIED },
			{ 0100644, "d963979c237d08b6ba39062ee7bf64c7d34a27f8", 0, "file-2", GIT_DELTA_MODIFIED },
			{ 0, "", 0, "", GIT_DELTA_DELETED },
			GIT_MERGE_DIFF_DIRECTORY_FILE,
		},
		
		{
			{ 0, "", 0, "", GIT_DELTA_UNMODIFIED },
			{ 0, "", 0, "", GIT_DELTA_UNMODIFIED },
			{ 0100644, "5c341ead2ba6f2af98ce5ec3fe84f6b6d2899c0d", 0, "file-2/new", GIT_DELTA_ADDED },
			GIT_MERGE_DIFF_DF_CHILD,
		},

		{
			{ 0100644, "032ebc5ab85d9553bb187d3cd40875ff23a63ed0", 0, "file-3", GIT_DELTA_UNMODIFIED },
			{ 0, "", 0, "", GIT_DELTA_DELETED },
			{ 0100644, "032ebc5ab85d9553bb187d3cd40875ff23a63ed0", 0, "file-3", GIT_DELTA_UNMODIFIED },
			GIT_MERGE_DIFF_NONE,
		},

		{
			{ 0, "", 0, "", GIT_DELTA_UNMODIFIED },
			{ 0100644, "9efe7723802d4305142eee177e018fee1572c4f4", 0, "file-3/new", GIT_DELTA_ADDED },
			{ 0, "", 0, "", GIT_DELTA_UNMODIFIED },
			GIT_MERGE_DIFF_NONE,
		},

		{
			{ 0100644, "bacac9b3493509aa15e1730e1545fc0919d1dae0", 0, "file-4", GIT_DELTA_UNMODIFIED },
			{ 0, "", 0, "", GIT_DELTA_DELETED },
			{ 0100644, "7663fce0130db092936b137cabd693ec234eb060", 0, "file-4", GIT_DELTA_MODIFIED },
			GIT_MERGE_DIFF_DIRECTORY_FILE,
		},

		{
			{ 0, "", 0, "", GIT_DELTA_UNMODIFIED },
			{ 0100644, "e49f917b448d1340b31d76e54ba388268fd4c922", 0, "file-4/new", GIT_DELTA_ADDED },
			{ 0, "", 0, "", GIT_DELTA_UNMODIFIED },
			GIT_MERGE_DIFF_DF_CHILD,
		},

		{
			{ 0100644, "ac4045f965119e6998f4340ed0f411decfb3ec05", 0, "file-5", GIT_DELTA_UNMODIFIED },
			{ 0, "", 0, "", GIT_DELTA_DELETED },
			{ 0, "", 0, "", GIT_DELTA_DELETED },
			GIT_MERGE_DIFF_BOTH_DELETED,
		},
		
		{
			{ 0, "", 0, "", GIT_DELTA_UNMODIFIED },
			{ 0100644, "cab2cf23998b40f1af2d9d9a756dc9e285a8df4b", 0, "file-5/new", GIT_DELTA_ADDED },
			{ 0100644, "f5504f36e6f4eb797a56fc5bac6c6c7f32969bf2", 0, "file-5/new", GIT_DELTA_ADDED },
			GIT_MERGE_DIFF_BOTH_ADDED,
		},
	};
	
	test_find_differences(TREE_OID_DF_ANCESTOR, TREE_OID_DF_SIDE1, TREE_OID_DF_SIDE2, treediff_conflict_data, 20);
}

