#include <git2.h>
#include <git2/sys/transport.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

struct fuzz_buffer {
	const uint8_t *data;
	size_t size;
};
typedef struct fuzz_buffer fuzz_buffer;

struct fuzzer_stream {
	git_smart_subtransport_stream base;
	const uint8_t *readp;
	const uint8_t *endp;
};
typedef struct fuzzer_stream fuzzer_stream;

int fuzzer_stream_read(git_smart_subtransport_stream *stream,
		       char *buffer,
		       size_t buf_size,
		       size_t *bytes_read)
{
	fuzzer_stream *fs = (fuzzer_stream*)(stream);
	size_t avail = fs->endp - fs->readp;
	if (buf_size < avail) {
		*bytes_read = buf_size;
	} else {
		*bytes_read = avail;
	}
	memcpy(buffer, fs->readp, *bytes_read);
	fs->readp += *bytes_read;
	return 0;
}

int fuzzer_stream_write(git_smart_subtransport_stream *stream,
			const char *buffer,
			size_t len)
{
	return 0;
}

void fuzzer_stream_free(git_smart_subtransport_stream *stream)
{
	fuzzer_stream *fs = (fuzzer_stream*)(stream);
	free(fs);
}

fuzzer_stream *fuzzer_stream_new(fuzz_buffer data)
{
	fuzzer_stream *fs = calloc(1, sizeof(fuzzer_stream));
	fs->base.read = fuzzer_stream_read;
	fs->base.write = fuzzer_stream_write;
	fs->base.free = fuzzer_stream_free;
	fs->readp = data.data;
	fs->endp = data.data + data.size;
	return fs;
}

struct fuzzer_subtransport {
	git_smart_subtransport base;
	git_transport *owner;
	fuzz_buffer data;
};

typedef struct fuzzer_subtransport fuzzer_subtransport;

int fuzzer_subtransport_action(git_smart_subtransport_stream **out,
			       git_smart_subtransport *transport,
			       const char *url,
			       git_smart_service_t action)
{
	fuzzer_subtransport *ft = (fuzzer_subtransport*)(transport);
	fuzzer_stream *stream = fuzzer_stream_new(ft->data);
	*out = &stream->base;
	return 0;
}

int fuzzer_subtransport_close(git_smart_subtransport *transport)
{
	return 0;
}

void fuzzer_subtransport_free(git_smart_subtransport *transport)
{
	fuzzer_subtransport *ft = (fuzzer_subtransport*)(transport);
	free(ft);
}

fuzzer_subtransport *fuzzer_subtransport_new(git_transport* owner, fuzz_buffer *buf)
{
	fuzzer_subtransport *fst = calloc(1, sizeof(fuzzer_subtransport));
	fst->base.action = fuzzer_subtransport_action;
	fst->base.close = fuzzer_subtransport_close;
	fst->base.free = fuzzer_subtransport_free;
	fst->owner = owner;
	fst->data = *buf;
	return fst;
}

int fuzzer_subtransport_cb(git_smart_subtransport **out,
			   git_transport* owner,
			   void* param)
{
	fuzz_buffer *buf = (fuzz_buffer*)(param);
	fuzzer_subtransport *sub = fuzzer_subtransport_new(owner, buf);

	*out = &sub->base;
	return 0;
}

int create_fuzzer_transport(git_transport **out, git_remote *owner, void *param)
{
	git_smart_subtransport_definition fuzzer_subtransport = {fuzzer_subtransport_cb, 1, param};
	return git_transport_smart(out, owner, &fuzzer_subtransport);
}

void fuzzer_git_abort(const char *op)
{
	const git_error *err  = giterr_last();
	fprintf(stderr, "unexpected libgit error: %s: %s\n",
		op, err ? err->message : "<none>");
	abort();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	static git_repository *repo = NULL;
	if (repo == NULL) {
		git_libgit2_init();
		char tmp[] = "/tmp/git2.XXXXXX";
		if (mkdtemp(tmp) != tmp) {
			abort();
		}
		int err = git_repository_init(&repo, tmp, 1);
		if (err != 0) {
			fuzzer_git_abort("git_repository_init");
		}
	}

	int err;
	git_remote *remote;
	err = git_remote_create_anonymous(&remote, repo, "fuzzer://remote-url");
	if (err != 0) {
		fuzzer_git_abort("git_remote_create");
	}


	fuzz_buffer buffer = {data, size};
	git_remote_callbacks callbacks = GIT_REMOTE_CALLBACKS_INIT;
	callbacks.transport = create_fuzzer_transport;
	callbacks.payload = &buffer;

	err = git_remote_connect(remote, GIT_DIRECTION_FETCH, &callbacks, NULL, NULL);
	if (err != 0) {
		goto out;
	}

	git_remote_download(remote, NULL, NULL);

 out:
	git_remote_free(remote);

	return 0;
}
