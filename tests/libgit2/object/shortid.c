#include "clar_libgit2.h"

git_repository *_repo;

void test_object_shortid__initialize(void)
{
	_repo = cl_git_sandbox_init("duplicate.git");
}

void test_object_shortid__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_object_shortid__select(void)
{
	git_oid full;
	git_object *obj;
	git_buf shorty = {0};

	git_oid_from_string(&full, "ce013625030ba8dba906f756967f9e9ca394464a", GIT_OID_SHA1);
	cl_git_pass(git_object_lookup(&obj, _repo, &full, GIT_OBJECT_ANY));
	cl_git_pass(git_object_short_id(&shorty, obj));
	cl_assert_equal_i(7, shorty.size);
	cl_assert_equal_s("ce01362", shorty.ptr);
	git_object_free(obj);

	git_oid_from_string(&full, "038d718da6a1ebbc6a7780a96ed75a70cc2ad6e2", GIT_OID_SHA1);
	cl_git_pass(git_object_lookup(&obj, _repo, &full, GIT_OBJECT_ANY));
	cl_git_pass(git_object_short_id(&shorty, obj));
	cl_assert_equal_i(7, shorty.size);
	cl_assert_equal_s("038d718", shorty.ptr);
	git_object_free(obj);

	git_oid_from_string(&full, "dea509d097ce692e167dfc6a48a7a280cc5e877e", GIT_OID_SHA1);
	cl_git_pass(git_object_lookup(&obj, _repo, &full, GIT_OBJECT_ANY));
	cl_git_pass(git_object_short_id(&shorty, obj));
	cl_assert_equal_i(9, shorty.size);
	cl_assert_equal_s("dea509d09", shorty.ptr);
	git_object_free(obj);

	git_oid_from_string(&full, "dea509d0b3cb8ee0650f6ca210bc83f4678851ba", GIT_OID_SHA1);
	cl_git_pass(git_object_lookup(&obj, _repo, &full, GIT_OBJECT_ANY));
	cl_git_pass(git_object_short_id(&shorty, obj));
	cl_assert_equal_i(9, shorty.size);
	cl_assert_equal_s("dea509d0b", shorty.ptr);
	git_object_free(obj);

	git_buf_dispose(&shorty);
}

void test_object_shortid__core_abbrev(void)
{
	git_oid full;
	git_object *obj;
	git_buf shorty = {0};
	git_config *cfg;

	cl_git_pass(git_repository_config(&cfg, _repo));
	git_oid_from_string(&full, "ce013625030ba8dba906f756967f9e9ca394464a", GIT_OID_SHA1);
	cl_git_pass(git_object_lookup(&obj, _repo, &full, GIT_OBJECT_ANY));

	cl_git_pass(git_config_set_string(cfg, "core.abbrev", "auto"));
	cl_git_pass(git_object_short_id(&shorty, obj));
	cl_assert_equal_i(7, shorty.size);
	cl_assert_equal_s("ce01362", shorty.ptr);

	cl_git_pass(git_config_set_string(cfg, "core.abbrev", "off"));
	cl_git_pass(git_object_short_id(&shorty, obj));
	cl_assert_equal_i(40, shorty.size);
	cl_assert_equal_s("ce013625030ba8dba906f756967f9e9ca394464a", shorty.ptr);

	cl_git_pass(git_config_set_string(cfg, "core.abbrev", "false"));
	cl_git_pass(git_object_short_id(&shorty, obj));
	cl_assert_equal_i(40, shorty.size);
	cl_assert_equal_s("ce013625030ba8dba906f756967f9e9ca394464a", shorty.ptr);

	cl_git_pass(git_config_set_string(cfg, "core.abbrev", "99"));
	cl_git_pass(git_object_short_id(&shorty, obj));
	cl_assert_equal_i(40, shorty.size);
	cl_assert_equal_s("ce013625030ba8dba906f756967f9e9ca394464a", shorty.ptr);

	cl_git_pass(git_config_set_string(cfg, "core.abbrev", "4"));
	cl_git_pass(git_object_short_id(&shorty, obj));
	cl_assert_equal_i(4, shorty.size);
	cl_assert_equal_s("ce01", shorty.ptr);

	cl_git_pass(git_config_set_string(cfg, "core.abbrev", "0"));
	cl_git_fail(git_object_short_id(&shorty, obj));
	cl_git_pass(git_config_set_string(cfg, "core.abbrev", "3"));
	cl_git_fail(git_object_short_id(&shorty, obj));
	cl_git_pass(git_config_set_string(cfg, "core.abbrev", "invalid"));
	cl_git_fail(git_object_short_id(&shorty, obj));
	cl_git_pass(git_config_set_string(cfg, "core.abbrev", "true"));
	cl_git_fail(git_object_short_id(&shorty, obj));

	git_object_free(obj);
	git_buf_dispose(&shorty);
	git_config_free(cfg);
}
