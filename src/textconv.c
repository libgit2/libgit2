/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "textconv.h"

#include "common.h"
#include "fileops.h"
#include "hash.h"
#include "repository.h"
#include "global.h"
#include "git2/sys/textconv.h"
#include "git2/config.h"
#include "blob.h"
#include "attr_file.h"
#include "array.h"

#define GIT_TEXTCONV_ATTR "diff"
#define TEXTCONV_IO_BUFSIZE 8096

typedef struct {
    char *textconv_name;
    git_textconv *textconv;
    int initialized;
} git_textconv_entry;

struct git_textconv_registry {
    git_rwlock lock;
    git_vector textconvs;
};

static struct git_textconv_registry textconv_registry;

static void git_textconv_global_shutdown(void);

static int textconv_entry_name_key_check(const void *key, const void *entry)
{
    const char *name =
    entry ? ((const git_textconv_entry *)entry)->textconv_name : NULL;
    return name ? git__strcmp(key, name) : -1;
}

static int textconv_entry_name_cmp(const void *a, const void *b)
{
    const char* na = ((const git_textconv_entry *)a)->textconv_name;
    const char* nb = ((const git_textconv_entry *)b)->textconv_name;
    return git__strcmp(na, nb);
}

static int textconv_entry_textconv_key_check(const void *key, const void *entry)
{
    const void *textconv = entry ? ((const git_textconv_entry *)entry)->textconv : NULL;
    return (key == textconv) ? 0 : -1;
}

/* Note: callers must lock the registry before calling this function */
static int textconv_registry_insert(
                                  const char *name, git_textconv *textconv)
{
    git_textconv_entry *entry;
    
    size_t alloc_len = 0;
    GITERR_CHECK_ALLOC_ADD(&alloc_len, alloc_len, sizeof(git_textconv_entry));
    
    entry = git__calloc(1, alloc_len);
    GITERR_CHECK_ALLOC(entry);
    
    entry->textconv_name = git__strdup(name);
    GITERR_CHECK_ALLOC(entry->textconv_name);
    
    entry->textconv      = textconv;
    
    if (git_vector_insert(&textconv_registry.textconvs, entry) < 0) {
        git__free(entry->textconv_name);
        git__free(entry);
        return -1;
    }
    
    return 0;
}

int git_textconv_global_init(void)
{
    git_textconv *http = NULL;
    int error = 0;
    
    if (git_rwlock_init(&textconv_registry.lock) < 0)
        return -1;
    
    if ((error = git_vector_init(&textconv_registry.textconvs, 2,
                                 textconv_entry_name_cmp)) < 0)
        goto done;
    
    git__on_shutdown(git_textconv_global_shutdown);
    
done:
    if (error) {
        git_textconv_free(http);
    }
    
    return error;
}

static void git_textconv_global_shutdown(void)
{
    size_t pos;
    git_textconv_entry *entry;
    
    if (git_rwlock_wrlock(&textconv_registry.lock) < 0)
        return;
    
    git_vector_foreach(&textconv_registry.textconvs, pos, entry) {
        if (entry->textconv && entry->textconv->shutdown) {
            entry->textconv->shutdown(entry->textconv);
            entry->initialized = false;
        }
        
        git__free(entry->textconv_name);
        git__free(entry);
    }
    
    git_vector_free(&textconv_registry.textconvs);
    
    git_rwlock_wrunlock(&textconv_registry.lock);
    git_rwlock_free(&textconv_registry.lock);
}

/* Note: callers must lock the registry before calling this function */
static int textconv_registry_find(size_t *pos, const char *name)
{
    return git_vector_bsearch2(pos, &textconv_registry.textconvs, textconv_entry_name_key_check, name);
}

/* Note: callers must lock the registry before calling this function */
static git_textconv_entry *textconv_registry_lookup(size_t *pos, const char *name)
{
    git_textconv_entry *entry = NULL;
    
    if (!textconv_registry_find(pos, name))
        entry = git_vector_get(&textconv_registry.textconvs, *pos);
    
    return entry;
}


int git_textconv_register(
                        const char *name, git_textconv *textconv)
{
    int error;
    
    assert(name && textconv);
    
    if (git_rwlock_wrlock(&textconv_registry.lock) < 0) {
        giterr_set(GITERR_OS, "failed to lock textconv registry");
        return -1;
    }
    
    if (!textconv_registry_find(NULL, name)) {
        giterr_set(
                   GITERR_TEXTCONV, "attempt to reregister existing textconv '%s'", name);
        error = GIT_EEXISTS;
        goto done;
    }
    
    error = textconv_registry_insert(name, textconv);
    
done:
    git_rwlock_wrunlock(&textconv_registry.lock);
    return error;
}

int git_textconv_unregister(const char *name)
{
    size_t pos;
    git_textconv_entry *entry;
    int error = 0;
    
    assert(name);
    
    if (git_rwlock_wrlock(&textconv_registry.lock) < 0) {
        giterr_set(GITERR_OS, "failed to lock textconv registry");
        return -1;
    }
    
    if ((entry = textconv_registry_lookup(&pos, name)) == NULL) {
        giterr_set(GITERR_TEXTCONV, "cannot find textconv '%s' to unregister", name);
        error = GIT_ENOTFOUND;
        goto done;
    }
    
    git_vector_remove(&textconv_registry.textconvs, pos);
    
    if (entry->initialized && entry->textconv && entry->textconv->shutdown) {
        entry->textconv->shutdown(entry->textconv);
        entry->initialized = false;
    }
    
    git__free(entry->textconv_name);
    git__free(entry);
    
done:
    git_rwlock_wrunlock(&textconv_registry.lock);
    return error;
}

static int textconv_initialize(git_textconv_entry *entry)
{
    int error = 0;
    
    if (!entry->initialized && entry->textconv && entry->textconv->initialize) {
        if ((error = entry->textconv->initialize(entry->textconv)) < 0)
            return error;
    }
    
    entry->initialized = true;
    return 0;
}

git_textconv *git_textconv_lookup(const char *name)
{
    size_t pos;
    git_textconv_entry *entry;
    git_textconv *textconv = NULL;
    
    if (git_rwlock_rdlock(&textconv_registry.lock) < 0) {
        giterr_set(GITERR_OS, "failed to lock textconv registry");
        return NULL;
    }
    
    if ((entry = textconv_registry_lookup(&pos, name)) == NULL ||
        (!entry->initialized && textconv_initialize(entry) < 0))
        goto done;
    
    textconv = entry->textconv;
    
done:
    git_rwlock_rdunlock(&textconv_registry.lock);
    return textconv;
}

void git_textconv_free(git_textconv *textconv)
{
    git__free(textconv);
}



/** Return any textconv applicable to the source */
static int git_textconv_check_attributes(
                                        const char **out,
                                        git_repository *repo,
                                        const char *path)
{
    int error;
    
    error = git_attr_get(out, repo, 0, path, GIT_TEXTCONV_ATTR);
    
    /* if no values were found there is no textconv, return passthrough */
    if (error == GIT_ENOTFOUND) {
        return GIT_PASSTHROUGH;
    }
    return error;
}

int git_textconv_load(git_textconv** out, git_repository* repo, const char* path) {
    int error;
    
    const char* textconv_name;
    error = git_textconv_check_attributes(&textconv_name, repo, path);
    
    /* if no values were found there is no textconv, return passthrough */
    if (error == GIT_PASSTHROUGH) {
        *out = NULL;
    } else {
        *out = git_textconv_lookup(textconv_name);
        if (*out == NULL) error = GIT_ENOTFOUND;
    }
    return error;
}

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

int git_textconv_apply_to_data(
                                  git_buf *tgt, git_textconv *textconv, git_buf *src)
{
    struct buf_stream writer;
    int error;
    
    git_buf_sanitize(tgt);
    git_buf_sanitize(src);
    
    if (!textconv) {
        git_buf_attach_notowned(tgt, src->ptr, src->size);
        return 0;
    }
    
    buf_stream_init(&writer, tgt);
    
    if ((error = git_textconv_stream_data(textconv, src,
                                             &writer.parent)) < 0)
        return error;
    
    assert(writer.complete);
    return error;
}

int git_textconv_apply_to_file(
                                  git_buf *out,
                                  git_textconv *textconv,
                                  git_repository *repo,
                                  const char *path)
{
    struct buf_stream writer;
    int error;
    
    buf_stream_init(&writer, out);
    
    if ((error = git_textconv_stream_file(textconv, repo, path, &writer.parent)) < 0)
        return error;
    
    assert(writer.complete);
    return error;
}

static int buf_from_blob(git_buf *out, git_blob *blob)
{
    git_off_t rawsize = git_blob_rawsize(blob);
    
    if (!git__is_sizet(rawsize)) {
        giterr_set(GITERR_OS, "blob is too large to textconv");
        return -1;
    }
    
    git_buf_attach_notowned(out, git_blob_rawcontent(blob), (size_t)rawsize);
    return 0;
}

int git_textconv_apply_to_blob(
                                  git_buf *out,
                                  git_textconv *textconv,
                                  git_blob *blob)
{
    struct buf_stream writer;
    int error;
    
    buf_stream_init(&writer, out);
    
    if ((error = git_textconv_stream_blob(textconv, blob, &writer.parent)) < 0)
        return error;
    
    assert(writer.complete);
    return error;
}

struct proxy_stream {
    git_writestream parent;
    git_textconv *textconv;
    git_buf input;
    git_buf temp_buf;
    git_buf *output;
    git_writestream *target;
};

static int proxy_stream_write(
                              git_writestream *s, const char *buffer, size_t len)
{
    struct proxy_stream *proxy_stream = (struct proxy_stream *)s;
    assert(proxy_stream);
    
    return git_buf_put(&proxy_stream->input, buffer, len);
}

static int proxy_stream_close(git_writestream *s)
{
    struct proxy_stream *proxy_stream = (struct proxy_stream *)s;
    git_buf *writebuf;
    int error;
    
    assert(proxy_stream);
    
    error = proxy_stream->textconv->apply(
                                        proxy_stream->textconv,
                                        proxy_stream->output,
                                        &proxy_stream->input);
    
    if (error == GIT_PASSTHROUGH) {
        writebuf = &proxy_stream->input;
    } else if (error == 0) {
        git_buf_sanitize(proxy_stream->output);
        writebuf = proxy_stream->output;
    } else {
        return error;
    }
    
    if ((error = proxy_stream->target->write(
                                             proxy_stream->target, writebuf->ptr, writebuf->size)) == 0)
        error = proxy_stream->target->close(proxy_stream->target);
    
    return error;
}

static void proxy_stream_free(git_writestream *s)
{
    struct proxy_stream *proxy_stream = (struct proxy_stream *)s;
    assert(proxy_stream);
    
    git_buf_free(&proxy_stream->input);
    git_buf_free(&proxy_stream->temp_buf);
    git__free(proxy_stream);
}

static int proxy_stream_init(
                             git_writestream **out,
                             git_textconv *textconv,
                             git_buf *temp_buf,
                             git_writestream *target)
{
    struct proxy_stream *proxy_stream = git__calloc(1, sizeof(struct proxy_stream));
    GITERR_CHECK_ALLOC(proxy_stream);
    
    proxy_stream->parent.write = proxy_stream_write;
    proxy_stream->parent.close = proxy_stream_close;
    proxy_stream->parent.free = proxy_stream_free;
    proxy_stream->textconv = textconv;
    proxy_stream->target = target;
    proxy_stream->output = temp_buf ? temp_buf : &proxy_stream->temp_buf;
    
    if (temp_buf)
        git_buf_clear(temp_buf);
    
    *out = (git_writestream *)proxy_stream;
    return 0;
}

static int stream_init(
                            git_writestream **out,
                            git_textconv *textconv,
                            git_buf *temp_buf,
                            git_writestream *target)
{
    int error = 0;
    
    *out = NULL;
    
    if (!textconv) {
        *out = target;
        return 0;
    }
    
    git_writestream *textconv_stream;
        
    assert(textconv->stream || textconv->apply);
        
    /* If necessary, create a stream that proxies the traditional
     * application.
     */
    if (textconv->stream)
        error = textconv->stream(&textconv_stream, textconv, target);
    else
    /* Create a stream that proxies the one-shot apply */
        error = proxy_stream_init(&textconv_stream, textconv, temp_buf, target);
        
    if (error)
        target->close(target);
    else
        *out = textconv_stream;
    
    return error;
}



int git_textconv_stream_file(
                                git_textconv *textconv,
                                git_repository *repo,
                                const char *path,
                                git_writestream *target)
{
    char buf[TEXTCONV_IO_BUFSIZE];
    git_buf abspath = GIT_BUF_INIT;
    git_buf tmp_buf = GIT_BUF_INIT;
    const char *base = repo ? git_repository_workdir(repo) : NULL;
    git_writestream *stream_start;
    ssize_t readlen;
    int fd = -1, error, initialized = 0;
    
    if ((error = stream_init(&stream_start, textconv, &tmp_buf, target)) < 0 ||
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
    stream_start->free(stream_start);
    git_buf_free(&abspath);
    git_buf_free(&tmp_buf);
    return error;
}

int git_textconv_stream_data(
                                git_textconv *textconv,
                                git_buf *data,
                                git_writestream *target)
{
    git_writestream *stream_start;
    int error, initialized = 0;
    git_buf tmp_buf = GIT_BUF_INIT;

    git_buf_sanitize(data);
    
    if ((error = stream_init(&stream_start, textconv, &tmp_buf, target)) < 0)
        goto out;
    initialized = 1;
    
    if ((error = stream_start->write(
                                     stream_start, data->ptr, data->size)) < 0)
        goto out;
    
out:
    if (initialized)
        error |= stream_start->close(stream_start);
    
    stream_start->free(stream_start);
    git_buf_free(&tmp_buf);
    return error;
}

int git_textconv_stream_blob(
                                git_textconv *textconv,
                                git_blob *blob,
                                git_writestream *target)
{
    git_buf in = GIT_BUF_INIT;

    if (buf_from_blob(&in, blob) < 0)
        return -1;
    
    
    return git_textconv_stream_data(textconv, &in, target);
}

int git_textconv_init(git_textconv *textconv, unsigned int version)
{
    GIT_INIT_STRUCTURE_FROM_TEMPLATE(textconv, version, git_textconv, GIT_TEXTCONV_INIT);
    return 0;
}

