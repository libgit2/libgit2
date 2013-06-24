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

/* Represents the data above scrambled (bits flipped) after \r\n -> \n conversion,
 * then bytewise reversed */
static unsigned char bitflipped_and_reversed_data[] = 
	{ 0xf5, 0xd1, 0x9b, 0x9a, 0x93, 0x9d, 0x92, 0x9e, 0x8d, 0x9c, 0x8c,
	  0xf5, 0x86, 0x93, 0x93, 0x9e, 0x96, 0x89, 0x96, 0x8d, 0x8b, 0xf5,
	  0x9a, 0x9d, 0xdf, 0x93, 0x93, 0x96, 0x88, 0xdf, 0x8b, 0x9e, 0x97,
	  0x8b, 0xf5, 0x9e, 0x8b, 0x9e, 0x9b, 0xf5, 0x9a, 0x93, 0x8f, 0x92,
	  0x96, 0x8c, 0xdf, 0x9a, 0x92, 0x90, 0x8c };
#define BITFLIPPED_AND_REVERSED_DATA_LEN 51

static git_repository *g_repo = NULL;

void test_filter_custom__initialize(void)
{
	g_repo = cl_git_sandbox_init("empty_standard_repo");
}

void test_filter_custom__cleanup(void)
{
	cl_git_sandbox_cleanup();
	g_repo = NULL;
}

static int bitflip_filter_should_apply(
	git_filter *filter,
	const char *path,
	git_filter_mode_t mode)
{
	GIT_UNUSED(filter);
	GIT_UNUSED(mode);

	return (git__strncmp("hero", path, 4) == 0);
}

static int bitflip_filter_apply(
	void **out,
	size_t *out_len,
	git_filter *filter,
	const char *path,
	git_filter_mode_t mode,
	const void *in,
	size_t in_len)
{
	const unsigned char *src = in;
	unsigned char *dst;
	int should_apply;
	size_t i;

	cl_assert((should_apply = filter->should_apply(filter, path, mode)) >= 0);

	if (should_apply == 0)
		return 0;

	if (in_len == 0)
		return 0;

	cl_assert(dst = git__malloc(in_len));

	for (i = 0; i < in_len; i++)
		dst[i] = VERY_SECURE_ENCRYPTION(src[i]);

	*out = dst;
	*out_len = in_len;

	return 1;
}

static void bitflip_filter_free_buf(void *buf)
{
	git__free(buf);
}

static void bitflip_filter_free(git_filter *f)
{
	git__free(f);
}

static git_filter *create_bitflip_filter(void)
{
	git_filter *filter;

	if ((filter = git__calloc(1, sizeof(git_filter))) == NULL)
		return NULL;

	filter->version = GIT_FILTER_VERSION;
	filter->should_apply = bitflip_filter_should_apply;
	filter->apply = bitflip_filter_apply;
	filter->free_buf = bitflip_filter_free_buf;
	filter->free = bitflip_filter_free;

	return filter;
}

static int reverse_filter_should_apply(
	git_filter *filter,
	const char *path,
	git_filter_mode_t mode)
{
	GIT_UNUSED(filter);
	GIT_UNUSED(mode);

	return (git__strncmp("hero", path, 4) == 0);
}

static int reverse_filter_apply(
	void **out,
	size_t *out_len,
	git_filter *filter,
	const char *path,
	git_filter_mode_t mode,
	const void *in,
	size_t in_len)
{
	const unsigned char *src = in;
	unsigned char *dst;
	int should_apply;
	size_t i;

	cl_assert((should_apply = filter->should_apply(filter, path, mode)) >= 0);

	if (should_apply == 0)
		return 0;

	if (in_len == 0)
		return 0;

	cl_assert(dst = git__malloc(in_len));

	for (i = 0; i < in_len; i++) {
		dst[i] = src[(in_len - 1) - i];
	}

	*out = dst;
	*out_len = in_len;

	return 1;
}

static void reverse_filter_free_buf(void *buf)
{
	git__free(buf);
}

static void reverse_filter_free(git_filter *f)
{
	git__free(f);
}

static git_filter *create_reverse_filter(void)
{
	git_filter *filter;

	if ((filter = git__calloc(1, sizeof(git_filter))) == NULL)
		return NULL;

	filter->version = GIT_FILTER_VERSION;
	filter->should_apply = reverse_filter_should_apply;
	filter->apply = reverse_filter_apply;
	filter->free_buf = reverse_filter_free_buf;
	filter->free = reverse_filter_free;

	return filter;
}

void test_filter_custom__to_odb(void)
{
	git_config *cfg;
	git_filterbuf *out;
	unsigned char *filtered;
	size_t i;

	cl_git_pass(git_repository_config(&cfg, g_repo));
	cl_assert(cfg);

	git_attr_cache_flush(g_repo);
	cl_git_append2file("empty_standard_repo/.gitattributes", "herofile text\n");

	cl_git_pass(git_repository_add_filter(g_repo, create_bitflip_filter(), BITFLIP_FILTER_PRIORITY));
	cl_git_pass(git_repository_add_filter(g_repo, create_reverse_filter(), REVERSE_FILTER_PRIORITY));

	git_filters__apply(&out, &g_repo->filters, "herofile",
		GIT_FILTER_TO_ODB, workdir_data, strlen(workdir_data));

	cl_assert_equal_i(BITFLIPPED_AND_REVERSED_DATA_LEN, out->len);

	for (i = 0, filtered = out->ptr; i < out->len; i++)
		cl_assert(filtered[i] == bitflipped_and_reversed_data[i]);

	git_filterbuf_free(out);
	git_config_free(cfg);
}

void test_filter_custom__to_workdir(void)
{
	git_config *cfg;
	git_filterbuf *out;
	unsigned char *filtered;
	size_t i;

	cl_git_pass(git_repository_config(&cfg, g_repo));
	cl_assert(cfg);

	git_attr_cache_flush(g_repo);
	cl_git_append2file("empty_standard_repo/.gitattributes", "herofile text\n");

	cl_git_pass(git_repository_add_filter(g_repo, create_bitflip_filter(), BITFLIP_FILTER_PRIORITY));
	cl_git_pass(git_repository_add_filter(g_repo, create_reverse_filter(), REVERSE_FILTER_PRIORITY));

	git_filters__apply(&out, &g_repo->filters, "herofile",
		GIT_FILTER_TO_WORKDIR,
		bitflipped_and_reversed_data, BITFLIPPED_AND_REVERSED_DATA_LEN);

	cl_assert_equal_i(strlen(workdir_data), out->len);

	for (i = 0, filtered = out->ptr; i < out->len; i++)
		cl_assert(filtered[i] == workdir_data[i]);

	git_filterbuf_free(out);
	git_config_free(cfg);
}

void test_filter_custom__can_register_a_custom_filter_in_the_repository(void)
{
	git_filter *filter;
	git_vector filters = GIT_VECTOR_INIT;
	int filters_nb = 0;

	filters_nb = git_filters__load(&filters, g_repo, "herocorp", GIT_FILTER_TO_WORKDIR);
	git_vector_clear(&filters);

	cl_assert(filter = create_bitflip_filter());
	cl_git_pass(git_repository_add_filter(g_repo, filter, BITFLIP_FILTER_PRIORITY));

	git_filters__load(&filters, g_repo, "herocorp", GIT_FILTER_TO_WORKDIR);
	cl_assert_equal_sz(filters_nb + 1, filters.length);
	git_vector_clear(&filters);

	git_filters__load(&filters, g_repo, "doesntapplytome", GIT_FILTER_TO_WORKDIR);
	cl_assert_equal_sz(filters_nb, filters.length);
	git_vector_clear(&filters);

	git_vector_free(&filters);
}
