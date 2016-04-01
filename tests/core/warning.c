#include "clar_libgit2.h"
#include "warning.h"

static git_warning g_warning = { GIT_WARNING_GENERIC, "Not really an error" };
static int g_dummy_payload;

void test_core_warning__cleanup(void)
{
	git_warning_set_callback(NULL, NULL);
}

void test_core_warning__zero_on_unset(void)
{
	cl_git_pass(git_warning__raise(&g_warning));
}

static int values_callback(git_warning *warning, void *payload)
{
	cl_assert_equal_p(&g_warning, warning);
	cl_assert_equal_p(&g_dummy_payload, payload);

	return 0;
}

void test_core_warning__raises_values(void)
{
	cl_git_pass(git_warning_set_callback(values_callback, &g_dummy_payload));
	cl_git_pass(git_warning__raise(&g_warning));
}

static int should_be_called;
static int can_unset_callback(git_warning *warning, void *payload)
{
	GIT_UNUSED(warning); GIT_UNUSED(payload);

	cl_assert(should_be_called);

	return 0;
}

void test_core_warning__can_unset(void)
{
	should_be_called = 1;
	cl_git_pass(git_warning_set_callback(can_unset_callback, &g_dummy_payload));
	cl_git_pass(git_warning__raise(&g_warning));
	should_be_called = 0;
	cl_git_pass(git_warning_set_callback(NULL, NULL));
	cl_git_pass(git_warning__raise(&g_warning));
}
