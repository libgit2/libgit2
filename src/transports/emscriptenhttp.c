#ifdef __EMSCRIPTEN__

#include "common.h"
#include "emscripten.h"
#include "git2/transport.h"
#include "smart.h"

static const char *upload_pack_ls_service_url = "/info/refs?service=git-upload-pack";
static const char *upload_pack_service_url = "/git-upload-pack";
static const char *receive_pack_ls_service_url = "/info/refs?service=git-receive-pack";
static const char *receive_pack_service_url = "/git-receive-pack";

typedef struct {
	git_smart_subtransport_stream parent;
    const char *service_url;
	int connectionNo;
} emscriptenhttp_stream;

typedef struct {
	git_smart_subtransport parent;
	transport_smart *owner;

} emscriptenhttp_subtransport;

static int emscriptenhttp_stream_read(
	git_smart_subtransport_stream *stream,
	char *buffer,
	size_t buf_size,
	size_t *bytes_read)
{
    emscriptenhttp_stream *s = (emscriptenhttp_stream *)stream;

	if(s->connectionNo == -1) {
		s->connectionNo = EM_ASM_INT({
			const url = UTF8ToString($0);
			return Module.emscriptenhttpconnect(url, $1);
		}, s->service_url, DEFAULT_BUFSIZE);
	}

    *bytes_read = EM_ASM_INT({
		return Module.emscriptenhttpread($0, $1, $2);
    }, s->connectionNo, buffer, buf_size);

    return 0;
}

static int emscriptenhttp_stream_write_single(
	git_smart_subtransport_stream *stream,
	const char *buffer,
	size_t len)
{
    emscriptenhttp_stream *s = (emscriptenhttp_stream *)stream;
    
	if(s->connectionNo == -1) {
		s->connectionNo = EM_ASM_INT({
			const url = UTF8ToString($0);
			return Module.emscriptenhttpconnect(url, $1, 'POST', {
				'Content-Type': url.indexOf('git-upload-pack') > 0 ? 
					'application/x-git-upload-pack-request' :
					'application/x-git-receive-pack-request'
			});
		}, s->service_url, DEFAULT_BUFSIZE);
	}

   EM_ASM({
		return Module.emscriptenhttpwrite($0, $1, $2);
    }, s->connectionNo, buffer, len);

    return 0;
}

static void emscriptenhttp_stream_free(git_smart_subtransport_stream *stream)
{
	emscriptenhttp_stream *s = (emscriptenhttp_stream *)stream;

	git__free(s);
}

static int emscriptenhttp_stream_alloc(emscriptenhttp_subtransport *t, emscriptenhttp_stream **stream)
{
	emscriptenhttp_stream *s;

	if (!stream)
		return -1;

	s = git__calloc(1, sizeof(emscriptenhttp_stream));
	GIT_ERROR_CHECK_ALLOC(s);

	s->parent.subtransport = &t->parent;
	s->parent.read = emscriptenhttp_stream_read;
	s->parent.write = emscriptenhttp_stream_write_single;
	s->parent.free = emscriptenhttp_stream_free;
	s->connectionNo = -1;

	*stream = s;

	return 0;
}

static int emscriptenhttp_action(
	git_smart_subtransport_stream **stream,
	git_smart_subtransport *subtransport,
	const char *url,
	git_smart_service_t action)
{
    emscriptenhttp_subtransport *t = (emscriptenhttp_subtransport *)subtransport;
	emscriptenhttp_stream *s;
	
    if (emscriptenhttp_stream_alloc(t, &s) < 0)
		return -1;

    git_buf buf = GIT_BUF_INIT;
    
    switch(action) {
        case GIT_SERVICE_UPLOADPACK_LS:
            git_buf_printf(&buf, "%s%s", url, upload_pack_ls_service_url);   

            break;
        case GIT_SERVICE_UPLOADPACK:
            git_buf_printf(&buf, "%s%s", url, upload_pack_service_url);
            break;
        case GIT_SERVICE_RECEIVEPACK_LS:
            git_buf_printf(&buf, "%s%s", url, receive_pack_ls_service_url);
            break;
        case GIT_SERVICE_RECEIVEPACK:
            git_buf_printf(&buf, "%s%s", url, receive_pack_service_url);
            break;            
    }

    s->service_url = git_buf_cstr(&buf);
    *stream = &s->parent;

    return 0;
}

static int emscriptenhttp_close(git_smart_subtransport *subtransport)
{	
	return 0;
}

static void emscriptenhttp_free(git_smart_subtransport *subtransport)
{
	emscriptenhttp_subtransport *t = (emscriptenhttp_subtransport *)subtransport;

	emscriptenhttp_close(subtransport);

	git__free(t);
}

int git_smart_subtransport_http(git_smart_subtransport **out, git_transport *owner, void *param) {
    emscriptenhttp_subtransport *t;

	GIT_UNUSED(param);

	if (!out)
		return -1;

	t = git__calloc(1, sizeof(emscriptenhttp_subtransport));
	GIT_ERROR_CHECK_ALLOC(t);

	t->owner = (transport_smart *)owner;
	t->parent.action = emscriptenhttp_action;
	t->parent.close = emscriptenhttp_close;
	t->parent.free = emscriptenhttp_free;

	*out = (git_smart_subtransport *) t;

    return 0;
}

#endif /* __EMSCRIPTEN__ */