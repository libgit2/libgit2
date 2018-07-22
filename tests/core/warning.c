#include "clar_libgit2.h"
#include "warning.h"

void test_core_warning__initialize(void)
{

}

void test_core_warning__cleanup(void)
{

}

static int test_dummy_cb(const git_warning *warn, void *payload)
{
	GIT_UNUSED(warn);
	GIT_UNUSED(payload);

	return GIT_PASSTHROUGH;
}

void test_core_warning__registration(void)
{
	git_warning_token token1, token2;

	cl_assert_equal_i(0, git_vector_length(&git_warning__registration));

	cl_git_pass(git_warning_register(&token1, GIT_WARNING_ANY, test_dummy_cb, NULL));
	cl_assert_equal_i(1, git_vector_length(&git_warning__registration));

	cl_git_pass(git_warning_register(&token2, GIT_WARNING_CLASS(2, 3), test_dummy_cb, NULL));
	cl_assert_equal_i(2, git_vector_length(&git_warning__registration));

	cl_git_pass(git_warning_unregister(&token1));
	cl_assert_equal_i(1, git_vector_length(&git_warning__registration));

	cl_git_fail_with(GIT_ENOTFOUND, git_warning_unregister(&token1));
	cl_assert_equal_i(1, git_vector_length(&git_warning__registration));

	cl_git_pass(git_warning_unregister(&token2));
	cl_assert_equal_i(0, git_vector_length(&git_warning__registration));
}

struct warning_cb_test {
	int called;
	int reply;
};

int test_warning_cb(const git_warning *warning, void *payload)
{
	struct warning_cb_test *data = payload;

	GIT_UNUSED(warning);

	data->called = 1;

	return data->reply;
}

void test_core_warning__raising(void)
{
	git_warning_token any_token, specific_token, replying_token;
	struct warning_cb_test any_data = {.called = 0, .reply = GIT_PASSTHROUGH};
	struct warning_cb_test specific_data = {.called = 0, .reply = GIT_PASSTHROUGH};
	struct warning_cb_test replying_data = {.called = 0, .reply = 0};
	int reply;

	cl_git_pass(git_warning_register(&any_token, GIT_WARNING_ANY, test_warning_cb, &any_data));
	cl_git_pass(git_warning_register(&specific_token, GIT_WARNING_CLASS(1, 1), test_warning_cb, &specific_data));
	cl_git_pass(git_warning_register(&replying_token, GIT_WARNING_CLASS(1, 2), test_warning_cb, &replying_data));

	reply = git_warn__raise(GIT_WARNING_CLASS(2, 1), NULL, NULL, "warning 1 from subsystem 2");
	cl_assert_equal_i(GIT_PASSTHROUGH, reply);
	cl_assert_equal_i(1, any_data.called);
	cl_assert_equal_i(0, specific_data.called);
	cl_assert_equal_i(0, replying_data.called);
	any_data.called = specific_data.called = replying_data.called = 0;

	reply = git_warn__raise(GIT_WARNING_CLASS(1, 1), NULL, NULL, "warning 1 from subsystem 1");
	cl_assert_equal_i(GIT_PASSTHROUGH, reply);
	cl_assert_equal_i(1, any_data.called);
	cl_assert_equal_i(1, specific_data.called);
	cl_assert_equal_i(0, replying_data.called);
	any_data.called = specific_data.called = replying_data.called = 0;

	reply = git_warn__raise(GIT_WARNING_CLASS(1, 2), NULL, NULL, "warning 2 from subsystem 1");
	cl_assert_equal_i(replying_data.reply, reply);
	cl_assert_equal_i(0, any_data.called);
	cl_assert_equal_i(1, specific_data.called);
	cl_assert_equal_i(0, replying_data.called);
	any_data.called = specific_data.called = replying_data.called = 0;

	cl_git_pass(git_warning_unregister(&any_token));
	cl_git_pass(git_warning_unregister(&specific_token));
	cl_git_pass(git_warning_unregister(&replying_token));

	reply = git_warn__raise(GIT_WARNING_CLASS(1, 1), NULL, NULL, "warning from subsystem 1");
	cl_assert_equal_i(GIT_PASSTHROUGH, reply);
	cl_assert_equal_i(0, any_data.called);
	cl_assert_equal_i(0, specific_data.called);
	cl_assert_equal_i(0, replying_data.called);
	any_data.called = specific_data.called = replying_data.called = 0;
}
