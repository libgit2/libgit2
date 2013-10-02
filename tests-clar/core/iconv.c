#include "clar_libgit2.h"
#include "path.h"

static git_path_iconv_t ic;
static char *nfc = "\xC3\x85\x73\x74\x72\xC3\xB6\x6D";
static char *nfd = "\x41\xCC\x8A\x73\x74\x72\x6F\xCC\x88\x6D";

void test_core_iconv__initialize(void)
{
	cl_git_pass(git_path_iconv_init_precompose(&ic));
}

void test_core_iconv__cleanup(void)
{
	git_path_iconv_clear(&ic);
}

void test_core_iconv__unchanged(void)
{
	char *data = "Ascii data", *original = data;
	size_t datalen = strlen(data);

	cl_git_pass(git_path_iconv(&ic, &data, &datalen));

	/* There are no high bits set, so this should leave data untouched */
	cl_assert(data == original);
}

void test_core_iconv__decomposed_to_precomposed(void)
{
	char *data = nfd;
	size_t datalen = strlen(nfd);

	cl_git_pass(git_path_iconv(&ic, &data, &datalen));

	/* The decomposed nfd string should be transformed to the nfc form
	 * (on platforms where iconv is enabled, of course).
	 */
#ifdef GIT_USE_ICONV
	cl_assert_equal_s(nfc, data);
#else
	cl_assert_equal_s(nfd, data);
#endif
}

void test_core_iconv__precomposed_is_unmodified(void)
{
	char *data = nfc;
	size_t datalen = strlen(nfc);

	cl_git_pass(git_path_iconv(&ic, &data, &datalen));

	/* data is already in precomposed form, so even though some bytes have
	 * the high-bit set, the iconv transform should result in no change.
	 */
	cl_assert_equal_s(nfc, data);
}
