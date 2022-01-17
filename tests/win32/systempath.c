#include "clar_libgit2.h"
#include "futils.h"
#include "sysdir.h"
#include "win32/findfile.h"

static char *path_save;
static git_str gfw_root = GIT_STR_INIT;

void test_win32_systempath__initialize(void)
{
    path_save = cl_getenv("PATH");

    cl_git_pass(git_str_puts(&gfw_root, clar_sandbox_path()));
    cl_git_pass(git_str_puts(&gfw_root, "/fake_gfw_install"));
}

void test_win32_systempath__cleanup(void)
{
    cl_fixture_cleanup("fake_gfw_install");
    git_str_dispose(&gfw_root);

    cl_setenv("PATH", path_save);
    git__free(path_save);
    path_save = NULL;

    git_sysdir_reset();
}

static void fix_path(git_str *s)
{
    char *c;

    for (c = s->ptr; *c; c++) {
	if (*c == '/')
	    *c = '\\';
    }
}

void test_win32_systempath__etc_gitconfig(void)
{
    git_str bin_path = GIT_STR_INIT, exe_path = GIT_STR_INIT,
	    etc_path = GIT_STR_INIT, config_path = GIT_STR_INIT,
	    path_env = GIT_STR_INIT, out = GIT_STR_INIT;
    git_config *cfg;
    int value;

    cl_git_pass(git_str_puts(&bin_path, gfw_root.ptr));
    cl_git_pass(git_str_puts(&bin_path, "/cmd"));
    cl_git_pass(git_futils_mkdir_r(bin_path.ptr, 0755));

    cl_git_pass(git_str_puts(&exe_path, bin_path.ptr));
    cl_git_pass(git_str_puts(&exe_path, "/git.cmd"));
    cl_git_mkfile(exe_path.ptr, "This is a fake executable.");

    cl_git_pass(git_str_puts(&etc_path, gfw_root.ptr));
    cl_git_pass(git_str_puts(&etc_path, "/etc"));
    cl_git_pass(git_futils_mkdir_r(etc_path.ptr, 0755));

    git_str_clear(&etc_path);

    cl_git_pass(git_str_puts(&etc_path, gfw_root.ptr));
    cl_git_pass(git_str_puts(&etc_path, "/etc"));
    cl_git_pass(git_futils_mkdir_r(etc_path.ptr, 0755));

    cl_git_pass(git_str_puts(&config_path, etc_path.ptr));
    cl_git_pass(git_str_puts(&config_path, "/gitconfig"));
    cl_git_mkfile(config_path.ptr, "[gfw]\n\ttest = 1337\n");

    fix_path(&bin_path);

    cl_git_pass(git_str_puts(&path_env, "C:\\GitTempTest\\Foo;\"c:\\program files\\doesnotexisttesttemp\";"));
    cl_git_pass(git_str_puts(&path_env, bin_path.ptr));
    cl_git_pass(git_str_puts(&path_env, ";C:\\fakefakedoesnotexist"));
    cl_setenv("PATH", path_env.ptr);

    cl_git_pass(git_win32__find_system_dir_in_path(&out, L"etc"));
    cl_assert_equal_s(out.ptr, etc_path.ptr);

    git_sysdir_reset();

    cl_git_pass(git_config_open_default(&cfg));
    cl_git_pass(git_config_get_int32(&value, cfg, "gfw.test"));
    cl_assert_equal_i(1337, value);

    git_str_dispose(&exe_path);
    git_str_dispose(&etc_path);
    git_str_dispose(&config_path);
    git_str_dispose(&path_env);
    git_str_dispose(&out);
    git_config_free(cfg);
}
