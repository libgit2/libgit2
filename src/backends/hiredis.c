/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "common.h"
#include "git2/object.h"
#include "hash.h"
#include "odb.h"

#include "git2/odb_backend.h"

#ifdef GIT2_HIREDIS_BACKEND

#include <hiredis/hiredis.h>

typedef struct {
    git_odb_backend parent;

    redisContext *db;
} hiredis_backend;

int hiredis_backend__read_header(size_t *len_p, git_otype *type_p, git_odb_backend *_backend, const git_oid *oid) {
    hiredis_backend *backend;
    int error;
    redisReply *reply;

    assert(len_p && type_p && _backend && oid);

    backend = (hiredis_backend *) _backend;
    error = GIT_ERROR;

    reply = redisCommand(backend->db, "HMGET %b %s %s", oid->id, GIT_OID_RAWSZ,
            "type", "size");

    if (reply->type == REDIS_REPLY_ARRAY) {
        if (reply->element[0]->type != REDIS_REPLY_NIL &&
                reply->element[0]->type != REDIS_REPLY_NIL) {
            *type_p = (git_otype) atoi(reply->element[0]->str);
            *len_p = (size_t) atoi(reply->element[1]->str);
            error = GIT_SUCCESS;
        } else {
            error = GIT_ENOTFOUND;
        }
    } else {
        error = GIT_ERROR;
    }

    freeReplyObject(reply);
    return error;
}

int hiredis_backend__read(void **data_p, size_t *len_p, git_otype *type_p, git_odb_backend *_backend, const git_oid *oid) {
    hiredis_backend *backend;
    int error;
    redisReply *reply;

    assert(data_p && len_p && type_p && _backend && oid);

    backend = (hiredis_backend *) _backend;
    error = GIT_ERROR;

    reply = redisCommand(backend->db, "HMGET %b %s %s %s", oid->id, GIT_OID_RAWSZ,
            "type", "size", "data");

    if (reply->type == REDIS_REPLY_ARRAY) {
        if (reply->element[0]->type != REDIS_REPLY_NIL &&
                reply->element[1]->type != REDIS_REPLY_NIL &&
                reply->element[2]->type != REDIS_REPLY_NIL) {
            *type_p = (git_otype) atoi(reply->element[0]->str);
            *len_p = (size_t) atoi(reply->element[1]->str);
            *data_p = git__malloc(*len_p);
            if (*data_p == NULL) {
                error = GIT_ENOMEM;
            } else {
                memcpy(*data_p, reply->element[2]->str, *len_p);
                error = GIT_SUCCESS;
            }
        } else {
            error = GIT_ENOTFOUND;
        }
    } else {
        error = GIT_ERROR;
    }

    freeReplyObject(reply);
    return error;
}

int hiredis_backend__exists(git_odb_backend *_backend, const git_oid *oid) {
    hiredis_backend *backend;
    int found;
    redisReply *reply;

    assert(_backend && oid);

    backend = (hiredis_backend *) _backend;
    found = 0;

    reply = redisCommand(backend->db, "exists %b", oid->id, GIT_OID_RAWSZ);
    if (reply->type != REDIS_REPLY_NIL && reply->type != REDIS_REPLY_ERROR)
        found = 1;


    freeReplyObject(reply);
    return found;
}

int hiredis_backend__write(git_oid *id, git_odb_backend *_backend, const void *data, size_t len, git_otype type) {
    hiredis_backend *backend;
    int error;
    redisReply *reply;

    assert(id && _backend && data);

    backend = (hiredis_backend *) _backend;
    error = GIT_ERROR;

    if ((error = git_odb_hash(id, data, len, type)) < 0)
        return error;

    reply = redisCommand(backend->db, "HMSET %b "
            "type %d "
            "size %d "
            "data %b ", id->id, GIT_OID_RAWSZ,
            (int) type, len, data, len);
    error = reply->type == REDIS_REPLY_ERROR ? GIT_ERROR : GIT_SUCCESS;

    freeReplyObject(reply);
    return error;
}

void hiredis_backend__free(git_odb_backend *_backend) {
    hiredis_backend *backend;
    assert(_backend);
    backend = (hiredis_backend *) _backend;

    redisFree(backend->db);

    free(backend);
}

int git_odb_backend_hiredis(git_odb_backend **backend_out, const char *host, int port) {
    hiredis_backend *backend;

    backend = git__calloc(1, sizeof (hiredis_backend));
    if (backend == NULL)
        return GIT_ENOMEM;


    backend->db = redisConnect(host, port);
    if (backend->db->err)
        goto cleanup;

    backend->parent.read = &hiredis_backend__read;
    backend->parent.read_header = &hiredis_backend__read_header;
    backend->parent.write = &hiredis_backend__write;
    backend->parent.exists = &hiredis_backend__exists;
    backend->parent.free = &hiredis_backend__free;

    *backend_out = (git_odb_backend *) backend;

    return GIT_SUCCESS;
cleanup:
    free(backend);
    return GIT_ERROR;
}

#else

int git_odb_backend_hiredis(git_odb_backend ** GIT_UNUSED(backend_out),
        const char *GIT_UNUSED(host), int GIT_UNUSED(port)) {
    GIT_UNUSED_ARG(backend_out);
    GIT_UNUSED_ARG(host);
    GIT_UNUSED_ARG(port);
    return GIT_ENOTIMPLEMENTED;
}


#endif /* HAVE_HIREDIS */
