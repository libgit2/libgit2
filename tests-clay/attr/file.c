#include "clay_libgit2.h"
#include "attr_file.h"

#define get_rule(X) ((git_attr_rule *)git_vector_get(&file->rules,(X)))
#define get_assign(R,Y) ((git_attr_assignment *)git_vector_get(&(R)->assigns,(Y)))

void test_attr_file__simple_read(void)
{
	git_attr_file *file = NULL;
	git_attr_assignment *assign;
	git_attr_rule *rule;

	cl_git_pass(git_attr_file__from_file(NULL, cl_fixture("attr/attr0"), &file));
	cl_assert_strequal(cl_fixture("attr/attr0"), file->path);
	cl_assert(file->rules.length == 1);

	rule = get_rule(0);
	cl_assert(rule != NULL);
	cl_assert_strequal("*", rule->match.pattern);
	cl_assert(rule->match.length == 1);
	cl_assert(rule->match.flags == 0);

	cl_assert(rule->assigns.length == 1);
	assign = get_assign(rule, 0);
	cl_assert(assign != NULL);
	cl_assert_strequal("binary", assign->name);
	cl_assert(assign->value == GIT_ATTR_TRUE);
	cl_assert(!assign->is_allocated);

	git_attr_file__free(file);
}

void test_attr_file__match_variants(void)
{
	git_attr_file *file = NULL;
	git_attr_rule *rule;
	git_attr_assignment *assign;

	cl_git_pass(git_attr_file__from_file(NULL, cl_fixture("attr/attr1"), &file));
	cl_assert_strequal(cl_fixture("attr/attr1"), file->path);
	cl_assert(file->rules.length == 10);

	/* let's do a thorough check of this rule, then just verify
	 * the things that are unique for the later rules
	 */
	rule = get_rule(0);
	cl_assert(rule);
	cl_assert_strequal("pat0", rule->match.pattern);
	cl_assert(rule->match.length == strlen("pat0"));
	cl_assert(rule->match.flags == 0);
	cl_assert(rule->assigns.length == 1);
	assign = get_assign(rule,0);
	cl_assert_strequal("attr0", assign->name);
	cl_assert(assign->name_hash == git_attr_file__name_hash(assign->name));
	cl_assert(assign->value == GIT_ATTR_TRUE);
	cl_assert(!assign->is_allocated);

	rule = get_rule(1);
	cl_assert_strequal("pat1", rule->match.pattern);
	cl_assert(rule->match.length == strlen("pat1"));
	cl_assert(rule->match.flags == GIT_ATTR_FNMATCH_NEGATIVE);

	rule = get_rule(2);
	cl_assert_strequal("pat2", rule->match.pattern);
	cl_assert(rule->match.length == strlen("pat2"));
	cl_assert(rule->match.flags == GIT_ATTR_FNMATCH_DIRECTORY);

	rule = get_rule(3);
	cl_assert_strequal("pat3dir/pat3file", rule->match.pattern);
	cl_assert(rule->match.flags == GIT_ATTR_FNMATCH_FULLPATH);

	rule = get_rule(4);
	cl_assert_strequal("pat4.*", rule->match.pattern);
	cl_assert(rule->match.flags == 0);

	rule = get_rule(5);
	cl_assert_strequal("*.pat5", rule->match.pattern);

	rule = get_rule(7);
	cl_assert_strequal("pat7[a-e]??[xyz]", rule->match.pattern);
	cl_assert(rule->assigns.length == 1);
	assign = get_assign(rule,0);
	cl_assert_strequal("attr7", assign->name);
	cl_assert(assign->value == GIT_ATTR_TRUE);

	rule = get_rule(8);
	cl_assert_strequal("pat8 with spaces", rule->match.pattern);
	cl_assert(rule->match.length == strlen("pat8 with spaces"));
	cl_assert(rule->match.flags == 0);

	rule = get_rule(9);
	cl_assert_strequal("pat9", rule->match.pattern);

	git_attr_file__free(file);
}

static void check_one_assign(
	git_attr_file *file,
	int rule_idx,
	int assign_idx,
	const char *pattern,
	const char *name,
	const char *value,
	int is_allocated)
{
	git_attr_rule *rule = get_rule(rule_idx);
	git_attr_assignment *assign = get_assign(rule, assign_idx);

	cl_assert_strequal(pattern, rule->match.pattern);
	cl_assert(rule->assigns.length == 1);
	cl_assert_strequal(name, assign->name);
	cl_assert(assign->name_hash == git_attr_file__name_hash(assign->name));
	cl_assert(assign->is_allocated == is_allocated);
	if (is_allocated)
		cl_assert_strequal(value, assign->value);
	else
		cl_assert(assign->value == value);
}

void test_attr_file__assign_variants(void)
{
	git_attr_file *file = NULL;
	git_attr_rule *rule;
	git_attr_assignment *assign;

	cl_git_pass(git_attr_file__from_file(NULL, cl_fixture("attr/attr2"), &file));
	cl_assert_strequal(cl_fixture("attr/attr2"), file->path);
	cl_assert(file->rules.length == 11);

	check_one_assign(file, 0, 0, "pat0", "simple", GIT_ATTR_TRUE, 0);
	check_one_assign(file, 1, 0, "pat1", "neg", GIT_ATTR_FALSE, 0);
	check_one_assign(file, 2, 0, "*", "notundef", GIT_ATTR_TRUE, 0);
	check_one_assign(file, 3, 0, "pat2", "notundef", NULL, 0);
	check_one_assign(file, 4, 0, "pat3", "assigned", "test-value", 1);
	check_one_assign(file, 5, 0, "pat4", "rule-with-more-chars", "value-with-more-chars", 1);
	check_one_assign(file, 6, 0, "pat5", "empty", GIT_ATTR_TRUE, 0);
	check_one_assign(file, 7, 0, "pat6", "negempty", GIT_ATTR_FALSE, 0);

	rule = get_rule(8);
	cl_assert_strequal("pat7", rule->match.pattern);
	cl_assert(rule->assigns.length == 5);
	/* assignments will be sorted by hash value, so we have to do
	 * lookups by search instead of by position
	 */
	assign = git_attr_rule__lookup_assignment(rule, "multiple");
	cl_assert(assign);
	cl_assert_strequal("multiple", assign->name);
	cl_assert(assign->value == GIT_ATTR_TRUE);
	assign = git_attr_rule__lookup_assignment(rule, "single");
	cl_assert(assign);
	cl_assert_strequal("single", assign->name);
	cl_assert(assign->value == GIT_ATTR_FALSE);
	assign = git_attr_rule__lookup_assignment(rule, "values");
	cl_assert(assign);
	cl_assert_strequal("values", assign->name);
	cl_assert_strequal("1", assign->value);
	assign = git_attr_rule__lookup_assignment(rule, "also");
	cl_assert(assign);
	cl_assert_strequal("also", assign->name);
	cl_assert_strequal("a-really-long-value/*", assign->value);
	assign = git_attr_rule__lookup_assignment(rule, "happy");
	cl_assert(assign);
	cl_assert_strequal("happy", assign->name);
	cl_assert_strequal("yes!", assign->value);
	assign = git_attr_rule__lookup_assignment(rule, "other");
	cl_assert(!assign);

	rule = get_rule(9);
	cl_assert_strequal("pat8", rule->match.pattern);
	cl_assert(rule->assigns.length == 2);
	assign = git_attr_rule__lookup_assignment(rule, "again");
	cl_assert(assign);
	cl_assert_strequal("again", assign->name);
	cl_assert(assign->value == GIT_ATTR_TRUE);
	assign = git_attr_rule__lookup_assignment(rule, "another");
	cl_assert(assign);
	cl_assert_strequal("another", assign->name);
	cl_assert_strequal("12321", assign->value);

	check_one_assign(file, 10, 0, "pat9", "at-eof", GIT_ATTR_FALSE, 0);

	git_attr_file__free(file);
}

void test_attr_file__check_attr_examples(void)
{
	git_attr_file *file = NULL;
	git_attr_rule *rule;
	git_attr_assignment *assign;

	cl_git_pass(git_attr_file__from_file(NULL, cl_fixture("attr/attr3"), &file));
	cl_assert_strequal(cl_fixture("attr/attr3"), file->path);
	cl_assert(file->rules.length == 3);

	rule = get_rule(0);
	cl_assert_strequal("*.java", rule->match.pattern);
	cl_assert(rule->assigns.length == 3);
	assign = git_attr_rule__lookup_assignment(rule, "diff");
	cl_assert_strequal("diff", assign->name);
	cl_assert_strequal("java", assign->value);
	assign = git_attr_rule__lookup_assignment(rule, "crlf");
	cl_assert_strequal("crlf", assign->name);
	cl_assert(GIT_ATTR_FALSE == assign->value);
	assign = git_attr_rule__lookup_assignment(rule, "myAttr");
	cl_assert_strequal("myAttr", assign->name);
	cl_assert(GIT_ATTR_TRUE == assign->value);
	assign = git_attr_rule__lookup_assignment(rule, "missing");
	cl_assert(assign == NULL);

	rule = get_rule(1);
	cl_assert_strequal("NoMyAttr.java", rule->match.pattern);
	cl_assert(rule->assigns.length == 1);
	assign = get_assign(rule, 0);
	cl_assert_strequal("myAttr", assign->name);
	cl_assert(assign->value == NULL);

	rule = get_rule(2);
	cl_assert_strequal("README", rule->match.pattern);
	cl_assert(rule->assigns.length == 1);
	assign = get_assign(rule, 0);
	cl_assert_strequal("caveat", assign->name);
	cl_assert_strequal("unspecified", assign->value);

	git_attr_file__free(file);
}
