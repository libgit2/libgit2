#include "clar_libgit2.h"
#include "push.h"
#include "transports/smart.h"
#include "git2/sys/repository.h"

struct push_stream {
	git_smart_subtransport_stream parent;
	git_str sent;
};

struct push_subtransport {
	git_smart_subtransport parent;
	struct push_stream *stream;
};

static int capture_write(git_smart_subtransport_stream *stream, const char *data, size_t len)
{
	struct push_stream *capture = (struct push_stream *)stream;
	return git_str_put(&capture->sent, data, len);
}

static int push_action(
	git_smart_subtransport_stream **out,
	git_smart_subtransport *transport,
	const char *url,
	git_smart_service_t service)
{
	struct push_subtransport *subtransport = (struct push_subtransport *)transport;
	GIT_UNUSED(url);
	cl_assert_equal_i(GIT_SERVICE_RECEIVEPACK, service);
	*out = &subtransport->stream->parent;
	return 0;
}

static void check_push_packet(git_oid_t local_type, const char *advertised_format, const char *capability, bool reject)
{
	git_repository *repo;
	git_repository_new_options opts = GIT_REPOSITORY_NEW_OPTIONS_INIT;
	git_remote remote = { 0 };
	git_push push = { 0 };
	push_spec spec;
	transport_smart transport;
	struct push_stream capture;
	struct push_subtransport subtransport;
	const char *name = "refs/heads/main";
	const char *sent, *capabilities;
	size_t packet_len, hexlen;
	char prefix[5];

	memset(&spec, 0, sizeof(spec));
	memset(&transport, 0, sizeof(transport));
	memset(&capture, 0, sizeof(capture));
	memset(&subtransport, 0, sizeof(subtransport));

	opts.oid_type = local_type;
	cl_git_pass(git_repository_new_ext(&repo, &opts));
	remote.repo = repo;
	push.repo = repo;
	push.remote = &remote;
	cl_git_pass(git_vector_init(&push.specs, 1, NULL));
	cl_git_pass(git_vector_init(&push.remote_push_options, 0, NULL));
	cl_git_pass(git_vector_init(&push.status, 0, NULL));
	spec.refspec.dst = (char *)name;
	git_oid_clear(&spec.loid, local_type);
	git_oid_clear(&spec.roid, local_type);
	cl_git_pass(git_vector_insert(&push.specs, &spec));
	transport.owner = &remote;
	transport.direction = GIT_DIRECTION_PUSH;
	transport.rpc = 1;
	transport.caps.object_format = (char *)advertised_format;
	transport.wrapped = &subtransport.parent;
	subtransport.parent.action = push_action;
	subtransport.stream = &capture;
	capture.parent.write = capture_write;
	capture.parent.subtransport = &subtransport.parent;

	if (reject) {
		cl_git_fail_with(GIT_ERROR, git_smart__push(&transport.parent, &push));
		cl_assert_equal_i(GIT_ERROR_INVALID, git_error_last()->klass);
		cl_assert_equal_i(0, (int)capture.sent.size);
	} else {
		cl_git_pass(git_smart__push(&transport.parent, &push));
		sent = git_str_cstr(&capture.sent);
		memcpy(prefix, sent, 4);
		prefix[4] = '\0';
		packet_len = (size_t)strtoul(prefix, NULL, 16);
		hexlen = git_oid_hexsize(local_type);
		cl_assert_equal_i((int)packet_len, (int)(capture.sent.size - 4));
		cl_assert_equal_s("0000", sent + packet_len);
		cl_assert_equal_i((int)hexlen, (int)(strchr(sent + 4, ' ') - (sent + 4)));
		cl_assert(strstr(sent + 4, name) != NULL);
		capabilities = memchr(sent, '\0', packet_len);
		cl_assert(capabilities != NULL);
		if (capability)
			cl_assert(git__memmem(capabilities, packet_len - (capabilities - sent), capability, strlen(capability)) != NULL);
		else
			cl_assert(git__memmem(capabilities, packet_len - (capabilities - sent), "object-format=", CONST_STRLEN("object-format=")) == NULL);
	}

	git_str_dispose(&capture.sent);
	git_vector_dispose(&push.specs);
	git_vector_dispose(&push.remote_push_options);
	git_vector_dispose(&push.status);
	git_repository_free(repo);
}

void test_transports_smart_push__sha256_negotiates_advertised_format(void)
{
	check_push_packet(GIT_OID_SHA256, "sha256", "object-format=sha256", false);
}

void test_transports_smart_push__sha1_without_advertisement(void)
{
	check_push_packet(GIT_OID_SHA1, NULL, NULL, false);
}

void test_transports_smart_push__sha1_advertised_format(void)
{
	check_push_packet(GIT_OID_SHA1, "sha1", "object-format=sha1", false);
}

void test_transports_smart_push__sha256_requires_advertisement(void)
{
	check_push_packet(GIT_OID_SHA256, NULL, NULL, true);
}

void test_transports_smart_push__rejects_mismatched_and_unknown_formats(void)
{
	check_push_packet(GIT_OID_SHA1, "sha256", NULL, true);
	check_push_packet(GIT_OID_SHA256, "sha1", NULL, true);
	check_push_packet(GIT_OID_SHA1, "sha512", NULL, true);
}
