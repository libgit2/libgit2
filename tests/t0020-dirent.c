#include "test_lib.h"
#include "fileops.h"

static char path_buffer[GIT_PATH_MAX];
static int state_loc;
static const char* names[] = {
	"./a",
	"./asdf",
	"./pack-foo.pack",
	NULL
};

static int one_entry(void *state, char *path)
{
	const char **c;

	must_be_true(state == &state_loc);
	must_be_true(path == path_buffer);
	for (c = names; *c; c++) {
		if (!strcmp(*c, path)) {
			*c = "";
			return 0;
		}
	}
	test_die("unexpected path \"%s\"", path);
}

BEGIN_TEST(setup)
	const char **c;
	for (c = names; *c; c++) {
		git_file fd = gitfo_creat(*c, 0600);
		must_be_true(fd >= 0);
		gitfo_close(fd);
	}
END_TEST

BEGIN_TEST(direent_walk)
	const char **c;

	strcpy(path_buffer, ".");
	must_pass(gitfo_dirent(path_buffer,
	                       sizeof(path_buffer),
	                       one_entry,
	                       &state_loc));

	for (c = names; *c; c++)
		must_pass(strcmp("", *c));
END_TEST
