#include "clar_libgit2.h"
#include "attr_file.h"

void test_attr_lookup__simple(void)
{
	git_attr_file *file;
	git_attr_path path;
	const char *value = NULL;

	cl_git_pass(git_attr_file__new(&file));
	cl_git_pass(git_attr_file__from_file(NULL, cl_fixture("attr/attr0"), file));
	cl_assert_strequal(cl_fixture("attr/attr0"), file->path);
	cl_assert(file->rules.length == 1);

	cl_git_pass(git_attr_path__init(&path, "test", NULL));
	cl_assert_strequal("test", path.path);
	cl_assert_strequal("test", path.basename);
	cl_assert(!path.is_dir);

	cl_git_pass(git_attr_file__lookup_one(file,&path,"binary",&value));
	cl_assert(value == GIT_ATTR_TRUE);

	cl_git_pass(git_attr_file__lookup_one(file,&path,"missing",&value));
	cl_assert(!value);

	git_attr_file__free(file);
}

typedef struct {
	const char *path;
	const char *attr;
	const char *expected;
	int use_strcmp;
	int force_dir;
} test_case;

static void run_test_cases(git_attr_file *file, test_case *cases)
{
	git_attr_path path;
	const char *value = NULL;
	test_case *c;
	int error;

	for (c = cases; c->path != NULL; c++) {
		cl_git_pass(git_attr_path__init(&path, c->path, NULL));

		if (c->force_dir)
			path.is_dir = 1;

		error = git_attr_file__lookup_one(file,&path,c->attr,&value);
		if (error != GIT_SUCCESS)
			fprintf(stderr, "failure with %s %s %s\n", c->path, c->attr, c->expected);
		cl_git_pass(error);

		if (c->use_strcmp)
			cl_assert_strequal(c->expected, value);
		else
			cl_assert(c->expected == value);
	}
}

void test_attr_lookup__match_variants(void)
{
	git_attr_file *file;
	git_attr_path path;
	test_case cases[] = {
		/* pat0 -> simple match */
		{ "pat0", "attr0", GIT_ATTR_TRUE, 0, 0 },
		{ "/testing/for/pat0", "attr0", GIT_ATTR_TRUE, 0, 0 },
		{ "relative/to/pat0", "attr0", GIT_ATTR_TRUE, 0, 0 },
		{ "this-contains-pat0-inside", "attr0", NULL, 0, 0 },
		{ "this-aint-right", "attr0", NULL, 0, 0 },
		{ "/this/pat0/dont/match", "attr0", NULL, 0, 0 },
		/* negative match */
		{ "pat0", "attr1", GIT_ATTR_TRUE, 0, 0 },
		{ "pat1", "attr1", NULL, 0, 0 },
		{ "/testing/for/pat1", "attr1", NULL, 0, 0 },
		{ "/testing/for/pat0", "attr1", GIT_ATTR_TRUE, 0, 0 },
		{ "/testing/for/pat1/inside", "attr1", GIT_ATTR_TRUE, 0, 0 },
		{ "misc", "attr1", GIT_ATTR_TRUE, 0, 0 },
		/* dir match */
		{ "pat2", "attr2", NULL, 0, 0 },
		{ "pat2", "attr2", GIT_ATTR_TRUE, 0, 1 },
		{ "/testing/for/pat2", "attr2", NULL, 0, 0 },
		{ "/testing/for/pat2", "attr2", GIT_ATTR_TRUE, 0, 1 },
		{ "/not/pat2/yousee", "attr2", NULL, 0, 0 },
		{ "/not/pat2/yousee", "attr2", NULL, 0, 1 },
		/* path match */
		{ "pat3file", "attr3", NULL, 0, 0 },
		{ "/pat3dir/pat3file", "attr3", NULL, 0, 0 },
		{ "pat3dir/pat3file", "attr3", GIT_ATTR_TRUE, 0, 0 },
		/* pattern* match */
		{ "pat4.txt", "attr4", GIT_ATTR_TRUE, 0, 0 },
		{ "/fun/fun/fun/pat4.c", "attr4", GIT_ATTR_TRUE, 0, 0 },
		{ "pat4.", "attr4", GIT_ATTR_TRUE, 0, 0 },
		{ "pat4", "attr4", NULL, 0, 0 },
		{ "/fun/fun/fun/pat4.dir", "attr4", GIT_ATTR_TRUE, 0, 1 },
		/* *pattern match */
		{ "foo.pat5", "attr5", GIT_ATTR_TRUE, 0, 0 },
		{ "foo.pat5", "attr5", GIT_ATTR_TRUE, 0, 1 },
		{ "/this/is/ok.pat5", "attr5", GIT_ATTR_TRUE, 0, 0 },
		{ "/this/is/bad.pat5/yousee.txt", "attr5", NULL, 0, 0 },
		{ "foo.pat5", "attr100", NULL, 0, 0 },
		/* glob match with slashes */
		{ "foo.pat6", "attr6", NULL, 0, 0 },
		{ "pat6/pat6/foobar.pat6", "attr6", GIT_ATTR_TRUE, 0, 0 },
		{ "pat6/pat6/.pat6", "attr6", GIT_ATTR_TRUE, 0, 0 },
		{ "pat6/pat6/extra/foobar.pat6", "attr6", NULL, 0, 0 },
		{ "/prefix/pat6/pat6/foobar.pat6", "attr6", NULL, 0, 0 },
		{ "/pat6/pat6/foobar.pat6", "attr6", NULL, 0, 0 },
		/* complex pattern */
		{ "pat7a12z", "attr7", GIT_ATTR_TRUE, 0, 0 },
		{ "pat7e__x", "attr7", GIT_ATTR_TRUE, 0, 0 },
		{ "pat7b/1y", "attr7", NULL, 0, 0 }, /* ? does not match / */
		{ "pat7e_x", "attr7", NULL, 0, 0 },
		{ "pat7aaaa", "attr7", NULL, 0, 0 },
		{ "pat7zzzz", "attr7", NULL, 0, 0 },
		{ "/this/can/be/anything/pat7a12z", "attr7", GIT_ATTR_TRUE, 0, 0 },
		{ "but/it/still/must/match/pat7aaaa", "attr7", NULL, 0, 0 },
		{ "pat7aaay.fail", "attr7", NULL, 0, 0 },
		/* pattern with spaces */
		{ "pat8 with spaces", "attr8", GIT_ATTR_TRUE, 0, 0 },
		{ "/gotta love/pat8 with spaces", "attr8", GIT_ATTR_TRUE, 0, 0 },
		{ "failing pat8 with spaces", "attr8", NULL, 0, 0 },
		{ "spaces", "attr8", NULL, 0, 0 },
		/* pattern at eof */
		{ "pat9", "attr9", GIT_ATTR_TRUE, 0, 0 },
		{ "/eof/pat9", "attr9", GIT_ATTR_TRUE, 0, 0 },
		{ "pat", "attr9", NULL, 0, 0 },
		{ "at9", "attr9", NULL, 0, 0 },
		{ "pat9.fail", "attr9", NULL, 0, 0 },
		/* sentinel at end */
		{ NULL, NULL, NULL, 0, 0 }
	};

	cl_git_pass(git_attr_file__new(&file));
	cl_git_pass(git_attr_file__from_file(NULL, cl_fixture("attr/attr1"), file));
	cl_assert_strequal(cl_fixture("attr/attr1"), file->path);
	cl_assert(file->rules.length == 10);

	cl_git_pass(git_attr_path__init(&path, "/testing/for/pat0", NULL));
	cl_assert_strequal("pat0", path.basename);

	run_test_cases(file, cases);

	git_attr_file__free(file);
}

void test_attr_lookup__assign_variants(void)
{
	git_attr_file *file;
	test_case cases[] = {
		/* pat0 -> simple assign */
		{ "pat0", "simple", GIT_ATTR_TRUE, 0, 0 },
		{ "/testing/pat0", "simple", GIT_ATTR_TRUE, 0, 0 },
		{ "pat0", "fail", NULL, 0, 0 },
		{ "/testing/pat0", "fail", NULL, 0, 0 },
		/* negative assign */
		{ "pat1", "neg", GIT_ATTR_FALSE, 0, 0 },
		{ "/testing/pat1", "neg", GIT_ATTR_FALSE, 0, 0 },
		{ "pat1", "fail", NULL, 0, 0 },
		{ "/testing/pat1", "fail", NULL, 0, 0 },
		/* forced undef */
		{ "pat1", "notundef", GIT_ATTR_TRUE, 0, 0 },
		{ "pat2", "notundef", NULL, 0, 0 },
		{ "/lead/in/pat1", "notundef", GIT_ATTR_TRUE, 0, 0 },
		{ "/lead/in/pat2", "notundef", NULL, 0, 0 },
		/* assign value */
		{ "pat3", "assigned", "test-value", 1, 0 },
		{ "pat3", "notassigned", NULL, 0, 0 },
		/* assign value */
		{ "pat4", "rule-with-more-chars", "value-with-more-chars", 1, 0 },
		{ "pat4", "notassigned-rule-with-more-chars", NULL, 0, 0 },
		/* empty assignments */
		{ "pat5", "empty", GIT_ATTR_TRUE, 0, 0 },
		{ "pat6", "negempty", GIT_ATTR_FALSE, 0, 0 },
		/* multiple assignment */
		{ "pat7", "multiple", GIT_ATTR_TRUE, 0, 0 },
		{ "pat7", "single", GIT_ATTR_FALSE, 0, 0 },
		{ "pat7", "values", "1", 1, 0 },
		{ "pat7", "also", "a-really-long-value/*", 1, 0 },
		{ "pat7", "happy", "yes!", 1, 0 },
		{ "pat8", "again", GIT_ATTR_TRUE, 0, 0 },
		{ "pat8", "another", "12321", 1, 0 },
		/* bad assignment */
		{ "patbad0", "simple", NULL, 0, 0 },
		{ "patbad0", "notundef", GIT_ATTR_TRUE, 0, 0 },
		{ "patbad1", "simple", NULL, 0, 0 },
		/* eof assignment */
		{ "pat9", "at-eof", GIT_ATTR_FALSE, 0, 0 },
		/* sentinel at end */
		{ NULL, NULL, NULL, 0, 0 }
	};

	cl_git_pass(git_attr_file__new(&file));
	cl_git_pass(git_attr_file__from_file(NULL, cl_fixture("attr/attr2"), file));
	cl_assert(file->rules.length == 11);

	run_test_cases(file, cases);

	git_attr_file__free(file);
}

void test_attr_lookup__check_attr_examples(void)
{
	git_attr_file *file;
	test_case cases[] = {
		{ "foo.java", "diff", "java", 1, 0 },
		{ "foo.java", "crlf", GIT_ATTR_FALSE, 0, 0 },
		{ "foo.java", "myAttr", GIT_ATTR_TRUE, 0, 0 },
		{ "foo.java", "other", NULL, 0, 0 },
		{ "/prefix/dir/foo.java", "diff", "java", 1, 0 },
		{ "/prefix/dir/foo.java", "crlf", GIT_ATTR_FALSE, 0, 0 },
		{ "/prefix/dir/foo.java", "myAttr", GIT_ATTR_TRUE, 0, 0 },
		{ "/prefix/dir/foo.java", "other", NULL, 0, 0 },
		{ "NoMyAttr.java", "crlf", GIT_ATTR_FALSE, 0, 0 },
		{ "NoMyAttr.java", "myAttr", NULL, 0, 0 },
		{ "NoMyAttr.java", "other", NULL, 0, 0 },
		{ "/prefix/dir/NoMyAttr.java", "crlf", GIT_ATTR_FALSE, 0, 0 },
		{ "/prefix/dir/NoMyAttr.java", "myAttr", NULL, 0, 0 },
		{ "/prefix/dir/NoMyAttr.java", "other", NULL, 0, 0 },
		{ "README", "caveat", "unspecified", 1, 0 },
		{ "/specific/path/README", "caveat", "unspecified", 1, 0 },
		{ "README", "missing", NULL, 0, 0 },
		{ "/specific/path/README", "missing", NULL, 0, 0 },
		/* sentinel at end */
		{ NULL, NULL, NULL, 0, 0 }
	};

	cl_git_pass(git_attr_file__new(&file));
	cl_git_pass(git_attr_file__from_file(NULL, cl_fixture("attr/attr3"), file));
	cl_assert(file->rules.length == 3);

	run_test_cases(file, cases);

	git_attr_file__free(file);
}

void test_attr_lookup__from_buffer(void)
{
	git_attr_file *file;
	test_case cases[] = {
		{ "abc", "foo", GIT_ATTR_TRUE, 0, 0 },
		{ "abc", "bar", GIT_ATTR_TRUE, 0, 0 },
		{ "abc", "baz", GIT_ATTR_TRUE, 0, 0 },
		{ "aaa", "foo", GIT_ATTR_TRUE, 0, 0 },
		{ "aaa", "bar", NULL, 0, 0 },
		{ "aaa", "baz", GIT_ATTR_TRUE, 0, 0 },
		{ "qqq", "foo", NULL, 0, 0 },
		{ "qqq", "bar", NULL, 0, 0 },
		{ "qqq", "baz", GIT_ATTR_TRUE, 0, 0 },
		{ NULL, NULL, NULL, 0, 0 }
	};

	cl_git_pass(git_attr_file__new(&file));
	cl_git_pass(git_attr_file__from_buffer(NULL, "a* foo\nabc bar\n* baz", file));
	cl_assert(file->rules.length == 3);

	run_test_cases(file, cases);

	git_attr_file__free(file);
}
