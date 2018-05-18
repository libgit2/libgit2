#include "clar_libgit2.h"
#include "path.h"

static char *gitmodules_altnames[] = {
	".gitmodules",

	".git\u200cmodules",

	".Gitmodules",
	".gitmoduleS",

	".gitmodules ",
	".gitmodules.",
	".gitmodules  ",
	".gitmodules. ",
	".gitmodules .",
	".gitmodules..",
	".gitmodules   ",
	".gitmodules.  ",
	".gitmodules . ",
	".gitmodules  .",

	".Gitmodules ",
	".Gitmodules.",
	".Gitmodules  ",
	".Gitmodules. ",
	".Gitmodules .",
	".Gitmodules..",
	".Gitmodules   ",
	".Gitmodules.  ",
	".Gitmodules . ",
	".Gitmodules  .",

	"GITMOD~1",
	"gitmod~1",
	"GITMOD~2",
	"gitmod~3",
	"GITMOD~4",

	"GITMOD~1 ",
	"gitmod~2.",
	"GITMOD~3  ",
	"gitmod~4. ",
	"GITMOD~1 .",
	"gitmod~2   ",
	"GITMOD~3.  ",
	"gitmod~4 . ",

	"GI7EBA~1",
	"gi7eba~9",

	"GI7EB~10",
	"GI7EB~11",
	"GI7EB~99",
	"GI7EB~10",
	"GI7E~100",
	"GI7E~101",
	"GI7E~999",
	"~1000000",
	"~9999999",
};

static char *gitmodules_not_altnames[] = {
	".gitmodules x",
	".gitmodules .x",

	" .gitmodules",

	"..gitmodules",

	"gitmodules",

	".gitmodule",

	".gitmodules x ",
	".gitmodules .x",

	"GI7EBA~",
	"GI7EBA~0",
	"GI7EBA~~1",
	"GI7EBA~X",
	"Gx7EBA~1",
	"GI7EBX~1",

	"GI7EB~1",
	"GI7EB~01",
	"GI7EB~1",
};

void test_path_dotgit__dotgit_modules(void)
{
	size_t i;
	cl_assert_equal_i(1, git_path_is_dotgit_modules(".gitmodules"));
	cl_assert_equal_i(1, git_path_is_dotgit_modules(".git\xe2\x80\x8cmodules"));

	for (i = 0; i < ARRAY_SIZE(gitmodules_altnames); i++) {
		const char *name = gitmodules_altnames[i];
		if (!git_path_is_dotgit_modules(name))
			cl_fail(name);
	}

	for (i = 0; i < ARRAY_SIZE(gitmodules_not_altnames); i++) {
		const char *name = gitmodules_not_altnames[i];
		if (git_path_is_dotgit_modules(name))
			cl_fail(name);
	}

}
