#include <stdlib.h>
#include <string.h>

#include "git2/mariadb.h"
#include "git2/oid.h"
#include "git2/refs.h"
#include "git2/sys/refs.h"

#include "error.h"
#include "fnmatch.h"


#define GIT2_STORAGE_ENGINE "XtraDB"
#define MAX_QUERY_LEN 1024 /* without the values */


#define MAX_REFNAME_LEN    (255)
#define MAX_REFNAME_LEN_STR    "255"


#define SQL_CREATE \
    "CREATE TABLE IF NOT EXISTS `%s` (" /* %s = table name */ \
    "  `repository_id` INTEGER UNSIGNED NOT NULL," \
    "  `refname` VARCHAR(" MAX_REFNAME_LEN_STR ") NOT NULL," \
    "  `target_oid` binary(20) NULL," \
    "  `target_symbolic` VARCHAR(" MAX_REFNAME_LEN_STR ") NULL," \
    "  `peel_oid` binary(20) NULL," \
    "  PRIMARY KEY (`repository_id`, `refname`)" \
    ") ENGINE=" GIT2_STORAGE_ENGINE \
    " DEFAULT CHARSET=utf8" \
    " COLLATE=utf8_bin" \
    " PARTITION BY KEY(`repository_id`)" \
    " PARTITIONS %d" \
    ";"


#define SQL_EXISTS \
    "SELECT refname FROM `%s`" /* %s = table name */ \
    " WHERE `repository_id` = ? AND `refname` = ?" \
    " LIMIT 1;"


#define SQL_LOOKUP \
    "SELECT `target_oid`, `target_symbolic`, `peel_oid`" \
    " FROM `%s`" /* %s = table name */ \
    " WHERE `repository_id` = ? AND `refname` = ?" \
    " LIMIT 1;"


/* for the iterator, we have to use the custom p_fnmatch() on each ref
 * so we must go through all of them. Hopefully there won't be too many
 */
#define SQL_ITERATOR \
    "SELECT `target_oid`, `target_symbolic`, `peel_oid`, `refname`" \
    " FROM `%s`" /* %s = table name */ \
    " WHERE `repository_id` = ?;"


/* will automatically fail if the primary key is already used */
#define SQL_WRITE_NO_FORCE \
    "INSERT INTO `%s`" /* %s = table name */ \
    " (`repository_id`, `refname`, `target_oid`, `target_symbolic`," \
    " `peel_oid`)" \
    " VALUES (?, ?, ?, ?, ?);"


/* try to insert ; if there is a primary key conflict, tell it to update
 * the existing entry instead */
#define SQL_WRITE_FORCE \
    "INSERT INTO `%s`" /* %s = table name */ \
    " (`repository_id`, `refname`, `target_oid`, `target_symbolic`," \
    " `peel_oid`)" \
    " VALUES (?, ?, ?, ?, ?)" \
    " ON DUPLICATE KEY" \
    " UPDATE " \
    " `target_oid`=VALUES(`target_oid`)," \
    " `target_symbolic`=VALUES(`target_symbolic`)," \
    " `peel_oid`=VALUES(`peel_oid`);"


#define SQL_RENAME \
    "UPDATE `%s`" /* %s = table name */ \
    " SET `refname`=?" \
    " WHERE `repository_id` = ? AND `refname` = ?" \
    " LIMIT 1;"


#define SQL_DELETE \
    "DELETE FROM `%s`" /* %s = table name */ \
    " WHERE `repository_id` = ? AND `refname` = ?" \
    " LIMIT 1;"


#define SQL_OPTIMIZE \
    "OPTIMIZE TABLE `%s`" /* %s = table name */


typedef struct {
    git_refdb_backend parent;

    MYSQL *db;
    uint32_t repository_id;

    MYSQL_STMT *st_exists;
    MYSQL_STMT *st_lookup;
    MYSQL_STMT *st_iterator;
    MYSQL_STMT *st_write_no_force;
    MYSQL_STMT *st_write_force;
    MYSQL_STMT *st_rename;
    MYSQL_STMT *st_delete;
    MYSQL_STMT *st_optimize;
} mariadb_refdb_backend_t;


typedef struct mariadb_reference_iterator_node_s {
    struct git_reference *reference;
    int must_free_ref;
    struct mariadb_reference_iterator_node_s *next;
} mariadb_reference_iterator_node_t;


typedef struct {
    git_reference_iterator parent;
    mariadb_reference_iterator_node_t *head;
    mariadb_reference_iterator_node_t *current;
} mariadb_reference_iterator_t;


static int mariadb_reference_iterator_next(git_reference **ref,
        git_reference_iterator *iter);
static int mariadb_reference_iterator_next_name(const char **ref_name,
        git_reference_iterator *iter);
static void mariadb_reference_iterator_free(git_reference_iterator *iter);


/*!
 * \brief Template for our mariadb_reference_iterator_t.
 * When creating, we can memcpy() this template, and just fill in the taggued
 * fields.
 */
static const mariadb_reference_iterator_t reference_iterator_template = {
    .parent = {
        .db = NULL,  /* To fill in */
        .next = mariadb_reference_iterator_next,
        .next_name = mariadb_reference_iterator_next_name,
        .free = mariadb_reference_iterator_free,
    },
};


static int mariadb_reference_iterator_next(git_reference **ref,
        git_reference_iterator *_iterator)
{
    mariadb_reference_iterator_t *iterator;

    assert(ref);
    assert(_iterator);

    iterator = (mariadb_reference_iterator_t *)_iterator;

    if (iterator->current == NULL) {
        *ref = NULL;
        return GIT_ITEROVER;
    }

    *ref = iterator->current->reference;
    /* we gave the ref back to the caller --> it takes care of it */
    iterator->current->must_free_ref = 0;

    iterator->current = iterator->current->next;
    return GIT_OK;
}


static int mariadb_reference_iterator_next_name(const char **ref_name,
        git_reference_iterator *_iterator)
{
    mariadb_reference_iterator_t *iterator;

    assert(ref_name);
    assert(_iterator);

    iterator = (mariadb_reference_iterator_t *)_iterator;

    if (iterator->current == NULL) {
        *ref_name = NULL;
        return GIT_ITEROVER;
    }

    *ref_name = git_reference_name(iterator->current->reference);
    /* we are only giving the ref name back --> it's up to us to free the ref */
    iterator->current->must_free_ref = 1;
    iterator->current = iterator->current->next;
    return GIT_OK;
}


static void mariadb_reference_iterator_free(git_reference_iterator *_iterator)
{
    mariadb_reference_iterator_t *iterator;
    mariadb_reference_iterator_node_t *node, *nnode;

    assert(_iterator);

    iterator = (mariadb_reference_iterator_t *)_iterator;

    for (node = iterator->head, nnode = (node != NULL ? node->next : NULL) ;
            node != NULL ;
            node = nnode, nnode = (node != NULL ? node->next : NULL)) {

        assert(node->reference);
        if (node->must_free_ref) {
            git_reference_free(node->reference);
        }
        free(node);

    }

    free(iterator);
}


static int mariadb_refdb_exists(int *exists, git_refdb_backend *_backend,
        const char *refname)
{
    mariadb_refdb_backend_t *backend;
    MYSQL_BIND bind_buffers[2];

    backend = (mariadb_refdb_backend_t *)_backend;

    memset(bind_buffers, 0, sizeof(bind_buffers));

    bind_buffers[0].buffer = &backend->repository_id;
    bind_buffers[0].buffer_type = MYSQL_TYPE_LONG;
    bind_buffers[0].buffer_length = sizeof(backend->repository_id);

    bind_buffers[1].buffer = (void *)refname; /* cast because of 'const' */
    bind_buffers[1].buffer_length = strlen(refname);
    bind_buffers[1].buffer_type = MYSQL_TYPE_STRING;

    if (mysql_stmt_bind_param(backend->st_exists, bind_buffers) != 0) {
        fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_stmt_bind_param() failed: %s",
                __FUNCTION__, __LINE__,
                mysql_error(backend->db));
        return GIT_EUSER;
    }

    /* execute the statement */
    if (mysql_stmt_execute(backend->st_exists) != 0) {
        fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_stmt_execute() failed: %s",
                __FUNCTION__, __LINE__,
                mysql_error(backend->db));
        return GIT_EUSER;
    }

    if (mysql_stmt_store_result(backend->st_exists) != 0) {
        fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_stmt_store_result() failed: %s",
                __FUNCTION__, __LINE__,
                mysql_error(backend->db));
        return GIT_EUSER;
    }

    *exists = (mysql_stmt_num_rows(backend->st_exists) > 0);

    /* reset the statement for further use */
    if (mysql_stmt_reset(backend->st_exists) != 0) {
        fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_stmt_reset() failed: %s",
                __FUNCTION__, __LINE__,
                mysql_error(backend->db));
        return GIT_EUSER;
    }

    return GIT_OK;
}


static int mariadb_refdb_lookup(git_reference **out,
        git_refdb_backend *_backend,
        const char *refname)
{
    mariadb_refdb_backend_t *backend;
    MYSQL_BIND bind_buffers[2];
    MYSQL_BIND result_buffers[3];

    git_oid target_oid;
    char target_symbolic[MAX_REFNAME_LEN + 1];
    git_oid peel_oid;

    int error;

    backend = (mariadb_refdb_backend_t *)_backend;

    *out = NULL;

    memset(bind_buffers, 0, sizeof(bind_buffers));

    bind_buffers[0].buffer_type = MYSQL_TYPE_LONG;
    bind_buffers[0].buffer = &backend->repository_id;
    bind_buffers[0].buffer_length = sizeof(backend->repository_id);

    bind_buffers[1].buffer_type = MYSQL_TYPE_STRING;
    bind_buffers[1].buffer = (void *)refname; /* cast because of 'const' */
    bind_buffers[1].buffer_length = strlen(refname);

    if (mysql_stmt_bind_param(backend->st_lookup, bind_buffers) != 0) {
        fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_stmt_bind_param() failed: %s",
                __FUNCTION__, __LINE__,
                mysql_error(backend->db));
        return GIT_EUSER;
    }

    /* execute the statement */
    if (mysql_stmt_execute(backend->st_lookup) != 0) {
        fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_stmt_execute() failed: %s",
                __FUNCTION__, __LINE__,
                mysql_stmt_error(backend->st_lookup));
        return GIT_EUSER;
    }

    if (mysql_stmt_store_result(backend->st_lookup) != 0) {
        fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_stmt_store_result() failed: %s",
                __FUNCTION__, __LINE__,
                mysql_stmt_error(backend->st_lookup));
        return GIT_EUSER;
    }

    if (mysql_stmt_num_rows(backend->st_lookup) <= 0) {
        fprintf(stderr, "%s : GIT_ENOTFOUND : %s\n", __FUNCTION__, refname);
        error = GIT_ENOTFOUND;
    } else {
        memset(result_buffers, 0, sizeof(result_buffers));

        result_buffers[0].buffer_type = MYSQL_TYPE_BLOB;
        result_buffers[0].buffer = (void*)target_oid.id;
        result_buffers[0].buffer_length = GIT_OID_RAWSZ;
        memset(&target_oid, 0, sizeof(target_oid));

        result_buffers[1].buffer_type = MYSQL_TYPE_STRING;
        result_buffers[1].buffer = target_symbolic;
        result_buffers[1].buffer_length = sizeof(target_symbolic) - 1;
        memset(target_symbolic, 0, sizeof(target_symbolic));

        result_buffers[2].buffer_type = MYSQL_TYPE_BLOB;
        result_buffers[2].buffer = (void*)peel_oid.id;
        result_buffers[2].buffer_length = GIT_OID_RAWSZ;
        memset(&peel_oid, 0, sizeof(peel_oid));

        if(mysql_stmt_bind_result(backend->st_lookup, result_buffers) != 0) {
            fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_stmt_bind_result() failed: %s",
                __FUNCTION__, __LINE__,
                mysql_stmt_error(backend->st_lookup));
            return GIT_EUSER;
        }

        /* this should populate the buffers */
        if (mysql_stmt_fetch(backend->st_lookup) != 0) {
            fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_stmt_fetch() failed: %s",
                __FUNCTION__, __LINE__,
                mysql_stmt_error(backend->st_lookup));
            return GIT_EUSER;
        }

        target_symbolic[sizeof(target_symbolic) - 1] = '\0'; /* safety */

        if (result_buffers[1].buffer_length > 0
                && strlen(target_symbolic) > 0
                && !result_buffers[1].is_null) {
            *out = git_reference__alloc_symbolic(refname, target_symbolic);
        } else {
            assert(result_buffers[0].buffer_length > 0
                    && !result_buffers[0].is_null);

            if (result_buffers[2].buffer_length > 0
                    && !result_buffers[2].is_null)
                *out = git_reference__alloc(refname, &target_oid, &peel_oid);
            else
                *out = git_reference__alloc(refname, &target_oid, NULL);
        }

        error = GIT_OK;
    }

    /* reset the statement for further use */
    if (mysql_stmt_reset(backend->st_lookup) != 0) {
        fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_stmt_reset() failed: %s",
                __FUNCTION__, __LINE__,
                mysql_error(backend->db));
        return GIT_EUSER;
    }

    return error;
}


static int mariadb_refdb_iterator(git_reference_iterator **_iterator,
        struct git_refdb_backend *_backend, const char *glob)
{
    mariadb_refdb_backend_t *backend;
    MYSQL_BIND bind_buffers[1];
    MYSQL_BIND result_buffers[4];
    git_oid target_oid;
    char target_symbolic[MAX_REFNAME_LEN + 1];
    char refname[MAX_REFNAME_LEN + 1];
    git_oid peel_oid;
    int fetch_result;
    mariadb_reference_iterator_t *iterator;
    mariadb_reference_iterator_node_t *current_node = NULL;
    mariadb_reference_iterator_node_t *previous_node = NULL;

    assert(_iterator);
    assert(_backend);

    *_iterator = NULL;
    backend = (mariadb_refdb_backend_t *)_backend;

    memset(bind_buffers, 0, sizeof(bind_buffers));

    bind_buffers[0].buffer_type = MYSQL_TYPE_LONG;
    bind_buffers[0].buffer = &backend->repository_id;
    bind_buffers[0].buffer_length = sizeof(backend->repository_id);

    if (mysql_stmt_bind_param(backend->st_iterator, bind_buffers) != 0) {
        fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_stmt_bind_param() failed: %s",
                __FUNCTION__, __LINE__,
                mysql_error(backend->db));
        return GIT_EUSER;
    }

    if (mysql_stmt_execute(backend->st_iterator) != 0) {
        fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_stmt_execute() failed: %s",
                __FUNCTION__, __LINE__,
                mysql_error(backend->db));
        return GIT_EUSER;
    }

    iterator = calloc(1, sizeof(*iterator));
    if (iterator == NULL) {
        mysql_stmt_reset(backend->st_exists);
        return GIT_EUSER;
    }
    iterator->parent.next = mariadb_reference_iterator_next;
    iterator->parent.next_name = mariadb_reference_iterator_next_name;
    iterator->parent.free = mariadb_reference_iterator_free;

    memset(result_buffers, 0, sizeof(result_buffers));

    result_buffers[0].buffer_type = MYSQL_TYPE_LONG_BLOB;
    result_buffers[0].buffer = &target_oid.id;
    result_buffers[0].buffer_length = sizeof(target_oid.id);

    result_buffers[1].buffer_type = MYSQL_TYPE_STRING;
    result_buffers[1].buffer = target_symbolic;
    result_buffers[1].buffer_length = sizeof(target_symbolic) - 1;

    result_buffers[2].buffer_type = MYSQL_TYPE_LONG_BLOB;
    result_buffers[2].buffer = &peel_oid.id;
    result_buffers[2].buffer_length = sizeof(peel_oid.id);

    result_buffers[3].buffer_type = MYSQL_TYPE_STRING;
    result_buffers[3].buffer = refname;
    result_buffers[3].buffer_length = sizeof(refname) - 1;

    if(mysql_stmt_bind_result(backend->st_iterator, result_buffers) != 0) {
        fprintf(stderr, __FILE__ ": %s: L%d: "
            "mysql_stmt_bind_result() failed: %s",
            __FUNCTION__, __LINE__,
            mysql_error(backend->db));
        mysql_stmt_reset(backend->st_exists);
        mariadb_reference_iterator_free(&iterator->parent);
        return GIT_EUSER;
    }

    /* while there is still row to fetch --> while mysql_stmt_fetch()
     * returns no error.
     */
    while( (fetch_result = mysql_stmt_fetch(backend->st_iterator)) == 0 ) {
        if (glob != NULL && p_fnmatch(glob, refname, 0) != 0)
            continue;

        current_node = calloc(1, sizeof(*current_node));
        if (current_node == NULL) {
            mariadb_reference_iterator_free(&iterator->parent);
            fprintf(stderr, "out of memory");
            mysql_stmt_reset(backend->st_exists);
            return GIT_EUSER;
        }
        current_node->must_free_ref = 1; /* default */

        if (result_buffers[1].buffer_length > 0
                && strlen(target_symbolic) > 0
                && !result_buffers[1].is_null) {
            current_node->reference = \
                git_reference__alloc_symbolic(refname, target_symbolic);
        } else {
            assert(result_buffers[0].buffer_length > 0
                    && !result_buffers[0].is_null);
            if (result_buffers[2].buffer_length > 0
                    && !result_buffers[2].is_null)
                current_node->reference = \
                    git_reference__alloc(refname, &target_oid, &peel_oid);
            else
                current_node->reference = \
                    git_reference__alloc(refname, &target_oid, NULL);
        }

        if (previous_node == NULL) {
            iterator->head = current_node;
            iterator->current = current_node;
        } else {
            previous_node->next = current_node;
        }
        previous_node = current_node;
    }

    if (fetch_result != MYSQL_NO_DATA) {
        /* an error occurred */
        fprintf(stderr, __FILE__ ": %s: L%d: "
            "mysql_stmt_fetch() failed: %s",
            __FUNCTION__, __LINE__,
            mysql_stmt_error(backend->st_iterator));
        mysql_stmt_reset(backend->st_iterator);
        mariadb_reference_iterator_free(&iterator->parent);
        return GIT_EUSER;
    }

    /* reset the statement for further use */
    if (mysql_stmt_reset(backend->st_iterator) != 0) {
        fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_stmt_reset() failed: %s",
                __FUNCTION__, __LINE__,
                mysql_error(backend->db));
        /* the next one may fail, but meh, we got our data for now */
    }

    *_iterator = &iterator->parent;
    return GIT_OK;
}


/*!
 * \warning assumes len(bind_buffers) >= 3 !
 */
static int _bind_ref_values(MYSQL_BIND *bind_buffers, const git_reference *ref)
{
    git_ref_t ref_type;
    const git_oid *ref_oid_target;
    const git_oid *ref_target_peel;
    const char *ref_symbolic_target;

    ref_type = git_reference_type(ref);

    ref_oid_target = NULL;
    ref_symbolic_target = NULL;

    ref_target_peel = git_reference_target_peel(ref);

    switch(ref_type) {
        case GIT_REF_OID:
            ref_oid_target = git_reference_target(ref);
            break;
        case GIT_REF_SYMBOLIC:
            ref_symbolic_target = git_reference_symbolic_target(ref);
            break;
        case GIT_REF_INVALID: /* BREAKTHROUGH */
        case GIT_REF_LISTALL:
            assert(ref_type != GIT_REF_INVALID);
            assert(ref_type != GIT_REF_LISTALL);
            fprintf(stderr, __FILE__ ": %s: L%d: "
                "invalid ref. Cannot insert",
                __FUNCTION__, __LINE__);
            return GIT_EUSER;
    }

    switch(ref_type)
    {
        case GIT_REF_OID:
            bind_buffers[0].buffer_type = MYSQL_TYPE_BLOB;
            /* cast because of const */
            bind_buffers[0].buffer = (void *)ref_oid_target;
            bind_buffers[0].buffer_length = sizeof(*ref_oid_target);

            bind_buffers[1].buffer_type = MYSQL_TYPE_NULL;
            bind_buffers[1].buffer = NULL;
            bind_buffers[1].buffer_length = 0;
            break;

        case GIT_REF_SYMBOLIC:
            bind_buffers[0].buffer_type = MYSQL_TYPE_NULL;
            bind_buffers[0].buffer = NULL;
            bind_buffers[0].buffer_length = 0;

            bind_buffers[1].buffer_type = MYSQL_TYPE_STRING;
            /* case because of const */
            bind_buffers[1].buffer = (void *)ref_symbolic_target;
            bind_buffers[1].buffer_length = strlen(ref_symbolic_target);
            break;

        case GIT_REF_LISTALL: /* BREAKTHROUGH */
        case GIT_REF_INVALID:
            assert(ref_type != GIT_REF_LISTALL);
            assert(ref_type != GIT_REF_INVALID);
            fprintf(stderr, __FILE__ ": %s: L%d: "
                "invalid ref. Cannot insert",
                __FUNCTION__, __LINE__);
            return GIT_EUSER;
    }

    if (ref_target_peel == NULL || git_oid_iszero(ref_target_peel)) {
        bind_buffers[2].buffer_type = MYSQL_TYPE_NULL;
        bind_buffers[2].buffer = NULL;
        bind_buffers[2].buffer_length = 0;
    } else {
        bind_buffers[2].buffer_type = MYSQL_TYPE_BLOB;
        /* cast because of const */
        bind_buffers[2].buffer = (void *)ref_target_peel;
        bind_buffers[2].buffer_length = sizeof(*ref_target_peel);
    }

    return GIT_OK;
}


/*!
 * \param ref ref to add to the db
 * \param force if TRUE (1), smash any previously ref with the same name ;
 *   if FALSE (0), fail if there is already a ref with this name
 * \param who used for reflog ; ignored in this implementation
 * \param message used for reflog ; ignore in this implementation
 * \param old used for reflog ; ignored in this implementation
 * \param old_target used for reflog ; ignored in this implementation
 */
static int mariadb_refdb_write(git_refdb_backend *_backend,
        const git_reference *ref, int force,
        const git_signature *who, const char *message,
        const git_oid *old, const char *old_target)
{
    mariadb_refdb_backend_t *backend;
    MYSQL_BIND bind_buffers[5];
    my_ulonglong affected_rows;
    MYSQL_STMT *sql_statement;
    const char *ref_name;

    who = who; /* -Wunused-parameter */
    message = message; /* -Wunused-parameter */
    old = old; /* -Wunused-parameter */
    old_target = old_target; /* -Wunused-parameter */

    assert(_backend);
    assert(ref);

    backend = (mariadb_refdb_backend_t *)_backend;

    ref_name = git_reference_name(ref);

    memset(bind_buffers, 0, sizeof(bind_buffers));

    bind_buffers[0].buffer_type = MYSQL_TYPE_LONG;
    bind_buffers[0].buffer = &backend->repository_id;
    bind_buffers[0].buffer_length = sizeof(backend->repository_id);

    bind_buffers[1].buffer_type = MYSQL_TYPE_STRING;
    bind_buffers[1].buffer = (void *)ref_name; /* cast because of 'const' */
    bind_buffers[1].buffer_length = strlen(ref_name);

    if (_bind_ref_values(bind_buffers + 2, ref) != GIT_OK)
        return GIT_EUSER;

    sql_statement = (force
            ? backend->st_write_force
            : backend->st_write_no_force);

    if (mysql_stmt_bind_param(sql_statement, bind_buffers) != 0) {
        fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_stmt_bind_param() failed: %s",
                __FUNCTION__, __LINE__,
                mysql_error(backend->db));
        return GIT_EUSER;
    }

    /* execute the statement */
    if (mysql_stmt_execute(sql_statement) != 0) {
        fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_stmt_execute() failed: %s (force ? %d)",
                __FUNCTION__, __LINE__,
                mysql_error(backend->db), force);
        mysql_stmt_reset(sql_statement);
        if (!force) {
            /* see if an existing ref messed things up */
            int err, exists = 0;
            err = mariadb_refdb_exists(&exists, _backend, ref_name);
            if (err == 0 && exists) {
                return GIT_EEXISTS;
            }
        }
        return GIT_EUSER;
    }

    /* now lets see if the insert worked */
    affected_rows = mysql_stmt_affected_rows(sql_statement);
    if ((!force && affected_rows != 1)
            || (force && affected_rows > 2)) {
        fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_stmt_affected_rows() failed: %lld, %s (force ? %d)",
                __FUNCTION__, __LINE__, affected_rows,
                mysql_stmt_error(sql_statement), force);
        return GIT_EUSER;
    }

    /* reset the statement for further use */
    if (mysql_stmt_reset(sql_statement) != 0) {
        fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_stmt_reset() failed: %s",
                __FUNCTION__, __LINE__,
                mysql_error(backend->db));
        return GIT_EUSER;
    }

    return GIT_OK;
}

static int mariadb_refdb_del(git_refdb_backend *backend, const char *ref_name,
        const git_oid *old_id, const char *old_target);

static int mariadb_refdb_rename(git_reference **out,
        git_refdb_backend *_backend,
        const char *old_name, const char *new_name, int force,
        const git_signature *who, const char *message)
{
    mariadb_refdb_backend_t *backend;
    MYSQL_BIND bind_buffers[3];
    my_ulonglong affected_rows;

    who = who; /* -Wunused-parameter */
    message = message; /* -Wunused-parameter */

    assert(_backend);
    assert(old_name);
    assert(new_name);

    *out = NULL;
    backend = (mariadb_refdb_backend_t *)_backend;

    if (force) {
        /* smash existing reference having the name 'new_name' */

        int exists = 0;

        if (mariadb_refdb_exists(&exists, _backend, new_name) != GIT_OK) {
            /* it already set a python exception for us */
            return GIT_EUSER;
        }

        if (exists
                && (mariadb_refdb_del(_backend, new_name, NULL, NULL)
                    != GIT_OK)) {
            /* it already set a python exception for us */
            return GIT_EUSER;
        }
    }

    memset(bind_buffers, 0, sizeof(bind_buffers));

    bind_buffers[0].buffer_type = MYSQL_TYPE_STRING;
    bind_buffers[0].buffer = (void *)new_name; /* cast because of 'const' */
    bind_buffers[0].buffer_length = strlen(new_name);

    bind_buffers[1].buffer_type = MYSQL_TYPE_LONG;
    bind_buffers[1].buffer = &backend->repository_id;
    bind_buffers[1].buffer_length = sizeof(backend->repository_id);

    bind_buffers[2].buffer_type = MYSQL_TYPE_STRING;
    bind_buffers[2].buffer = (void *)old_name; /* cast because of 'const' */
    bind_buffers[2].buffer_length = strlen(old_name);

    if (mysql_stmt_bind_param(backend->st_rename, bind_buffers) != 0) {
        fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_stmt_bind_param() failed: %s",
                __FUNCTION__, __LINE__,
                mysql_stmt_error(backend->st_rename));
        return GIT_EUSER;
    }

    /* execute the statement */
    if (mysql_stmt_execute(backend->st_rename) != 0) {
        fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_stmt_execute() failed: %s",
                __FUNCTION__, __LINE__,
                mysql_stmt_error(backend->st_rename));
        return GIT_EUSER;
    }

    /* now lets see if the update worked */
    affected_rows = mysql_stmt_affected_rows(backend->st_rename);
    if (affected_rows != 1) {
        mysql_stmt_reset(backend->st_rename);
        fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_stmt_affected_rows() failed: %s, %lld (force ? %d)",
                __FUNCTION__, __LINE__,
                mysql_stmt_error(backend->st_rename), affected_rows, force);
        if (affected_rows == 0) {
            fprintf(stderr, "%s : GIT_ENOTFOUND : %s : %s\n",
                __FUNCTION__, old_name, new_name);
            return GIT_ENOTFOUND;
        }
        return GIT_EUSER;
    }

    /* reset the statement for further use */
    if (mysql_stmt_reset(backend->st_rename) != 0) {
        fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_stmt_reset() failed: %s",
                __FUNCTION__, __LINE__,
                mysql_error(backend->db));
        return GIT_EUSER;
    }

    return mariadb_refdb_lookup(out, _backend, new_name);
}


static int mariadb_refdb_del(git_refdb_backend *_backend, const char *ref_name,
        const git_oid *old_id, const char *old_target)
{
    /* XXX(JFlesch):
     * refdb_fs check old_id and old_target before deleting the ref.
     * but we are crazy daredevils, so we don't.
     */

    mariadb_refdb_backend_t *backend;
    MYSQL_BIND bind_buffers[2];
    my_ulonglong affected_rows;

    old_id = old_id; /* -Wunused-parameter */
    old_target = old_target; /* -Wunused-parameter */

    assert(_backend);
    assert(ref_name);

    backend = (mariadb_refdb_backend_t *)_backend;

    memset(bind_buffers, 0, sizeof(bind_buffers));

    bind_buffers[0].buffer_type = MYSQL_TYPE_LONG;
    bind_buffers[0].buffer = &backend->repository_id;
    bind_buffers[0].buffer_length = sizeof(backend->repository_id);

    bind_buffers[1].buffer_type = MYSQL_TYPE_STRING;
    bind_buffers[1].buffer = (void *)ref_name; /* cast because of 'const' */
    bind_buffers[1].buffer_length = strlen(ref_name);

    if (mysql_stmt_bind_param(backend->st_delete, bind_buffers) != 0) {
        fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_stmt_bind_param() failed: %s",
                __FUNCTION__, __LINE__,
                mysql_error(backend->db));
        return GIT_EUSER;
    }

    /* execute the statement */
    if (mysql_stmt_execute(backend->st_delete) != 0) {
        fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_stmt_execute() failed: %s",
                __FUNCTION__, __LINE__,
                mysql_error(backend->db));
        return GIT_EUSER;
    }

    /* now lets see if the delete worked */
    affected_rows = mysql_stmt_affected_rows(backend->st_delete);
    if (affected_rows != 1) {
        fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_stmt_affected_rows() failed: %s, %lld",
                __FUNCTION__, __LINE__,
                mysql_error(backend->db), affected_rows);
        return GIT_ENOTFOUND;
    }

    /* reset the statement for further use */
    if (mysql_stmt_reset(backend->st_delete) != 0) {
        fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_stmt_reset() failed: %s",
                __FUNCTION__, __LINE__,
                mysql_error(backend->db));
        return GIT_EUSER;
    }

    return GIT_OK;
}


static int mariadb_refdb_compress(git_refdb_backend *_backend)
{
    mariadb_refdb_backend_t *backend;

    backend = (mariadb_refdb_backend_t *)_backend;

    if (mysql_stmt_execute(backend->st_optimize) != 0) {
        fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_stmt_execute() failed: %s",
                __FUNCTION__, __LINE__,
                mysql_error(backend->db));
        return GIT_EUSER;
    }

    if (mysql_stmt_reset(backend->st_optimize) != 0) {
        fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_stmt_reset() failed: %s",
                __FUNCTION__, __LINE__,
                mysql_error(backend->db));
        return GIT_EUSER;
    }

    return GIT_OK;
}


static int mariadb_refdb_lock(void **payload_out, git_refdb_backend *backend,
        const char *refname)
{
    /* Meh, who needs locking ? :P */
    payload_out = payload_out; /* -Wunused-parameter */
    backend = backend; /* -Wunused-parameter */
    refname = refname; /* -Wunused-parameter */
    return GIT_OK;
}


static int mariadb_refdb_unlock(git_refdb_backend *backend, void *payload,
        int success, int update_reflog, const git_reference *ref,
        const git_signature *sig, const char *message)
{
    /* Meh, who needs locking ? :P */
    backend = backend; /* -Wunused-parameter */
    payload = payload; /* -Wunused-parameter */
    success = success; /* -Wunused-parameter */
    update_reflog = update_reflog; /* -Wunused-parameter */
    ref = ref; /* -Wunused-parameter */
    sig = sig; /* -Wunused-parameter */
    message = message; /* -Wunused-parameter */
    return GIT_OK;
}


static void mariadb_refdb_free(git_refdb_backend *_backend)
{
    mariadb_refdb_backend_t *backend;

    backend = (mariadb_refdb_backend_t *)_backend;

    if (backend->st_exists)
        mysql_stmt_close(backend->st_exists);
    if (backend->st_lookup)
        mysql_stmt_close(backend->st_lookup);
    if (backend->st_iterator)
        mysql_stmt_close(backend->st_iterator);
    if (backend->st_write_no_force)
        mysql_stmt_close(backend->st_write_no_force);
    if (backend->st_write_force)
        mysql_stmt_close(backend->st_write_force);
    if (backend->st_rename)
        mysql_stmt_close(backend->st_rename);
    if (backend->st_delete)
        mysql_stmt_close(backend->st_delete);
    if (backend->st_optimize)
        mysql_stmt_close(backend->st_optimize);

    free(backend);
}


static int mariadb_refdb_has_log(git_refdb_backend *backend,
        const char *refname)
{
    /* We don't use reflogs, so we never have one */
    backend = backend; /* -Wunused-parameter */
    refname = refname; /* -Wunused-parameter */
    return 0;
}


static int mariadb_refdb_ensure_log(git_refdb_backend *backend,
        const char *refname)
{
    /* We don't use reflogs */
    backend = backend; /* -Wunused-parameter */
    refname = refname; /* -Wunused-parameter */
    return GIT_OK;
}


static int mariadb_refdb_reflog_read(git_reflog **out,
        git_refdb_backend *backend, const char *refname)
{
    /* We don't use reflogs */
    out = out; /* -Wunused-parameter */
    backend = backend; /* -Wunused-parameter */
    refname = refname; /* -Wunused-parameter */
    return GIT_EUSER;
}


static int mariadb_refdb_reflog_write(git_refdb_backend *backend,
        git_reflog *reflog)
{
    /* We don't use reflogs */
    backend = backend; /* -Wunused-parameter */
    reflog = reflog; /* -Wunused-parameter */
    return GIT_OK;
}


static int mariadb_refdb_reflog_rename(git_refdb_backend *backend,
        const char *old_name, const char *new_name)
{
    /* We don't use reflogs */
    backend = backend; /* -Wunused-parameter */
    old_name = old_name; /* -Wunused-parameter */
    new_name = new_name; /* -Wunused-parameter */
    return GIT_OK;
}


static int mariadb_refdb_reflog_delete(git_refdb_backend *backend,
        const char *refname)
{
    /* We don't use reflogs */
    backend = backend; /* -Wunused-parameter */
    refname = refname; /* -Wunused-parameter */
    return GIT_OK;
}


static int init_db(MYSQL *db, const char *table_name, int refdb_partitions)
{
    char sql_create[MAX_QUERY_LEN];

    snprintf(sql_create, sizeof(sql_create), SQL_CREATE, table_name,
        refdb_partitions);

    if (mysql_real_query(db, sql_create, strlen(sql_create)) != 0) {
        fprintf(stderr, __FILE__ ": %s: L%d: "
                "mysql_real_query() failed: %s",
                __FUNCTION__, __LINE__,
                mysql_error(db));
        return GIT_EUSER;
    }

    return GIT_OK;
}


static int init_statement(MYSQL *db,
    const char *sql_query_short_name,
    const char *sql_statement,
    const char *mysql_table,
    MYSQL_STMT **statement)
{
    my_bool truth = 1;
    char sql_query[MAX_QUERY_LEN];

    snprintf(sql_query, sizeof(sql_query), sql_statement, mysql_table);

    *statement = mysql_stmt_init(db);
    if (*statement == NULL) {
        fprintf(stderr, __FILE__ ": mysql_stmt_init() failed");
        return GIT_EUSER;
    }

    if (mysql_stmt_attr_set(*statement, STMT_ATTR_UPDATE_MAX_LENGTH,
            &truth) != 0) {
        fprintf(stderr, __FILE__ ": mysql_stmt_attr_set() failed");
        return GIT_EUSER;
    }

    if (mysql_stmt_prepare(*statement, sql_query, strlen(sql_query)) != 0) {
        fprintf(stderr, __FILE__ ": mysql_stmt_prepare(%s) failed: %s",
            sql_query_short_name,
            mysql_error(db));
        return GIT_EUSER;
    }

    return GIT_OK;
}


static int init_statements(mariadb_refdb_backend_t *backend,
        const char *mysql_table)
{
    if (init_statement(backend->db, "exists", SQL_EXISTS, mysql_table,
            &backend->st_exists) != GIT_OK)
        return GIT_EUSER;

    if (init_statement(backend->db, "lookup", SQL_LOOKUP, mysql_table,
            &backend->st_lookup) != GIT_OK)
        return GIT_EUSER;

    if (init_statement(backend->db, "iterator", SQL_ITERATOR, mysql_table,
            &backend->st_iterator) != GIT_OK)
        return GIT_EUSER;

    if (init_statement(backend->db, "write no force", SQL_WRITE_NO_FORCE,
            mysql_table, &backend->st_write_no_force) != GIT_OK)
        return GIT_EUSER;

    if (init_statement(backend->db, "write force", SQL_WRITE_FORCE,
            mysql_table, &backend->st_write_force) != GIT_OK)
        return GIT_EUSER;

    if (init_statement(backend->db, "rename", SQL_RENAME, mysql_table,
            &backend->st_rename) != GIT_OK)
        return GIT_EUSER;

    if (init_statement(backend->db, "delete", SQL_DELETE, mysql_table,
            &backend->st_delete) != GIT_OK)
        return GIT_EUSER;

    if (init_statement(backend->db, "optimize", SQL_OPTIMIZE, mysql_table,
            &backend->st_optimize) != GIT_OK)
        return GIT_EUSER;

    return GIT_OK;
}


int git_refdb_backend_mariadb(git_refdb_backend **backend_out,
        MYSQL *db,
        const char *mariadb_table,
        uint32_t git_repository_id,
        int refdb_partitions)
{
    mariadb_refdb_backend_t *backend;
    int error;

    backend = calloc(1, sizeof(mariadb_refdb_backend_t));
    if (backend == NULL) {
        return GIT_EUSER;
    }

    *backend_out = &backend->parent;
    backend->db = db;

    error = init_db(db, mariadb_table, refdb_partitions);
    if (error < 0) {
        goto error;
    }

    error = init_statements(backend, mariadb_table);
    if (error < 0) {
        goto error;
    }

    backend->repository_id = git_repository_id;

    backend->parent.exists = mariadb_refdb_exists;
    backend->parent.lookup = mariadb_refdb_lookup;
    backend->parent.iterator = mariadb_refdb_iterator;
    backend->parent.write = mariadb_refdb_write;
    backend->parent.rename = mariadb_refdb_rename;
    backend->parent.del = mariadb_refdb_del;
    backend->parent.compress = mariadb_refdb_compress;
    backend->parent.has_log = mariadb_refdb_has_log;
    backend->parent.ensure_log = mariadb_refdb_ensure_log;
    backend->parent.free = mariadb_refdb_free;
    backend->parent.reflog_read = mariadb_refdb_reflog_read;
    backend->parent.reflog_write = mariadb_refdb_reflog_write;
    backend->parent.reflog_rename = mariadb_refdb_reflog_rename;
    backend->parent.reflog_delete = mariadb_refdb_reflog_delete;
    backend->parent.lock = mariadb_refdb_lock;
    backend->parent.unlock = mariadb_refdb_unlock;

    return GIT_OK;

error:
    mariadb_refdb_free(&backend->parent);
    return GIT_EUSER;
}
