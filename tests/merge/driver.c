#include "clar_libgit2.h"
#include "git2/repository.h"
#include "git2/merge.h"
#include "buffer.h"
#include "merge.h"

#define TEST_REPO_PATH "merge-resolve"
#define BRANCH_ID "7cb63eed597130ba4abb87b3e544b85021905520"

#define AUTOMERGEABLE_IDSTR "f2e1550a0c9e53d5811175864a29536642ae3821"

static git_repository *repo;
static git_index *repo_index;
static git_oid automergeable_id;

static void test_drivers_register(void);
static void test_drivers_unregister(void);

void test_merge_driver__initialize(void)
{
    git_config *cfg;

    repo = cl_git_sandbox_init(TEST_REPO_PATH);
    git_repository_index(&repo_index, repo);

	git_oid_fromstr(&automergeable_id, AUTOMERGEABLE_IDSTR);

    /* Ensure that the user's merge.conflictstyle doesn't interfere */
    cl_git_pass(git_repository_config(&cfg, repo));

    cl_git_pass(git_config_set_string(cfg, "merge.conflictstyle", "merge"));
    cl_git_pass(git_config_set_bool(cfg, "core.autocrlf", false));

	test_drivers_register();

    git_config_free(cfg);
}

void test_merge_driver__cleanup(void)
{
	test_drivers_unregister();

    git_index_free(repo_index);
	cl_git_sandbox_cleanup();
}

struct test_merge_driver {
	git_merge_driver base;
	int initialized;
	int shutdown;
};

static int test_driver_init(git_merge_driver *s)
{
	struct test_merge_driver *self = (struct test_merge_driver *)s;
	self->initialized = 1;
	return 0;
}

static void test_driver_shutdown(git_merge_driver *s)
{
	struct test_merge_driver *self = (struct test_merge_driver *)s;
	self->shutdown = 1;
}

static int test_driver_check(
	git_merge_driver *s,
	void **payload,
	const char *name,
	const git_merge_driver_source *src)
{
	GIT_UNUSED(s);
	GIT_UNUSED(src);

	*payload = git__strdup(name);
	GITERR_CHECK_ALLOC(*payload);

	return 0;
}

static int test_driver_apply(
	git_merge_driver *s,
	void **payload,
	const char **path_out,
	uint32_t *mode_out,
	git_buf *merged_out,
	const git_merge_driver_source *src)
{
	GIT_UNUSED(s);
	GIT_UNUSED(src);

	*path_out = "applied.txt";
	*mode_out = GIT_FILEMODE_BLOB;

	return git_buf_printf(merged_out, "This is the `%s` driver.\n",
		(char *)*payload);
}

static void test_driver_cleanup(git_merge_driver *s, void *payload)
{
	GIT_UNUSED(s);

	git__free(payload);
}


static struct test_merge_driver test_driver_custom = {
	{
		GIT_MERGE_DRIVER_VERSION,
		test_driver_init,
		test_driver_shutdown,
		test_driver_check,
		test_driver_apply,
		test_driver_cleanup
	},
	0,
	0,
};

static struct test_merge_driver test_driver_wildcard = {
	{
		GIT_MERGE_DRIVER_VERSION,
		test_driver_init,
		test_driver_shutdown,
		test_driver_check,
		test_driver_apply,
		test_driver_cleanup
	},
	0,
	0,
};

static void test_drivers_register(void)
{
	cl_git_pass(git_merge_driver_register("custom", &test_driver_custom.base));
	cl_git_pass(git_merge_driver_register("*", &test_driver_wildcard.base));
}

static void test_drivers_unregister(void)
{
	cl_git_pass(git_merge_driver_unregister("custom"));
	cl_git_pass(git_merge_driver_unregister("*"));
}

static void set_gitattributes_to(const char *driver)
{
	git_buf line = GIT_BUF_INIT;

	cl_git_pass(git_buf_printf(&line, "automergeable.txt merge=%s\n", driver));
	cl_git_mkfile(TEST_REPO_PATH "/.gitattributes", line.ptr);
	git_buf_free(&line);
}

static void merge_branch(void)
{
	git_oid their_id;
	git_annotated_commit *their_head;

	cl_git_pass(git_oid_fromstr(&their_id, BRANCH_ID));
	cl_git_pass(git_annotated_commit_lookup(&their_head, repo, &their_id));

	cl_git_pass(git_merge(repo, (const git_annotated_commit **)&their_head,
		1, NULL, NULL));

	git_annotated_commit_free(their_head);
}

void test_merge_driver__custom(void)
{
	const char *expected = "This is the `custom` driver.\n";
	set_gitattributes_to("custom");
	merge_branch();

	cl_assert_equal_file(expected, strlen(expected),
		TEST_REPO_PATH "/applied.txt");
}

void test_merge_driver__wildcard(void)
{
	const char *expected = "This is the `foobar` driver.\n";
	set_gitattributes_to("foobar");
	merge_branch();

	cl_assert_equal_file(expected, strlen(expected),
		TEST_REPO_PATH "/applied.txt");
}

void test_merge_driver__shutdown_is_called(void)
{
    test_driver_custom.initialized = 0;
    test_driver_custom.shutdown = 0;
    test_driver_wildcard.initialized = 0;
    test_driver_wildcard.shutdown = 0;
    
    /* run the merge with the custom driver */
    set_gitattributes_to("custom");
    merge_branch();
    
	/* unregister the drivers, ensure their shutdown function is called */
	test_drivers_unregister();

    /* since the `custom` driver was used, it should have been initialized and
     * shutdown, but the wildcard driver was not used at all and should not
     * have been initialized or shutdown.
     */
	cl_assert(test_driver_custom.initialized);
	cl_assert(test_driver_custom.shutdown);
	cl_assert(!test_driver_wildcard.initialized);
	cl_assert(!test_driver_wildcard.shutdown);

	test_drivers_register();
}

static int defer_driver_check(
	git_merge_driver *s,
	void **payload,
	const char *name,
	const git_merge_driver_source *src)
{
	GIT_UNUSED(s);
	GIT_UNUSED(payload);
	GIT_UNUSED(name);
	GIT_UNUSED(src);

	return GIT_PASSTHROUGH;
}

static struct test_merge_driver test_driver_defer_check = {
	{
		GIT_MERGE_DRIVER_VERSION,
		test_driver_init,
		test_driver_shutdown,
		defer_driver_check,
		test_driver_apply,
		test_driver_cleanup
	},
	0,
	0,
};

void test_merge_driver__check_can_defer(void)
{
	const git_index_entry *idx;

	cl_git_pass(git_merge_driver_register("defer",
		&test_driver_defer_check.base));

    set_gitattributes_to("defer");
    merge_branch();

	cl_assert((idx = git_index_get_bypath(repo_index, "automergeable.txt", 0)));
	cl_assert_equal_oid(&automergeable_id, &idx->id);

	git_merge_driver_unregister("defer");
}

static int defer_driver_apply(
	git_merge_driver *s,
	void **payload,
	const char **path_out,
	uint32_t *mode_out,
	git_buf *merged_out,
	const git_merge_driver_source *src)
{
	GIT_UNUSED(s);
	GIT_UNUSED(payload);
	GIT_UNUSED(path_out);
	GIT_UNUSED(mode_out);
	GIT_UNUSED(merged_out);
	GIT_UNUSED(src);

	return GIT_PASSTHROUGH;
}

static struct test_merge_driver test_driver_defer_apply = {
	{
		GIT_MERGE_DRIVER_VERSION,
		test_driver_init,
		test_driver_shutdown,
		test_driver_check,
		defer_driver_apply,
		test_driver_cleanup
	},
	0,
	0,
};

void test_merge_driver__apply_can_defer(void)
{
	const git_index_entry *idx;

	cl_git_pass(git_merge_driver_register("defer",
		&test_driver_defer_apply.base));

    set_gitattributes_to("defer");
    merge_branch();

	cl_assert((idx = git_index_get_bypath(repo_index, "automergeable.txt", 0)));
	cl_assert_equal_oid(&automergeable_id, &idx->id);

	git_merge_driver_unregister("defer");
}

