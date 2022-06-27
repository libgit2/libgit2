#include "clar_libgit2.h"

void test_shallow_feature_flag__set_feature_flag(void)
{
    cl_must_pass(git_libgit2_opts(GIT_OPT_ENABLE_SHALLOW, 1));
}

void test_shallow_feature_flag__unset_feature_flag(void)
{
    cl_must_pass(git_libgit2_opts(GIT_OPT_ENABLE_SHALLOW, 0));
}
