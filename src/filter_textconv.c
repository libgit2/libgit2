/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/filter_textconv.h"

#include "common.h"
#include "fileops.h"
#include "hash.h"
#include "git2/config.h"
#include "blob.h"
#include "filter.h"
#include "textconv.h"

struct buf_stream {
    git_writestream parent;
    git_buf *target;
    bool complete;
};

static int buf_stream_write(
                            git_writestream *s, const char *buffer, size_t len)
{
    struct buf_stream *buf_stream = (struct buf_stream *)s;
    assert(buf_stream);
    
    assert(buf_stream->complete == 0);
    
    return git_buf_put(buf_stream->target, buffer, len);
}

static int buf_stream_close(git_writestream *s)
{
    struct buf_stream *buf_stream = (struct buf_stream *)s;
    assert(buf_stream);
    
    assert(buf_stream->complete == 0);
    buf_stream->complete = 1;
    
    return 0;
}

static void buf_stream_free(git_writestream *s)
{
    GIT_UNUSED(s);
}

static void buf_stream_init(struct buf_stream *writer, git_buf *target)
{
    memset(writer, 0, sizeof(struct buf_stream));
    
    writer->parent.write = buf_stream_write;
    writer->parent.close = buf_stream_close;
    writer->parent.free = buf_stream_free;
    writer->target = target;
    
    git_buf_clear(target);
}

static int buf_from_blob(git_buf *out, git_blob *blob)
{
    git_off_t rawsize = git_blob_rawsize(blob);
    
    if (!git__is_sizet(rawsize)) {
        giterr_set(GITERR_OS, "blob is too large to filter");
        return -1;
    }
    
    git_buf_attach_notowned(out, git_blob_rawcontent(blob), (size_t)rawsize);
    return 0;
}

int git_filter_textconv_apply_to_data(
                                  git_buf *tgt, git_filter_list *filters, git_textconv *textconv, git_buf *src)
{
    struct buf_stream writer;
    int error;
    
    git_buf_sanitize(tgt);
    git_buf_sanitize(src);
    
    if (!filters && !textconv) {
        git_buf_attach_notowned(tgt, src->ptr, src->size);
        return 0;
    }
    
    buf_stream_init(&writer, tgt);
    
    if ((error = git_filter_textconv_stream_data(filters, textconv, src,
                                             &writer.parent)) < 0)
        return error;
    
    assert(writer.complete);
    return error;
}

int git_filter_textconv_apply_to_file(
                                  git_buf *out,
                                  git_filter_list *filters,
                                  git_textconv *textconv,
                                  git_repository *repo,
                                  const char *path)
{
    struct buf_stream writer;
    int error;
    
    buf_stream_init(&writer, out);
    
    if ((error = git_filter_textconv_stream_file(
                                             filters, textconv, repo, path, &writer.parent)) < 0)
        return error;
    
    assert(writer.complete);
    return error;
}

int git_filter_textconv_apply_to_blob(
                                  git_buf *out,
                                  git_filter_list *filters,
                                  git_textconv *textconv,
                                      git_blob *blob) {
    struct buf_stream writer;
    int error;
    buf_stream_init(&writer, out);
    if ((error = git_filter_textconv_stream_blob(filters, textconv, blob, &writer.parent)) < 0)
        return error;
    assert(writer.complete);
    return error;
}

int git_filter_textconv_stream_file(
                                git_filter_list *filters,
                                git_textconv *textconv,
                                git_repository *repo,
                                const char *path,
                                git_writestream *target)
{
    char buf[FILTERIO_BUFSIZE];
    git_buf abspath = GIT_BUF_INIT;
    const char *base = repo ? git_repository_workdir(repo) : NULL;
    git_vector filter_streams = GIT_VECTOR_INIT;
    git_writestream *stream_start;
    ssize_t readlen;
    int fd = -1, error, initialized = 0;
    
    git_writestream* textconv_stream = NULL;
    git_buf textconv_buf = GIT_BUF_INIT;
    
    if ((error = git_textconv_init_stream(&textconv_stream, textconv, &textconv_buf, target)) < 0)
        goto done;
    
    if ((error = git_filter_list_stream_init(
                                             &stream_start, &filter_streams, filters, textconv_stream)) < 0 ||
        (error = git_path_join_unrooted(&abspath, path, base, NULL)) < 0)
        goto done;
    initialized = 1;
    
    if ((fd = git_futils_open_ro(abspath.ptr)) < 0) {
        error = fd;
        goto done;
    }
    
    while ((readlen = p_read(fd, buf, sizeof(buf))) > 0) {
        if ((error = stream_start->write(stream_start, buf, readlen)) < 0)
            goto done;
    }
    
    if (readlen < 0)
        error = readlen;
    
done:
    if (initialized)
        error |= stream_start->close(stream_start);
    
    if (fd >= 0)
        p_close(fd);
    stream_list_free(&filter_streams);
    if (textconv_stream && textconv_stream != target) textconv_stream->free(textconv_stream);
    git_buf_free(&abspath);
    git_buf_free(&textconv_buf);
    return error;
}

int git_filter_textconv_stream_data(
                                git_filter_list *filters,
                                git_textconv *textconv,
                                git_buf *data,
                                git_writestream *target)
{
    git_vector filter_streams = GIT_VECTOR_INIT;
    git_writestream *stream_start;
    int error, initialized = 0;
    git_writestream* textconv_stream  = NULL;
    git_buf textconv_buf = GIT_BUF_INIT;

    git_buf_sanitize(data);

    if ((error = git_textconv_init_stream(&textconv_stream, textconv, &textconv_buf, target)) < 0)
        goto out;

    if ((error = git_filter_list_stream_init(&stream_start, &filter_streams, filters, textconv_stream)) < 0)
        goto out;
    initialized = 1;
    
    if ((error = stream_start->write(
                                     stream_start, data->ptr, data->size)) < 0)
        goto out;
    
out:
    if (initialized)
        error |= stream_start->close(stream_start);
    
    stream_list_free(&filter_streams);
    if (textconv_stream && textconv_stream != target) textconv_stream->free(textconv_stream);
    git_buf_free(&textconv_buf);
    return error;
}

int git_filter_textconv_stream_blob(
                                git_filter_list *filters,
                                git_textconv *textconv,
                                git_blob *blob,
                                git_writestream *target)
{
    git_buf in = GIT_BUF_INIT;
    
    if (buf_from_blob(&in, blob) < 0)
        return -1;
    
    if (filters)
        git_oid_cpy(&filters->source.oid, git_blob_id(blob));
    
    return git_filter_textconv_stream_data(filters, textconv, &in, target);
}
