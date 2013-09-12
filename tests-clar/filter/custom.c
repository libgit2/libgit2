#include "clar_libgit2.h"
#include "posix.h"
#include "blob.h"
#include "filter.h"
#include "buf_text.h"
#include "git2/sys/filter.h"
#include "git2/sys/repository.h"

#define BITFLIP_FILTER_PRIORITY 20
#define REVERSE_FILTER_PRIORITY 25

#define VERY_SECURE_ENCRYPTION(b) ((b) ^ 0xff)

#ifdef GIT_WIN32
# define NEWLINE "\r\n"
#else
# define NEWLINE "\n"
#endif

static char workdir_data[] =
	"some simple" NEWLINE
	"data" NEWLINE
	"that will be" NEWLINE
	"trivially" NEWLINE
	"scrambled." NEWLINE;

/* Represents the data above scrambled (bits flipped) after \r\n -> \n
 * conversion, then bytewise reversed
 */
static unsigned char bitflipped_and_reversed_data[] =
	{ 0xf5, 0xd1, 0x9b, 0x9a, 0x93, 0x9d, 0x92, 0x9e, 0x8d, 0x9c, 0x8c,
	  0xf5, 0x86, 0x93, 0x93, 0x9e, 0x96, 0x89, 0x96, 0x8d, 0x8b, 0xf5,
	  0x9a, 0x9d, 0xdf, 0x93, 0x93, 0x96, 0x88, 0xdf, 0x8b, 0x9e, 0x97,
	  0x8b, 0xf5, 0x9e, 0x8b, 0x9e, 0x9b, 0xf5, 0x9a, 0x93, 0x8f, 0x92,
	  0x96, 0x8c, 0xdf, 0x9a, 0x92, 0x90, 0x8c };

#define BITFLIPPED_AND_REVERSED_DATA_LEN 51

static git_repository *g_repo = NULL;

static void register_custom_filters(void);

void test_filter_custom__initialize(void)
{
	register_custom_filters();

	g_repo = cl_git_sandbox_init("empty_standard_repo");

	cl_git_mkfile(
		"empty_standard_repo/.gitattributes",
		"hero* bitflip reverse\n"
		"herofile text\n"
		"heroflip -reverse\n");
}

void test_filter_custom__cleanup(void)
{
	cl_git_sandbox_cleanup();
	g_repo = NULL;
}

static int bitflip_filter_apply(
	git_filter     *self,
	void          **payload,
	git_buf        *to,
	const git_buf  *from,
	const git_filter_source *source)
{
	const unsigned char *src = (const unsigned char *)from->ptr;
	unsigned char *dst;
	size_t i;

	GIT_UNUSED(self); GIT_UNUSED(payload);

	/* verify that attribute path match worked as expected */
	cl_assert_equal_i(
		0, git__strncmp("hero", git_filter_source_path(source), 4));

	if (!from->size)
		return 0;

	cl_git_pass(git_buf_grow(to, from->size));

	dst = (unsigned char *)to->ptr;

	for (i = 0; i < from->size; i++)
		dst[i] = VERY_SECURE_ENCRYPTION(src[i]);

	to->size = from->size;

	return 0;
}

static void bitflip_filter_free(git_filter *f)
{
	git__free(f);
}

static git_filter *create_bitflip_filter(void)
{
	git_filter *filter = git__calloc(1, sizeof(git_filter));
	cl_assert(filter);

	filter->version = GIT_FILTER_VERSION;
	filter->attributes = "+bitflip";
	filter->shutdown = bitflip_filter_free;
	filter->apply = bitflip_filter_apply;

	return filter;
}


static int reverse_filter_apply(
	git_filter     *self,
	void          **payload,
	git_buf        *to,
	const git_buf  *from,
	const git_filter_source *source)
{
	const unsigned char *src = (const unsigned char *)from->ptr;
	const unsigned char *end = src + from->size;
	unsigned char *dst;

	GIT_UNUSED(self); GIT_UNUSED(payload); GIT_UNUSED(source);

	/* verify that attribute path match worked as expected */
	cl_assert_equal_i(
		0, git__strncmp("hero", git_filter_source_path(source), 4));

	if (!from->size)
		return 0;

	cl_git_pass(git_buf_grow(to, from->size));

	dst = (unsigned char *)to->ptr + from->size - 1;

	while (src < end)
		*dst-- = *src++;

	to->size = from->size;

	return 0;
}

static void reverse_filter_free(git_filter *f)
{
	git__free(f);
}

static git_filter *create_reverse_filter(void)
{
	git_filter *filter = git__calloc(1, sizeof(git_filter));
	cl_assert(filter);

	filter->version = GIT_FILTER_VERSION;
	filter->attributes = "+reverse";
	filter->shutdown = reverse_filter_free;
	filter->apply = reverse_filter_apply;

	return filter;
}

static void register_custom_filters(void)
{
	static int filters_registered = 0;

	if (!filters_registered) {
		cl_git_pass(git_filter_register(
			"bitflip", create_bitflip_filter(), BITFLIP_FILTER_PRIORITY));

		cl_git_pass(git_filter_register(
			"reverse", create_reverse_filter(), REVERSE_FILTER_PRIORITY));

		filters_registered = 1;
	}
}


void test_filter_custom__to_odb(void)
{
	git_filter_list *fl;
	git_buf out = { 0 };
	git_buf in = GIT_BUF_INIT_CONST(workdir_data, strlen(workdir_data));

	cl_git_pass(git_filter_list_load(
		&fl, g_repo, NULL, "herofile", GIT_FILTER_TO_ODB));

	cl_git_pass(git_filter_list_apply_to_data(&out, fl, &in));

	cl_assert_equal_i(BITFLIPPED_AND_REVERSED_DATA_LEN, out.size);

	cl_assert_equal_i(
		0, memcmp(bitflipped_and_reversed_data, out.ptr, out.size));

	git_filter_list_free(fl);
	git_buf_free(&out);
}

void test_filter_custom__to_workdir(void)
{
	git_filter_list *fl;
	git_buf out = { 0 };
	git_buf in = GIT_BUF_INIT_CONST(
		bitflipped_and_reversed_data, BITFLIPPED_AND_REVERSED_DATA_LEN);

	cl_git_pass(git_filter_list_load(
		&fl, g_repo, NULL, "herofile", GIT_FILTER_TO_WORKTREE));

	cl_git_pass(git_filter_list_apply_to_data(&out, fl, &in));

	cl_assert_equal_i(strlen(workdir_data), out.size);

	cl_assert_equal_i(
		0, memcmp(workdir_data, out.ptr, out.size));

	git_filter_list_free(fl);
	git_buf_free(&out);
}

void test_filter_custom__can_register_a_custom_filter_in_the_repository(void)
{
	git_filter_list *fl;

	cl_git_pass(git_filter_list_load(
		&fl, g_repo, NULL, "herofile", GIT_FILTER_TO_WORKTREE));
	/* expect: bitflip, reverse, crlf */
	cl_assert_equal_sz(3, git_filter_list_length(fl));
	git_filter_list_free(fl);

	cl_git_pass(git_filter_list_load(
		&fl, g_repo, NULL, "herocorp", GIT_FILTER_TO_WORKTREE));
	/* expect: bitflip, reverse */
	cl_assert_equal_sz(2, git_filter_list_length(fl));
	git_filter_list_free(fl);

	cl_git_pass(git_filter_list_load(
		&fl, g_repo, NULL, "heroflip", GIT_FILTER_TO_WORKTREE));
	/* expect: bitflip (because of -reverse) */
	cl_assert_equal_sz(1, git_filter_list_length(fl));
	git_filter_list_free(fl);

	cl_git_pass(git_filter_list_load(
		&fl, g_repo, NULL, "doesntapplytome", GIT_FILTER_TO_WORKTREE));
	/* expect: none */
	cl_assert_equal_sz(0, git_filter_list_length(fl));
	git_filter_list_free(fl);
}
