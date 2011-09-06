#include "clay_libgit2.h"
#include "fileops.h"

typedef struct name_data {
	int count; /* return count */
	char *name; /* filename		*/
} name_data;

typedef struct walk_data {
	char *sub;		/* sub-directory name */
	name_data *names; /* name state data	*/
} walk_data;


static char path_buffer[GIT_PATH_MAX];
static char *top_dir = "dir-walk";
static walk_data *state_loc;

static void setup(walk_data *d)
{
	name_data *n;

	cl_must_pass(p_mkdir(top_dir, 0777));

	cl_must_pass(p_chdir(top_dir));

	if (strcmp(d->sub, ".") != 0)
		cl_must_pass(p_mkdir(d->sub, 0777));

	strcpy(path_buffer, d->sub);
	state_loc = d;

	for (n = d->names; n->name; n++) {
		git_file fd = p_creat(n->name, 0666);
		cl_assert(fd >= 0);
		p_close(fd);
		n->count = 0;
	}
}

static void dirent_cleanup__cb(void *_d)
{
	walk_data *d = _d;
	name_data *n;

	for (n = d->names; n->name; n++) {
		cl_must_pass(p_unlink(n->name));
	}

	if (strcmp(d->sub, ".") != 0)
		cl_must_pass(p_rmdir(d->sub));

	cl_must_pass(p_chdir(".."));

	cl_must_pass(p_rmdir(top_dir));
}

static void check_counts(walk_data *d)
{
	name_data *n;

	for (n = d->names; n->name; n++) {
		cl_assert(n->count == 1);
	}
}

static int one_entry(void *state, char *path)
{
	walk_data *d = (walk_data *) state;
	name_data *n;

	if (state != state_loc)
		return GIT_ERROR;

	if (path != path_buffer)
		return GIT_ERROR;

	for (n = d->names; n->name; n++) {
		if (!strcmp(n->name, path)) {
			n->count++;
			return 0;
		}
	}

	return GIT_ERROR;
}

static int dont_call_me(void *GIT_UNUSED(state), char *GIT_UNUSED(path))
{
	GIT_UNUSED_ARG(state)
	GIT_UNUSED_ARG(path)
	return GIT_ERROR;
}



static name_data dot_names[] = {
	{ 0, "./a" },
	{ 0, "./asdf" },
	{ 0, "./pack-foo.pack" },
	{ 0, NULL }
};
static walk_data dot = {
	".",
	dot_names
};

/* make sure that the '.' folder is not traversed */
void test_core_dirent__dont_traverse_dot(void)
{
	cl_set_cleanup(&dirent_cleanup__cb, &dot);
	setup(&dot);

	cl_git_pass(git_futils_direach(path_buffer,
					sizeof(path_buffer),
					one_entry,
					&dot));

	check_counts(&dot);
}


static name_data sub_names[] = {
	{ 0, "sub/a" },
	{ 0, "sub/asdf" },
	{ 0, "sub/pack-foo.pack" },
	{ 0, NULL }
};
static walk_data sub = {
	"sub",
	sub_names
};

/* traverse a subfolder */
void test_core_dirent__traverse_subfolder(void)
{
	cl_set_cleanup(&dirent_cleanup__cb, &sub);
	setup(&sub);

	cl_git_pass(git_futils_direach(path_buffer,
					sizeof(path_buffer),
					one_entry,
					&sub));

	check_counts(&sub);
}


static walk_data sub_slash = {
	"sub/",
	sub_names
};

/* traverse a slash-terminated subfolder */
void test_core_dirent__traverse_slash_terminated_folder(void)
{
	cl_set_cleanup(&dirent_cleanup__cb, &sub_slash);
	setup(&sub_slash);

	cl_git_pass(git_futils_direach(path_buffer,
					sizeof(path_buffer),
					one_entry,
					&sub_slash));

	check_counts(&sub_slash);
}


static name_data empty_names[] = {
	{ 0, NULL }
};
static walk_data empty = {
	"empty",
	empty_names
};

/* make sure that empty folders are not traversed */
void test_core_dirent__dont_traverse_empty_folders(void)
{
	cl_set_cleanup(&dirent_cleanup__cb, &empty);
	setup(&empty);

	cl_git_pass(git_futils_direach(path_buffer,
					sizeof(path_buffer),
					one_entry,
					&empty));

	check_counts(&empty);

	/* make sure callback not called */
	cl_git_pass(git_futils_direach(path_buffer,
					sizeof(path_buffer),
					dont_call_me,
					&empty));
}

static name_data odd_names[] = {
	{ 0, "odd/.a" },
	{ 0, "odd/..c" },
	/* the following don't work on cygwin/win32 */
	/* { 0, "odd/.b." }, */
	/* { 0, "odd/..d.." }, */
	{ 0, NULL }
};
static walk_data odd = {
	"odd",
	odd_names
};

/* make sure that strange looking filenames ('..c') are traversed */
void test_core_dirent__traverse_weird_filenames(void)
{
	cl_set_cleanup(&dirent_cleanup__cb, &odd);
	setup(&odd);

	cl_git_pass(git_futils_direach(path_buffer,
					sizeof(path_buffer),
					one_entry,
					&odd));

	check_counts(&odd);
}
