/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_streams_openssl_dynamic_h__
#define INCLUDE_streams_openssl_dynamic_h__

#ifdef GIT_HTTPS_OPENSSL_DYNAMIC

/* These constants are taken from the OpenSSL 3.0 headers. */

# define BIO_CTRL_FLUSH               11

# define BIO_TYPE_SOURCE_SINK         0x0400

# define CRYPTO_LOCK                  1

# define GEN_DNS                      2
# define GEN_IPADD                    7

# define NID_commonName               13
# define NID_subject_alt_name         85

# define SSL_VERIFY_NONE              0x00

# define SSL_CTRL_OPTIONS             32
# define SSL_CTRL_MODE                33
# define SSL_CTRL_SET_TLSEXT_HOSTNAME 55

# define SSL_ERROR_NONE               0
# define SSL_ERROR_SSL                1
# define SSL_ERROR_WANT_READ          2
# define SSL_ERROR_WANT_WRITE         3
# define SSL_ERROR_WANT_X509_LOOKUP   4
# define SSL_ERROR_SYSCALL            5
# define SSL_ERROR_ZERO_RETURN        6
# define SSL_ERROR_WANT_CONNECT       7
# define SSL_ERROR_WANT_ACCEPT        8

# define SSL_OP_NO_COMPRESSION        0x00020000L
# define SSL_OP_NO_SSLv2              0x01000000L
# define SSL_OP_NO_SSLv3              0x02000000L
# define SSL_OP_NO_TLSv1              0x04000000L
# define SSL_OP_NO_TLSv1_1            0x10000000L

# define SSL_MODE_AUTO_RETRY          0x00000004L

# define TLSEXT_NAMETYPE_host_name    0

# define V_ASN1_UTF8STRING            12

# define X509_V_OK 0

/* Most of the OpenSSL types are mercifully opaque, so we can treat them like `void *` */
typedef struct bio_st BIO;
typedef struct bio_method_st BIO_METHOD;
typedef void bio_info_cb;
typedef void * CRYPTO_EX_DATA;
typedef void CRYPTO_THREADID;
typedef void GENERAL_NAMES;
typedef void SSL;
typedef void SSL_CTX;
typedef void SSL_METHOD;
typedef void X509;
typedef void X509_NAME;
typedef void X509_NAME_ENTRY;
typedef void X509_STORE;
typedef void X509_STORE_CTX;

typedef struct {
    int length;
    int type;
    unsigned char *data;
    long flags;
} ASN1_STRING;

typedef struct {
    int type;
    union {
        char *ptr;
        ASN1_STRING *ia5;
    } d;
} GENERAL_NAME;

struct bio_st {
    BIO_METHOD *method;
    /* bio, mode, argp, argi, argl, ret */
    long (*callback) (struct bio_st *, int, const char *, int, long, long);
    char *cb_arg;               /* first argument for the callback */
    int init;
    int shutdown;
    int flags;                  /* extra storage */
    int retry_reason;
    int num;
    void *ptr;
    struct bio_st *next_bio;    /* used by filter BIOs */
    struct bio_st *prev_bio;    /* used by filter BIOs */
    int references;
    unsigned long num_read;
    unsigned long num_write;
    CRYPTO_EX_DATA ex_data;
};

struct bio_method_st {
    int type;
    const char *name;
    int (*bwrite) (BIO *, const char *, int);
    int (*bread) (BIO *, char *, int);
    int (*bputs) (BIO *, const char *);
    int (*bgets) (BIO *, char *, int);
    long (*ctrl) (BIO *, int, long, void *);
    int (*create) (BIO *);
    int (*destroy) (BIO *);
    long (*callback_ctrl) (BIO *, int, bio_info_cb *);
};

extern unsigned char *(*ASN1_STRING_data)(ASN1_STRING *x);
extern const unsigned char *(*ASN1_STRING_get0_data)(const ASN1_STRING *x);
extern int (*ASN1_STRING_length)(const ASN1_STRING *x);
extern int (*ASN1_STRING_to_UTF8)(unsigned char **out, const ASN1_STRING *in);
extern int (*ASN1_STRING_type)(const ASN1_STRING *x);

extern void *(*BIO_get_data)(BIO *a);
extern int (*BIO_get_new_index)(void);
extern int (*OPENSSL_init_ssl)(uint64_t opts, const void *settings);
extern void (*BIO_meth_free)(BIO_METHOD *biom);
extern int (*BIO_meth_set_create)(BIO_METHOD *biom, int (*create) (BIO *));
extern int (*BIO_meth_set_ctrl)(BIO_METHOD *biom, long (*ctrl) (BIO *, int, long, void *));
extern int (*BIO_meth_set_destroy)(BIO_METHOD *biom, int (*destroy) (BIO *));
extern int (*BIO_meth_set_gets)(BIO_METHOD *biom, int (*gets) (BIO *, char *, int));
extern int (*BIO_meth_set_puts)(BIO_METHOD *biom, int (*puts) (BIO *, const char *));
extern int (*BIO_meth_set_read)(BIO_METHOD *biom, int (*read) (BIO *, char *, int));
extern int (*BIO_meth_set_write)(BIO_METHOD *biom, int (*write) (BIO *, const char *, int));
extern BIO_METHOD *(*BIO_meth_new)(int type, const char *name);
extern BIO *(*BIO_new)(const BIO_METHOD *type);
extern void (*BIO_set_data)(BIO *a, void *ptr);
extern void (*BIO_set_init)(BIO *a, int init);

extern void (*CRYPTO_free)(void *ptr, const char *file, int line);
extern void *(*CRYPTO_malloc)(size_t num, const char *file, int line);
extern int (*CRYPTO_num_locks)(void);
extern void (*CRYPTO_set_locking_callback)(void (*func)(int mode, int type, const char *file, int line));
extern int (*CRYPTO_set_mem_functions)(void *(*m)(size_t bytes), void *(*r)(void *mem, size_t size), void (*f)(void *mem));
extern int (*CRYPTO_THREADID_set_callback)(void (*func)(CRYPTO_THREADID *id));
extern void (*CRYPTO_THREADID_set_numeric)(CRYPTO_THREADID *id, unsigned long val);

extern char *(*ERR_error_string)(unsigned long e, char *buf);
extern void (*ERR_error_string_n)(unsigned long e, char *buf, size_t len);
extern unsigned long (*ERR_get_error)(void);

# define OPENSSL_malloc(num) CRYPTO_malloc(num, __FILE__, __LINE__)
# define OPENSSL_free(addr) CRYPTO_free(addr, __FILE__, __LINE__)

extern int (*SSL_connect)(SSL *ssl);
extern long (*SSL_ctrl)(SSL *ssl, int cmd, long arg, void *parg);
extern void (*SSL_free)(SSL *ssl);
extern int (*SSL_get_error)(SSL *ssl, int ret);
extern X509 *(*SSL_get_peer_certificate)(const SSL *ssl);
extern long (*SSL_get_verify_result)(const SSL *ssl);
extern int (*SSL_library_init)(void);
extern void (*SSL_load_error_strings)(void);
extern SSL *(*SSL_new)(SSL_CTX *ctx);
extern int (*SSL_read)(SSL *ssl, const void *buf, int num);
extern void (*SSL_set_bio)(SSL *ssl, BIO *rbio, BIO *wbio);
extern int (*SSL_shutdown)(SSL *ssl);
extern int (*SSL_write)(SSL *ssl, const void *buf, int num);

# define SSL_set_tlsext_host_name(s, name) SSL_ctrl((s), SSL_CTRL_SET_TLSEXT_HOSTNAME, TLSEXT_NAMETYPE_host_name, (char *)(name));

extern long (*SSL_CTX_ctrl)(SSL_CTX *ctx, int cmd, long larg, void *parg);
extern void (*SSL_CTX_free)(SSL_CTX *ctx);
extern SSL_CTX *(*SSL_CTX_new)(const SSL_METHOD *method);
extern X509_STORE *(*SSL_CTX_get_cert_store)(const SSL_CTX *ctx);
extern int (*SSL_CTX_set_cipher_list)(SSL_CTX *ctx, const char *str);
extern int (*SSL_CTX_set_default_verify_paths)(SSL_CTX *ctx);
extern long (*SSL_CTX_set_options)(SSL_CTX *ctx, long options);
extern void (*SSL_CTX_set_verify)(SSL_CTX *ctx, int mode, int (*verify_callback)(int, X509_STORE_CTX *));
extern int (*SSL_CTX_load_verify_locations)(SSL_CTX *ctx, const char *CAfile, const char *CApath);

# define SSL_CTX_set_mode(ctx, mode) SSL_CTX_ctrl((ctx), SSL_CTRL_MODE, (mode), NULL);

extern const SSL_METHOD *(*SSLv23_method)(void);
extern const SSL_METHOD *(*TLS_method)(void);

extern ASN1_STRING *(*X509_NAME_ENTRY_get_data)(const X509_NAME_ENTRY *ne);
extern X509_NAME_ENTRY *(*X509_NAME_get_entry)(X509_NAME *name, int loc);
extern int (*X509_NAME_get_index_by_NID)(X509_NAME *name, int nid, int lastpos);
extern void (*X509_free)(X509 *a);
extern void *(*X509_get_ext_d2i)(const X509 *x, int nid, int *crit, int *idx);
extern X509_NAME *(*X509_get_subject_name)(const X509 *x);
extern int (*X509_STORE_add_cert)(X509_STORE *ctx, X509 *x);

extern int (*i2d_X509)(X509 *a, unsigned char **ppout);

extern int (*OPENSSL_sk_num)(const void *sk);
extern void *(*OPENSSL_sk_value)(const void *sk, int i);
extern void (*OPENSSL_sk_free)(void *sk);

extern int (*sk_num)(const void *sk);
extern void *(*sk_value)(const void *sk, int i);
extern void (*sk_free)(void *sk);

extern int sk_GENERAL_NAME_num(const GENERAL_NAME *sk);
extern GENERAL_NAME *sk_GENERAL_NAME_value(const GENERAL_NAME *sk, int i);
extern void GENERAL_NAMES_free(GENERAL_NAME *sk);

extern int git_openssl_stream_dynamic_init(void);

#endif /* GIT_HTTPS_OPENSSL_DYNAMIC */

#endif
