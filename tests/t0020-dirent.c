#include "test_lib.h"
#include "fileops.h"


typedef struct name_data {
	int  count;  /* return count */
	char *name;  /* filename     */
} name_data;

typedef struct walk_data {
	char *sub;        /* sub-directory name */
	name_data *names; /* name state data    */
} walk_data;


static char path_buffer[GIT_PATH_MAX];
static char *top_dir = "dir-walk";
static walk_data *state_loc;


static int error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	return -1;
}

static int setup(walk_data *d)
{
	name_data *n;

	if (gitfo_mkdir(top_dir, 0755) < 0)
		return error("can't mkdir(\"%s\")", top_dir);

	if (gitfo_chdir(top_dir) < 0)
		return error("can't chdir(\"%s\")", top_dir);

	if (strcmp(d->sub, ".") != 0)
		if (gitfo_mkdir(d->sub, 0755) < 0)
			return error("can't mkdir(\"%s\")", d->sub);

	strcpy(path_buffer, d->sub);
	state_loc = d;

	for (n = d->names; n->name; n++) {
		git_file fd = gitfo_creat(n->name, 0600);
		must_be_true(fd >= 0);
		gitfo_close(fd);
		n->count = 0;
	}

	return 0;
}

static int knockdown(walk_data *d)
{
	name_data *n;

	for (n = d->names; n->name; n++) {
		if (gitfo_unlink(n->name) < 0)
			return error("can't unlink(\"%s\")", n->name);
	}

	if (strcmp(d->sub, ".") != 0)
		if (gitfo_rmdir(d->sub) < 0)
			return error("can't rmdir(\"%s\")", d->sub);

	if (gitfo_chdir("..") < 0)
		return error("can't chdir(\"..\")");

	if (gitfo_rmdir(top_dir) < 0)
		return error("can't rmdir(\"%s\")", top_dir);

	return 0;
}

static int check_counts(walk_data *d)
{
	int ret = 0;
	name_data *n;

	for (n = d->names; n->name; n++) {
		if (n->count != 1)
			ret = error("count (%d, %s)", n->count, n->name);
	}
	return ret;
}

static int one_entry(void *state, char *path)
{
	walk_data *d = (walk_data *) state;
	name_data *n;

	must_be_true(state == state_loc);
	must_be_true(path == path_buffer);
	for (n = d->names; n->name; n++) {
		if (!strcmp(n->name, path)) {
			n->count++;
			return 0;
		}
	}
	test_die("unexpected path \"%s\"", path);
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

BEGIN_TEST(dot)

	must_pass(setup(&dot));

	must_pass(gitfo_dirent(path_buffer,
			       sizeof(path_buffer),
			       one_entry,
			       &dot));

	must_pass(check_counts(&dot));

	must_pass(knockdown(&dot));
END_TEST

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

BEGIN_TEST(sub)

	must_pass(setup(&sub));

	must_pass(gitfo_dirent(path_buffer,
			       sizeof(path_buffer),
			       one_entry,
			       &sub));

	must_pass(check_counts(&sub));

	must_pass(knockdown(&sub));
END_TEST

static walk_data sub_slash = {
	"sub/",
	sub_names
};

BEGIN_TEST(sub_slash)

	must_pass(setup(&sub_slash));

	must_pass(gitfo_dirent(path_buffer,
			       sizeof(path_buffer),
			       one_entry,
			       &sub_slash));

	must_pass(check_counts(&sub_slash));

	must_pass(knockdown(&sub_slash));
END_TEST

static name_data empty_names[] = {
	{ 0, NULL }
};
static walk_data empty = {
	"empty",
	empty_names
};

static int dont_call_me(void *GIT_UNUSED(state), char *GIT_UNUSED(path))
{
	GIT_UNUSED_ARG(state)
	GIT_UNUSED_ARG(path)
	test_die("dont_call_me: unexpected callback!");
}

BEGIN_TEST(empty)

	must_pass(setup(&empty));

	must_pass(gitfo_dirent(path_buffer,
			       sizeof(path_buffer),
			       one_entry,
			       &empty));

	must_pass(check_counts(&empty));

	/* make sure callback not called */
	must_pass(gitfo_dirent(path_buffer,
			       sizeof(path_buffer),
			       dont_call_me,
			       &empty));

	must_pass(knockdown(&empty));
END_TEST

static name_data odd_names[] = {
	{ 0, "odd/.a" },
	{ 0, "odd/..c" },
	/* the following don't work on cygwin/win32 */
	/* { 0, "odd/.b." }, */
	/* { 0, "odd/..d.." },  */
	{ 0, NULL }
};
static walk_data odd = {
	"odd",
	odd_names
};

BEGIN_TEST(odd)

	must_pass(setup(&odd));

	must_pass(gitfo_dirent(path_buffer,
			       sizeof(path_buffer),
			       one_entry,
			       &odd));

	must_pass(check_counts(&odd));

	must_pass(knockdown(&odd));
END_TEST

