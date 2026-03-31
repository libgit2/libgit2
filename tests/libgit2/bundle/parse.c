#include "clar_libgit2.h"
#include "posix.h"
#include "bundle.h"
#include "futils.h"

void test_bundle_parse__initialize(void)
{
	cl_fixture_sandbox("bundle");
}

void test_bundle_parse__cleanup(void)
{
	cl_fixture_cleanup("bundle");
}

void test_bundle_parse__v2(void)
{
	git_bundle_header *header = NULL;

	cl_assert(git_bundle__is_bundle("bundle/v2.header") == 1);
	cl_git_pass(git_bundle_header_open(&header, "bundle/v2.header"));
	cl_assert(header->version == 2);
	cl_assert(header->oid_type == GIT_OID_SHA1);
	cl_assert(git_vector_length(&header->prerequisites) == 2);
	cl_assert(git_vector_length(&header->refs) == 4);

	git_bundle_header_free(header);
}

void test_bundle_parse__v3(void)
{
	git_bundle_header *header = NULL;

	cl_assert(git_bundle__is_bundle("bundle/v3.header") == 1);
	cl_git_pass(git_bundle_header_open(&header, "bundle/v3.header"));
	cl_assert(header->version == 3);
	cl_assert(header->oid_type == GIT_OID_SHA1);
	cl_assert(git_vector_length(&header->prerequisites) == 3);
	cl_assert(git_vector_length(&header->refs) == 3);

	git_bundle_header_free(header);
}

void test_bundle_parse__v3_sha256(void)
{
	git_bundle_header *header = NULL;

#ifdef GIT_EXPERIMENTAL_SHA256
	cl_assert(git_bundle__is_bundle("bundle/v3_sha256.header") == 1);
	cl_git_pass(git_bundle_header_open(&header, "bundle/v3_sha256.header"));
	cl_assert(header->version == 3);
	cl_assert(header->oid_type == GIT_OID_SHA256);
	cl_assert(git_vector_length(&header->prerequisites) == 3);
	cl_assert(git_vector_length(&header->refs) == 3);
	git_bundle_header_free(header);
#else
	cl_assert(git_bundle__is_bundle("bundle/v3_sha256.header") == 0);
	cl_git_fail_with(
	        GIT_ENOTSUPPORTED,
	        git_bundle_header_open(&header, "bundle/v3_sha256.header"));
#endif
}

void test_bundle_parse__invalid(void)
{
	git_bundle_header *header = NULL;

	cl_assert(git_bundle__is_bundle("bundle/invalid.header") == 0);
	cl_git_fail_with(
	        GIT_EINVALID,
	        git_bundle_header_open(&header, "bundle/invalid.header"));
}

void test_bundle_parse__bundle_does_not_exist(void)
{
	git_bundle_header *header = NULL;

	cl_assert(git_bundle__is_bundle("bundle/does_not_exist.header") == 0);
	cl_git_fail_with(
	        GIT_ENOTFOUND,
	        git_bundle_header_open(&header, "bundle/does_not_exist.header"));

	git_bundle_header_free(header);
}

void test_bundle_parse__filter_capability_unsupported(void)
{
	git_bundle_header *header = NULL;

	cl_assert(
	        git_bundle__is_bundle("bundle/filter_capability.header") == 0);
	cl_git_fail_with(
	        GIT_ENOTSUPPORTED,
	        git_bundle_header_open(
	                &header, "bundle/filter_capability.header"));

	git_bundle_header_free(header);
}

void test_bundle_parse__unknown_capability(void)
{
	git_bundle_header *header = NULL;

	cl_assert(
	        git_bundle__is_bundle("bundle/unknown_capability.header") == 0);
	cl_git_fail_with(
	        GIT_EINVALID,
	        git_bundle_header_open(
	                &header, "bundle/unknown_capability.header"));

	git_bundle_header_free(header);
}

void test_bundle_parse__bad_oid(void)
{
	git_bundle_header *header = NULL;

	cl_assert(git_bundle__is_bundle("bundle/bad_oid.header") == 0);
	cl_git_fail_with(
	        GIT_ERROR,
	        git_bundle_header_open(&header, "bundle/bad_oid.header"));

	git_bundle_header_free(header);
}
