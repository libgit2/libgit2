#ifdef __EMSCRIPTEN__

#include "emscripten.h"
#include "emscripten/fetch.h"

extern "C" {
#include "common.h"
#include "git2/transport.h"
#include "smart.h"
}

#include <map>

#include "deps/picosha2/picosha2.h"

static const char *upload_pack_ls_service_url = "/info/refs?service=git-upload-pack";
static const char *upload_pack_service_url = "/git-upload-pack";
static const char *receive_pack_ls_service_url = "/info/refs?service=git-receive-pack";
static const char *receive_pack_service_url = "/git-receive-pack";

namespace {
struct StreamInternal {
  std::string url;
  std::vector<char> writeBuffer;
  std::vector<const char *> headers;  // [key1, value1, key2, value2, ...]
  emscripten_fetch_attr_t attr;
  emscripten_fetch_t *fetch{nullptr};
  size_t totalBytesRead{0};
};

uint64_t connectionCount{0};
std::map<uint64_t, StreamInternal> connectionMap;

uint64_t xhrConnect(
  const std::string url, const char *method, const std::vector<const char *> headers) {
  auto connectionNumber = connectionCount++;
  auto &connection = connectionMap[connectionNumber];
  connection.url = url;
  connection.headers = headers;
  emscripten_fetch_attr_init(&connection.attr);
  strcpy(connection.attr.requestMethod, method);
  // NOTE(pawel) EMSCRIPTEN_FETCH_REPLACE is needed for synchronous to work...
  // https://github.com/emscripten-core/emscripten/issues/8183
  connection.attr.attributes =
    EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_SYNCHRONOUS | EMSCRIPTEN_FETCH_REPLACE;
  if (std::string(method) == "GET") {
    auto headersToSend = connection.headers;
    headersToSend.push_back(0);
    connection.attr.requestHeaders = headersToSend.data();
    connection.fetch = emscripten_fetch(&connection.attr, url.c_str());
  }
  return connectionNumber;
}

// This call buffers the writes. The buffer is sent on the wire when xhrRead() is called.
void xhrWrite(uint64_t connectionNumber, const char *buffer, size_t size) {
  if (connectionMap.count(connectionNumber) != 1) {
    printf("Attempting to write to connection %l but it is not connected", connectionNumber);
    return;
  }
  auto &connection = connectionMap[connectionNumber];
  connection.writeBuffer.insert(end(connection.writeBuffer), buffer, buffer + size);
}

// Sends pending writes and returns response async. The result is buffered so this can be invoked
// until the full length of the buffer is read.
void xhrRead(uint64_t connectionNumber, char *buffer, size_t bufferSize, size_t *bytesRead) {
  if (connectionMap.count(connectionNumber) != 1) {
    printf("Attempting to read from connection %l but it is not connected\n", connectionNumber);
    *bytesRead = 0;
    return;
  }
  auto &connection = connectionMap[connectionNumber];
  std::vector<const char *> headersToSend = connection.headers;
  std::string sha256;
  if (!connection.writeBuffer.empty()) {
    const auto data = connection.writeBuffer.data();
    const auto dataSize = connection.writeBuffer.size();
    connection.attr.requestData = data;
    connection.attr.requestDataSize = dataSize;

    std::vector<uint8_t> hash(picosha2::k_digest_size);
    picosha2::hash256(data, data + dataSize, hash.begin(), hash.end());
    sha256 = picosha2::bytes_to_hex_string(hash.begin(), hash.end());
    headersToSend.push_back("x-amz-content-sha256");
    headersToSend.push_back(sha256.c_str());  // This pointer is only valid in this scope.
  }
  auto &f = connection.fetch;
  if (!f) {
    headersToSend.push_back(0);  // null terminate the array.
    connection.attr.requestHeaders = headersToSend.data();

    f = emscripten_fetch(&connection.attr, connection.url.c_str());
    if (f->status != 200) {
      printf("%d %s %s\n", f->status, f->statusText, f->url);
    }
    if (f->readyState != 4) {
      printf("Connection is not in ready state (%d)\n", f->readyState);
    }
  }
  connection.writeBuffer.clear();
  *bytesRead = min(f->numBytes - connection.totalBytesRead, bufferSize);
  std::memcpy(buffer, f->data + connection.totalBytesRead, *bytesRead);
  connection.totalBytesRead += *bytesRead;
}

void xhrFree(uint64_t connectionNumber) {
  if (connectionMap.count(connectionNumber != 1)) {
    printf("Attempting to free unkown connection %l", connectionNumber);
    return;
  }
  auto &connection = connectionMap[connectionNumber];
  emscripten_fetch_close(connection.fetch);
  connectionMap.erase(connectionNumber);
}
}  // namespace

namespace {
typedef struct {
  git_smart_subtransport_stream parent;
  git_str service_url;
  uint64_t connectionNo;
} emscriptenhttp_stream;

typedef struct {
  git_smart_subtransport parent;
  transport_smart *owner;
} emscriptenhttp_subtransport;

// Since these types are being sent back to C, we need to ensure they are PODs.
static_assert(std::is_pod<emscriptenhttp_stream>());
static_assert(std::is_pod<emscriptenhttp_subtransport>());

static int emscriptenhttp_stream_read(
  git_smart_subtransport_stream *stream, char *buffer, size_t buf_size, size_t *bytes_read) {
  emscriptenhttp_stream *s = (emscriptenhttp_stream *)stream;

  if (s->connectionNo == -1) {
    s->connectionNo = xhrConnect(git_str_cstr(&s->service_url), "GET", {});
  }
  xhrRead(s->connectionNo, buffer, buf_size, bytes_read);

  return 0;
}

static int emscriptenhttp_stream_write_single(
  git_smart_subtransport_stream *stream, const char *buffer, size_t len) {
  emscriptenhttp_stream *s = (emscriptenhttp_stream *)stream;

  if (s->connectionNo == -1) {

    auto serviceUrl = git_str_cstr(&s->service_url);
    bool uploadPack = strstr(serviceUrl, "git-upload-pack") != 0;

    s->connectionNo = xhrConnect(
      serviceUrl,
      "POST",
      {
        "Content-Type",
        uploadPack ? "application/x-git-upload-pack-request"
                   : "application/x-git-receive-pack-request",
        "Pragma",
        "no-cache",
      });
  }
  xhrWrite(s->connectionNo, buffer, len);

  return 0;
}

static void emscriptenhttp_stream_free(git_smart_subtransport_stream *stream) {
  emscriptenhttp_stream *s = (emscriptenhttp_stream *)stream;
  if (s->connectionNo != -1) {
    xhrFree(s->connectionNo);
  }
  git_str_dispose(&s->service_url);
  git__free(s);
}

static int emscriptenhttp_stream_alloc(
  emscriptenhttp_subtransport *t, emscriptenhttp_stream **stream) {
  emscriptenhttp_stream *s;

  if (!stream)
    return -1;

  s = reinterpret_cast<emscriptenhttp_stream *>(git__calloc(1, sizeof(emscriptenhttp_stream)));
  GIT_ERROR_CHECK_ALLOC(s);

  s->parent.subtransport = &t->parent;
  s->parent.read = emscriptenhttp_stream_read;
  s->parent.write = emscriptenhttp_stream_write_single;
  s->parent.free = emscriptenhttp_stream_free;
  s->connectionNo = -1;
  s->service_url = GIT_STR_INIT;

  *stream = s;

  return 0;
}

static int emscriptenhttp_action(
  git_smart_subtransport_stream **stream,
  git_smart_subtransport *subtransport,
  const char *url,
  git_smart_service_t action) {
  emscriptenhttp_subtransport *t = (emscriptenhttp_subtransport *)subtransport;
  emscriptenhttp_stream *s;

  if (emscriptenhttp_stream_alloc(t, &s) < 0) {
    return -1;
  }

  switch (action) {
    case GIT_SERVICE_UPLOADPACK_LS:
      git_str_printf(&s->service_url, "%s%s", url, upload_pack_ls_service_url);
      break;
    case GIT_SERVICE_UPLOADPACK:
      git_str_printf(&s->service_url, "%s%s", url, upload_pack_service_url);
      break;
    case GIT_SERVICE_RECEIVEPACK_LS:
      git_str_printf(&s->service_url, "%s%s", url, receive_pack_ls_service_url);
      break;
    case GIT_SERVICE_RECEIVEPACK:
      git_str_printf(&s->service_url, "%s%s", url, receive_pack_service_url);
      break;
  }

  if (git_str_oom(&s->service_url)) {
    return -1;
  }

  *stream = &s->parent;
  return 0;
}

static int emscriptenhttp_close(git_smart_subtransport *subtransport) { return 0; }

static void emscriptenhttp_free(git_smart_subtransport *subtransport) {
  emscriptenhttp_subtransport *t = (emscriptenhttp_subtransport *)subtransport;
  emscriptenhttp_close(subtransport);
  git__free(t);
}

}  // namespace

extern "C" {
int git_smart_subtransport_http(git_smart_subtransport **out, git_transport *owner, void *param) {
  emscriptenhttp_subtransport *t;

  GIT_UNUSED(param);

  if (!out)
    return -1;

  t = reinterpret_cast<emscriptenhttp_subtransport *>(
    git__calloc(1, sizeof(emscriptenhttp_subtransport)));
  GIT_ERROR_CHECK_ALLOC(t);

  t->owner = (transport_smart *)owner;
  t->parent.action = emscriptenhttp_action;
  t->parent.close = emscriptenhttp_close;
  t->parent.free = emscriptenhttp_free;

  *out = (git_smart_subtransport *)t;

  return 0;
}
}

#endif /* __EMSCRIPTEN__ */
