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

#define GIT_HIREDIS_BACKEND

//#ifdef GIT2_HIREDIS_BACKEND

#include <hiredis/hiredis.h>

#define GIT2_REDIS_PREFIX "git2:"
#define GIT2_REDIS_PREFIX_LEN 5

typedef struct {
    git_odb_backend parent;

    redisContext *db;
} hiredis_backend;


char* gen_key(const char* key){
    char *redis_key;
    int prefix_len = strlen(GIT2_REDIS_PREFIX);
    redis_key = (char*)git__malloc(prefix_len + strlen(key));
    strncpy(redis_key, GIT2_REDIS_PREFIX, prefix_len);
    strncpy(redis_key + prefix_len, key, 41);
    return redis_key;
}

int hiredis_backend__read_header(size_t *len_p, git_otype *type_p, git_odb_backend *_backend, const git_oid *oid) {
    hiredis_backend *backend;
    int error;
    char *key;
    int j;
    redisReply *reply;
    
    assert(len_p && type_p && _backend && oid);
    
    key = gen_key(oid);
    if (key == NULL){
        return GIT_ENOMEM;
    }
  
    backend = (hiredis_backend *)_backend;
    error = GIT_ERROR;
    
    reply = redisCommand(backend->db, "HMGET %s %s %s", key, "type", "size");
    assert(reply->type != REDIS_REPLY_ERROR);
    
    if (reply->type == REDIS_REPLY_ARRAY){
        error = GIT_SUCCESS;
        *type_p = (git_otype)atoi(reply->element[0]->str);
	*len_p = (size_t)atoi(reply->element[1]->str);
    } else if (reply->type == REDIS_REPLY_NIL){
        error = GIT_ENOTFOUND;
    } else {
        error = GIT_ERROR;
    }
    
    free(key);
    freeReplyObject(reply);
    return error;
}

int hiredis_backend__read(void **data_p, size_t *len_p, git_otype *type_p, git_odb_backend *_backend, const git_oid *oid) {
    hiredis_backend *backend;
    int error;
    redisReply *reply;
    
    assert(data_p && len_p && type_p && _backend && oid);

    char key[40];
    git_oid_fmt(key, oid);
    char* redis_key = gen_key(key);
  
    backend = (hiredis_backend *)_backend;
    error = GIT_ERROR;
    
    reply = redisCommand(backend->db, "HMGET %s %s %s %s", redis_key, "type", "size", "data");
    assert(reply->type != REDIS_REPLY_ERROR);
    if (reply->type == REDIS_REPLY_ARRAY){
        *type_p = (git_otype)atoll(reply->element[0]->str);
        *len_p = (size_t)atoll(reply->element[1]->str);
        *data_p = git__malloc(*len_p);
        if (*data_p == NULL) {
                error = GIT_ENOMEM;
	} else {
                memcpy(*data_p, reply->element[2]->str, *len_p);
		error = GIT_SUCCESS;
	}
    } else if (reply->type == REDIS_REPLY_NIL){
        error = GIT_ENOTFOUND;
    } else {
        error = GIT_ERROR;
    }
    
    free(redis_key);
    freeReplyObject(reply);
    return GIT_SUCCESS;
}

int hiredis_backend__exists(git_odb_backend *_backend, const git_oid *oid) {
    
    return GIT_ERROR;
}

int hiredis_backend__write(git_oid *id, git_odb_backend *_backend, const void *data, size_t len, git_otype type) {
    hiredis_backend *backend;
    int error;
    redisReply *reply;
    
    assert(id && _backend && data);


  
    backend = (hiredis_backend *)_backend;
    error = GIT_ERROR;
    
    if ((error = git_odb_hash(id, data, len, type)) < 0)
	return error;

    char key[40];
    git_oid_fmt(key, id);
    char* redis_key = gen_key(key);
    //TODO: convert data to base64
    reply = redisCommand(backend->db, "HMSET %s "
                                      "type %d "
                                      "size %d "
                                      "data %b ", redis_key, (int)type, len, data, len);
    error = reply->type == REDIS_REPLY_ERROR ? GIT_ERROR : GIT_SUCCESS;
    
    free(redis_key);
    freeReplyObject(reply);
    return error;
}

void hiredis_backend__free(git_odb_backend *_backend) {
    return GIT_SUCCESS;
}

int git_odb_backend_hiredis(git_odb_backend **backend_out, const char *host, int port) {
        hiredis_backend *backend;
        int error;

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

	*backend_out = (git_odb_backend *)backend;
        
	return GIT_SUCCESS;
cleanup:
        return GIT_ERROR;
}

//#else

int g2it_odb_backend_hiredis(git_odb_backend ** GIT_UNUSED(backend_out),
            const char *GIT_UNUSED(host), int GIT_UNUSED(port)) {
        GIT_UNUSED_ARG(backend_out);
        GIT_UNUSED_ARG(host);
        GIT_UNUSED_ARG(port);
        return GIT_ENOTIMPLEMENTED;
}


//#endif /* HAVE_HIREDIS */

