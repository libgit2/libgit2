/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "streams/openssl.h"
#include "streams/openssl_legacy.h"

#if defined(GIT_OPENSSL) && defined(GIT_OPENSSL_LEGACY)

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include <openssl/bio.h>

#include "runtime.h"
#include "git2/sys/openssl.h"

/*
 * OpenSSL 1.1 made BIO opaque so we have to use functions to interact with it
 * which do not exist in previous versions. We define these inline functions so
 * we can program against the interface instead of littering the implementation
 * with ifdefs. We do the same for OPENSSL_init_ssl.
 */

int OPENSSL_init_ssl(int opts, void *settings)
{
	GIT_UNUSED(opts);
	GIT_UNUSED(settings);
	SSL_load_error_strings();
	OpenSSL_add_ssl_algorithms();
	return 0;
}

BIO_METHOD* BIO_meth_new(int type, const char *name)
{
	BIO_METHOD *meth = git__calloc(1, sizeof(BIO_METHOD));
	if (!meth) {
		return NULL;
	}

	meth->type = type;
	meth->name = name;

	return meth;
}

void BIO_meth_free(BIO_METHOD *biom)
{
	git__free(biom);
}

int BIO_meth_set_write(BIO_METHOD *biom, int (*write) (BIO *, const char *, int))
{
	biom->bwrite = write;
	return 1;
}

int BIO_meth_set_read(BIO_METHOD *biom, int (*read) (BIO *, char *, int))
{
	biom->bread = read;
	return 1;
}

int BIO_meth_set_puts(BIO_METHOD *biom, int (*puts) (BIO *, const char *))
{
	biom->bputs = puts;
	return 1;
}

int BIO_meth_set_gets(BIO_METHOD *biom, int (*gets) (BIO *, char *, int))

{
	biom->bgets = gets;
	return 1;
}

int BIO_meth_set_ctrl(BIO_METHOD *biom, long (*ctrl) (BIO *, int, long, void *))
{
	biom->ctrl = ctrl;
	return 1;
}

int BIO_meth_set_create(BIO_METHOD *biom, int (*create) (BIO *))
{
	biom->create = create;
	return 1;
}

int BIO_meth_set_destroy(BIO_METHOD *biom, int (*destroy) (BIO *))
{
	biom->destroy = destroy;
	return 1;
}

int BIO_get_new_index(void)
{
	/* This exists as of 1.1 so before we'd just have 0 */
	return 0;
}

void BIO_set_init(BIO *b, int init)
{
	b->init = init;
}

void BIO_set_data(BIO *a, void *ptr)
{
	a->ptr = ptr;
}

void *BIO_get_data(BIO *a)
{
	return a->ptr;
}

const unsigned char *ASN1_STRING_get0_data(const ASN1_STRING *x)
{
	return ASN1_STRING_data((ASN1_STRING *)x);
}

# if defined(GIT_THREADS)
static git_mutex *openssl_locks;

static void openssl_locking_function(
	int mode, int n, const char *file, int line)
{
	int lock;

	GIT_UNUSED(file);
	GIT_UNUSED(line);

	lock = mode & CRYPTO_LOCK;

	if (lock) {
		(void)git_mutex_lock(&openssl_locks[n]);
	} else {
		git_mutex_unlock(&openssl_locks[n]);
	}
}

static void shutdown_ssl_locking(void)
{
	int num_locks, i;

	num_locks = CRYPTO_num_locks();
	CRYPTO_set_locking_callback(NULL);

	for (i = 0; i < num_locks; ++i)
		git_mutex_free(&openssl_locks[i]);
	git__free(openssl_locks);
}

static void threadid_cb(CRYPTO_THREADID *threadid)
{
	GIT_UNUSED(threadid);
	CRYPTO_THREADID_set_numeric(threadid, git_thread_currentid());
}

int git_openssl_set_locking(void)
{
	int num_locks, i;

	CRYPTO_THREADID_set_callback(threadid_cb);

	num_locks = CRYPTO_num_locks();
	openssl_locks = git__calloc(num_locks, sizeof(git_mutex));
	GIT_ERROR_CHECK_ALLOC(openssl_locks);

	for (i = 0; i < num_locks; i++) {
		if (git_mutex_init(&openssl_locks[i]) != 0) {
			git_error_set(GIT_ERROR_SSL, "failed to initialize openssl locks");
			return -1;
		}
	}

	CRYPTO_set_locking_callback(openssl_locking_function);
	return git_runtime_shutdown_register(shutdown_ssl_locking);
}
#endif /* GIT_THREADS */

#ifdef VALGRIND
void *git_openssl_malloc(size_t bytes)
{
	return git__calloc(1, bytes);
}

void *git_openssl_realloc(void *mem, size_t size)
{
	return git__realloc(mem, size);
}

void git_openssl_free(void *mem)
{
	return git__free(mem);
}
#endif

#endif /* GIT_OPENSSL && GIT_OPENSSL_LEGACY */
