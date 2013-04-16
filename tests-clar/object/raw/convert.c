
#include "clar_libgit2.h"

#include "odb.h"

void test_object_raw_convert__succeed_on_oid_to_string_conversion(void)
{
	const char *exp = "16a0123456789abcdef4b775213c23a8bd74f5e0";
	git_oid in;
	char out[GIT_OID_HEXSZ + 1];
	char *str;
	int i;

	cl_git_pass(git_oid_fromstr(&in, exp));

	/* NULL buffer pointer, returns static empty string */
	str = git_oid_tostr(NULL, sizeof(out), &in);
	cl_assert(str && *str == '\0' && str != out);

	/* zero buffer size, returns static empty string */
	str = git_oid_tostr(out, 0, &in);
	cl_assert(str && *str == '\0' && str != out);

	/* NULL oid pointer, sets existing buffer to empty string */
	str = git_oid_tostr(out, sizeof(out), NULL);
	cl_assert(str && *str == '\0' && str == out);

	/* n == 1, returns out as an empty string */
	str = git_oid_tostr(out, 1, &in);
	cl_assert(str && *str == '\0' && str == out);

	for (i = 1; i < GIT_OID_HEXSZ; i++) {
		out[i+1] = 'Z';
		str = git_oid_tostr(out, i+1, &in);
		/* returns out containing c-string */
		cl_assert(str && str == out);
		/* must be '\0' terminated */
		cl_assert(*(str+i) == '\0');
		/* must not touch bytes past end of string */
		cl_assert(*(str+(i+1)) == 'Z');
		/* i == n-1 charaters of string */
		cl_git_pass(strncmp(exp, out, i));
	}

	/* returns out as hex formatted c-string */
	str = git_oid_tostr(out, sizeof(out), &in);
	cl_assert(str && str == out && *(str+GIT_OID_HEXSZ) == '\0');
	cl_assert_equal_s(exp, out);
}

void test_object_raw_convert__succeed_on_oid_to_string_conversion_big(void)
{
	const char *exp = "16a0123456789abcdef4b775213c23a8bd74f5e0";
	git_oid in;
	char big[GIT_OID_HEXSZ + 1 + 3]; /* note + 4 => big buffer */
	char *str;

	cl_git_pass(git_oid_fromstr(&in, exp));

	/* place some tail material */
	big[GIT_OID_HEXSZ+0] = 'W'; /* should be '\0' afterwards */
	big[GIT_OID_HEXSZ+1] = 'X'; /* should remain untouched   */
	big[GIT_OID_HEXSZ+2] = 'Y'; /* ditto */
	big[GIT_OID_HEXSZ+3] = 'Z'; /* ditto */

	/* returns big as hex formatted c-string */
	str = git_oid_tostr(big, sizeof(big), &in);
	cl_assert(str && str == big && *(str+GIT_OID_HEXSZ) == '\0');
	cl_assert_equal_s(exp, big);

	/* check tail material is untouched */
	cl_assert(str && str == big && *(str+GIT_OID_HEXSZ+1) == 'X');
	cl_assert(str && str == big && *(str+GIT_OID_HEXSZ+2) == 'Y');
	cl_assert(str && str == big && *(str+GIT_OID_HEXSZ+3) == 'Z');
}
