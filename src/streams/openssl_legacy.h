/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_streams_openssl_legacy_h__
#define INCLUDE_streams_openssl_legacy_h__

#ifdef GIT_OPENSSL
# include <openssl/ssl.h>
# include <openssl/err.h>
# include <openssl/x509v3.h>
# include <openssl/bio.h>

# if (defined(OPENSSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER < 0x10100000L) || \
     (defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER < 0x20700000L)
#  define GIT_OPENSSL_LEGACY
# endif
#endif

#ifdef GIT_OPENSSL_LEGACY

extern int OPENSSL_init_ssl(int opts, void *settings);
extern BIO_METHOD* BIO_meth_new(int type, const char *name);
extern void BIO_meth_free(BIO_METHOD *biom);
extern int BIO_meth_set_write(BIO_METHOD *biom, int (*write) (BIO *, const char *, int));
extern int BIO_meth_set_read(BIO_METHOD *biom, int (*read) (BIO *, char *, int));
extern int BIO_meth_set_puts(BIO_METHOD *biom, int (*puts) (BIO *, const char *));
extern int BIO_meth_set_gets(BIO_METHOD *biom, int (*gets) (BIO *, char *, int));
extern int BIO_meth_set_ctrl(BIO_METHOD *biom, long (*ctrl) (BIO *, int, long, void *));
extern int BIO_meth_set_create(BIO_METHOD *biom, int (*create) (BIO *));
extern int BIO_meth_set_destroy(BIO_METHOD *biom, int (*destroy) (BIO *));
extern int BIO_get_new_index(void);
extern void BIO_set_data(BIO *a, void *ptr);
extern void BIO_set_init(BIO *b, int init);
extern void *BIO_get_data(BIO *a);
extern const unsigned char *ASN1_STRING_get0_data(const ASN1_STRING *x);

#endif

#endif
