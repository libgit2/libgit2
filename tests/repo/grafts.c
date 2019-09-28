#include "clar_libgit2.h"
#include "fileops.h"
#include "graft.h"

static git_repository *g_repo;

void test_repo_grafts__initialize(void)
{
	g_repo = cl_git_sandbox_init("grafted.git");
}

void test_repo_grafts__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_repo_grafts__graft_register(void)
{
	git_oid oid_src;
	git_commit_graft *graft;
	git_graftmap *grafts = git_oidmap_alloc();
	git_array_oid_t parents = GIT_ARRAY_INIT;

	git_oid *oid1 = git_array_alloc(parents);
	cl_git_pass(git_oid_fromstr(&oid_src, "2f3053cbff8a4ca2f0666de364ddb734a28a31a9"));
	git_oid_cpy(oid1, &oid_src);

	git_oid_fromstr(&oid_src, "f503807ffa920e407a600cfaee96b7152259acc7");
	cl_git_pass(git__graft_register(grafts, &oid_src, parents));
	git_array_clear(parents);

	cl_assert_equal_i(1, git_oidmap_size(grafts));
	cl_git_pass(git__graft_for_oid(&graft, grafts, &oid_src));
	cl_assert_equal_s("f503807ffa920e407a600cfaee96b7152259acc7", git_oid_tostr_s(&graft->oid));
	cl_assert_equal_i(1, git_array_size(graft->parents));
	cl_assert_equal_s("2f3053cbff8a4ca2f0666de364ddb734a28a31a9", git_oid_tostr_s(git_array_get(graft->parents, 0)));

	git__graft_clear(grafts);
	git_oidmap_free(grafts);
}

void test_repo_grafts__grafted_revwalk(void)
{
	git_revwalk *w;
	git_oid oids[10];
	size_t i = 0;
	git_commit *commit;

	cl_git_pass(git_revwalk_new(&w, g_repo));
	cl_git_pass(git_revwalk_push_ref(w, "refs/heads/branch"));

	cl_git_pass(git_revwalk_next(&oids[i++], w));
	cl_assert_equal_s(git_oid_tostr_s(&oids[0]), "8a00e91619098618be97c0d2ceabb05a2c58edd9");
	cl_git_pass(git_revwalk_next(&oids[i++], w));
	cl_assert_equal_s(git_oid_tostr_s(&oids[1]), "f503807ffa920e407a600cfaee96b7152259acc7");
	cl_git_pass(git_revwalk_next(&oids[i++], w));
	cl_assert_equal_s(git_oid_tostr_s(&oids[2]), "2f3053cbff8a4ca2f0666de364ddb734a28a31a9");

	cl_git_fail_with(GIT_ITEROVER, git_revwalk_next(&oids[i++], w));

	cl_git_pass(git_commit_lookup(&commit, g_repo, &oids[0]));

	cl_assert_equal_i(1, git_commit_parentcount(commit));

	git_commit_free(commit);
	git_revwalk_free(w);
}

void test_repo_grafts__grafted_objects(void)
{
	git_oid oid;
	git_commit *commit;

	cl_git_pass(git_oid_fromstr(&oid, "f503807ffa920e407a600cfaee96b7152259acc7"));
	cl_git_pass(git_commit_lookup(&commit, g_repo, &oid));
	cl_assert_equal_i(1, git_commit_parentcount(commit));
	git_commit_free(commit);

	cl_git_pass(git_oid_fromstr(&oid, "0512adebd3782157f0d5c9b22b043f87b4aaff9e"));
	cl_git_pass(git_commit_lookup(&commit, g_repo, &oid));
	cl_assert_equal_i(1, git_commit_parentcount(commit));
	git_commit_free(commit);

	cl_git_pass(git_oid_fromstr(&oid, "66cc22a015f6ca75b34c82d28f78ba663876bade"));
	cl_git_pass(git_commit_lookup(&commit, g_repo, &oid));
	cl_assert_equal_i(4, git_commit_parentcount(commit));
	git_commit_free(commit);
}

void test_repo_grafts__grafted_merge_revwalk(void)
{
	git_revwalk *w;
	git_oid oids[10];
	size_t i = 0;

	cl_git_pass(git_revwalk_new(&w, g_repo));
	cl_git_pass(git_revwalk_push_ref(w, "refs/heads/bottom"));

	cl_git_pass(git_revwalk_next(&oids[i++], w));
	cl_assert_equal_s(git_oid_tostr_s(&oids[i - 1]), "66cc22a015f6ca75b34c82d28f78ba663876bade");
	cl_git_pass(git_revwalk_next(&oids[i++], w));
	cl_assert_equal_s(git_oid_tostr_s(&oids[i - 1]), "e414f42f4e6bc6934563a2349a8600f0ab68618e");
	cl_git_pass(git_revwalk_next(&oids[i++], w));
	cl_assert_equal_s(git_oid_tostr_s(&oids[i - 1]), "8a00e91619098618be97c0d2ceabb05a2c58edd9");
	cl_git_pass(git_revwalk_next(&oids[i++], w));
	cl_assert_equal_s(git_oid_tostr_s(&oids[i - 1]), "1c18e80a276611bb9b146590616bbc5aebdf2945");
	cl_git_pass(git_revwalk_next(&oids[i++], w));
	cl_assert_equal_s(git_oid_tostr_s(&oids[i - 1]), "d7224d49d6d5aff6ade596ed74f4bcd4f77b29e2");
	cl_git_pass(git_revwalk_next(&oids[i++], w));
	cl_assert_equal_s(git_oid_tostr_s(&oids[i - 1]), "0512adebd3782157f0d5c9b22b043f87b4aaff9e");
	cl_git_pass(git_revwalk_next(&oids[i++], w));
	cl_assert_equal_s(git_oid_tostr_s(&oids[i - 1]), "f503807ffa920e407a600cfaee96b7152259acc7");
	cl_git_pass(git_revwalk_next(&oids[i++], w));
	cl_assert_equal_s(git_oid_tostr_s(&oids[i - 1]), "2f3053cbff8a4ca2f0666de364ddb734a28a31a9");

	cl_git_fail_with(GIT_ITEROVER, git_revwalk_next(&oids[i++], w));

	git_revwalk_free(w);
}
