/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "streams/mbedtls.h"

#ifdef GIT_MBEDTLS

#include <ctype.h>

#include "global.h"
#include "stream.h"
#include "streams/socket.h"
#include "netops.h"
#include "git2/transport.h"
#include "util.h"

#ifdef GIT_CURL
# include "streams/curl.h"
#endif

#include <mbedtls/config.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>

#define CRT_LOC "/etc/ssl/certs"

mbedtls_ssl_config *git__ssl_conf;
mbedtls_entropy_context *mbedtls_entropy;

#define GIT_SSL_DEFAULT_CIPHERS "TLS-ECDHE-ECDSA-WITH-AES-128-GCM-SHA256:TLS-ECDHE-RSA-WITH-AES-128-GCM-SHA256:TLS-ECDHE-ECDSA-WITH-AES-256-GCM-SHA384:TLS-ECDHE-RSA-WITH-AES-256-GCM-SHA384:TLS-DHE-RSA-WITH-AES-128-GCM-SHA256:TLS-DHE-DSS-WITH-AES-128-GCM-SHA256:TLS-DHE-RSA-WITH-AES-256-GCM-SHA384:TLS-DHE-DSS-WITH-AES-256-GCM-SHA384:TLS-ECDHE-ECDSA-WITH-AES-128-CBC-SHA256:TLS-ECDHE-RSA-WITH-AES-128-CBC-SHA256:TLS-ECDHE-ECDSA-WITH-AES-128-CBC-SHA:TLS-ECDHE-RSA-WITH-AES-128-CBC-SHA:TLS-ECDHE-ECDSA-WITH-AES-256-CBC-SHA384:TLS-ECDHE-RSA-WITH-AES-256-CBC-SHA384:TLS-ECDHE-ECDSA-WITH-AES-256-CBC-SHA:TLS-ECDHE-RSA-WITH-AES-256-CBC-SHA:TLS-DHE-RSA-WITH-AES-128-CBC-SHA256:TLS-DHE-RSA-WITH-AES-256-CBC-SHA256:TLS-DHE-RSA-WITH-AES-128-CBC-SHA:TLS-DHE-RSA-WITH-AES-256-CBC-SHA:TLS-DHE-DSS-WITH-AES-128-CBC-SHA256:TLS-DHE-DSS-WITH-AES-256-CBC-SHA256:TLS-DHE-DSS-WITH-AES-128-CBC-SHA:TLS-DHE-DSS-WITH-AES-256-CBC-SHA:TLS-RSA-WITH-AES-128-GCM-SHA256:TLS-RSA-WITH-AES-256-GCM-SHA384:TLS-RSA-WITH-AES-128-CBC-SHA256:TLS-RSA-WITH-AES-256-CBC-SHA256:TLS-RSA-WITH-AES-128-CBC-SHA:TLS-RSA-WITH-AES-256-CBC-SHA"
#define GIT_SSL_DEFAULT_CIPHERS_COUNT 30

/**
 * This function aims to clean-up the SSL context which
 * we allocated.
 */
static void shutdown_ssl(void)
{
	if (git__ssl_conf) {
		mbedtls_x509_crt_free(git__ssl_conf->ca_chain);
		git__free(git__ssl_conf->ca_chain);
		mbedtls_ctr_drbg_free(git__ssl_conf->p_rng);
		git__free(git__ssl_conf->p_rng);
		mbedtls_ssl_config_free(git__ssl_conf);
		git__free(git__ssl_conf);
		git__ssl_conf = NULL;
	}
	if (mbedtls_entropy) {
		mbedtls_entropy_free(mbedtls_entropy);
		git__free(mbedtls_entropy);
		mbedtls_entropy = NULL;
	}
}

int git_mbedtls_stream_global_init(void)
{
	int ret;
	mbedtls_ctr_drbg_context *ctr_drbg = NULL;

	int *ciphers_list = NULL;
	int ciphers_known = 0;
	char *cipher_name = NULL;
	char *cipher_string = NULL;
	char *cipher_string_tmp = NULL;

	mbedtls_x509_crt *cacert = NULL;

	git__ssl_conf = git__malloc(sizeof(mbedtls_ssl_config));
	mbedtls_ssl_config_init(git__ssl_conf);
	if (mbedtls_ssl_config_defaults(git__ssl_conf,
		                            MBEDTLS_SSL_IS_CLIENT,
		                            MBEDTLS_SSL_TRANSPORT_STREAM,
		                            MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
		giterr_set(GITERR_SSL, "failed to initialize mbedTLS");
		goto cleanup;
	}

	/* configure TLSv1 */
	mbedtls_ssl_conf_min_version(git__ssl_conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_0);
	mbedtls_ssl_conf_authmode(git__ssl_conf, MBEDTLS_SSL_VERIFY_REQUIRED);

	/* set the list of allowed ciphersuites */
	ciphers_list = calloc(GIT_SSL_DEFAULT_CIPHERS_COUNT, sizeof(int));
	ciphers_known = 0;
	cipher_string = cipher_string_tmp = git__strdup(GIT_SSL_DEFAULT_CIPHERS);
	while ((cipher_name = git__strtok(&cipher_string_tmp, ":")) != NULL) {
		int cipherid = mbedtls_ssl_get_ciphersuite_id(cipher_name);
		if (cipherid == 0) continue;

		ciphers_list[ciphers_known++] = cipherid;
	}
	git__free(cipher_string);

	if (!ciphers_known) {
		giterr_set(GITERR_SSL, "no cipher could be enabled");
		goto cleanup;
	}
	mbedtls_ssl_conf_ciphersuites(git__ssl_conf, ciphers_list);

	/* Seeding the random number generator */
	mbedtls_entropy = git__malloc(sizeof(mbedtls_entropy_context));
	mbedtls_entropy_init(mbedtls_entropy);

	ctr_drbg = git__malloc(sizeof(mbedtls_ctr_drbg_context));
	mbedtls_ctr_drbg_init(ctr_drbg);
	if (mbedtls_ctr_drbg_seed(ctr_drbg,
		                      mbedtls_entropy_func,
		                      mbedtls_entropy, NULL, 0) != 0) {
		giterr_set(GITERR_SSL, "failed to initialize mbedTLS entropy pool");
		goto cleanup;
	}

	mbedtls_ssl_conf_rng(git__ssl_conf, mbedtls_ctr_drbg_random, ctr_drbg);

	// set root certificates
	cacert = git__malloc(sizeof(mbedtls_x509_crt));
	mbedtls_x509_crt_init(cacert);
	ret = mbedtls_x509_crt_parse_path(cacert, CRT_LOC);
	if (ret) {
		giterr_set(GITERR_SSL, "failed to load CA certificates: %d", ret);
		goto cleanup;
	}

	mbedtls_ssl_conf_ca_chain(git__ssl_conf, cacert, NULL);

	git__on_shutdown(shutdown_ssl);

	return 0;

cleanup:
	mbedtls_x509_crt_free(cacert);
	git__free(cacert);
	mbedtls_ctr_drbg_free(ctr_drbg);
	git__free(ctr_drbg);
	mbedtls_ssl_config_free(git__ssl_conf);
	git__free(git__ssl_conf);
	git__ssl_conf = NULL;

	return -1;
}

mbedtls_ssl_config *git__ssl_conf;

static int bio_read(void *b, unsigned char *buf, size_t len)
{
	git_stream *io = (git_stream *) b;
	return (int) git_stream_read(io, buf, len);
}

static int bio_write(void *b, const unsigned char *buf, size_t len)
{
	git_stream *io = (git_stream *) b;
	return (int) git_stream_write(io, (const char *)buf, len, 0);
}

static int ssl_set_error(mbedtls_ssl_context *ssl, int error)
{
	char errbuf[512];
	int ret = -1;

	assert(error != MBEDTLS_ERR_SSL_WANT_READ);
	assert(error != MBEDTLS_ERR_SSL_WANT_WRITE);

	if (error != 0)
		mbedtls_strerror( error, errbuf, 512 );

	switch(error) {
		case 0:
		giterr_set(GITERR_SSL, "SSL error: unknown error");
		break;

	case MBEDTLS_ERR_X509_CERT_VERIFY_FAILED:
		giterr_set(GITERR_SSL, "SSL error: %x[%x] - %s", error, ssl->session_negotiate->verify_result, errbuf);
		ret = GIT_ECERTIFICATE;
		break;

	default:
		giterr_set(GITERR_SSL, "SSL error: %x - %s", error, errbuf);
	}

	return ret;
}

static int ssl_teardown(mbedtls_ssl_context *ssl)
{
	int ret = 0;

	ret = mbedtls_ssl_close_notify(ssl);
	if (ret < 0)
		ret = ssl_set_error(ssl, ret);

	mbedtls_ssl_free(ssl);
	return ret;
}

static int check_host_name(const char *name, const char *host)
{
	if (!strcasecmp(name, host))
		return 0;

	if (gitno__match_host(name, host) < 0)
		return -1;

	return 0;
}

static int verify_server_cert(mbedtls_ssl_context *ssl, const char *host)
{
	const mbedtls_x509_crt *cert;
	const mbedtls_x509_sequence *alts;
	int ret, matched = -1;
	size_t sn_size = 512;
	char subject_name[sn_size], alt_name[sn_size];


	if ((ret = mbedtls_ssl_get_verify_result(ssl)) != 0) {
		char vrfy_buf[512];
		mbedtls_x509_crt_verify_info( vrfy_buf, sizeof( vrfy_buf ), "  ! ", ret );
		giterr_set(GITERR_SSL, "The SSL certificate is invalid: %s", vrfy_buf);
		return GIT_ECERTIFICATE;
	}

	cert = mbedtls_ssl_get_peer_cert(ssl);
	if (!cert) {
		giterr_set(GITERR_SSL, "the server did not provide a certificate");
		return -1;
	}

	/* Check the alternative names */
	alts = &cert->subject_alt_names;
	while (alts != NULL && matched != 1) {
		// Buffer is too small
		if( alts->buf.len >= sn_size )
			goto on_error;

		memcpy(alt_name, alts->buf.p, alts->buf.len);
		alt_name[alts->buf.len] = '\0';

		if (!memchr(alt_name, '\0', alts->buf.len)) {
			if (check_host_name(alt_name, host) < 0)
				matched = 0;
			else
				matched = 1;
		}

		alts = alts->next;
	}
	if (matched == 0)
		goto cert_fail_name;

	if (matched == 1)
		return 0;

	/* If no alternative names are available, check the common name */
	ret = mbedtls_x509_dn_gets(subject_name, sn_size, &cert->subject);
	if (ret == 0)
		goto on_error;
	if (memchr(subject_name, '\0', ret))
		goto cert_fail_name;

	if (check_host_name(subject_name, host) < 0)
		goto cert_fail_name;

	return 0;

on_error:
	return ssl_set_error(ssl, 0);

cert_fail_name:
	giterr_set(GITERR_SSL, "hostname does not match certificate");
	return GIT_ECERTIFICATE;
}

typedef struct {
	git_stream parent;
	git_stream *io;
	bool connected;
	char *host;
	mbedtls_ssl_context *ssl;
	git_cert_x509 cert_info;
} mbedtls_stream;


int mbedtls_connect(git_stream *stream)
{
	int ret;
	mbedtls_stream *st = (mbedtls_stream *) stream;

	if ((ret = git_stream_connect(st->io)) < 0)
		return ret;

	st->connected = true;

	mbedtls_ssl_set_hostname(st->ssl, st->host);

	mbedtls_ssl_set_bio(st->ssl, st->io, bio_write, bio_read, NULL);

	if ((ret = mbedtls_ssl_handshake(st->ssl)) != 0)
		return ssl_set_error(st->ssl, ret);

	return verify_server_cert(st->ssl, st->host);
}

int mbedtls_certificate(git_cert **out, git_stream *stream)
{
	unsigned char *encoded_cert;
	mbedtls_stream *st = (mbedtls_stream *) stream;

	const mbedtls_x509_crt *cert = mbedtls_ssl_get_peer_cert(st->ssl);
	if (!cert) {
		giterr_set(GITERR_SSL, "the server did not provide a certificate");
		return -1;
	}

	/* Retrieve the length of the certificate first */
	if (cert->raw.len == 0) {
		giterr_set(GITERR_NET, "failed to retrieve certificate information");
		return -1;
	}

	encoded_cert = git__malloc(cert->raw.len);
	GITERR_CHECK_ALLOC(encoded_cert);
	memcpy(encoded_cert, cert->raw.p, cert->raw.len);

	st->cert_info.parent.cert_type = GIT_CERT_X509;
	st->cert_info.data = encoded_cert;
	st->cert_info.len = cert->raw.len;

	*out = &st->cert_info.parent;

	return 0;
}

static int mbedtls_set_proxy(git_stream *stream, const git_proxy_options *proxy_options)
{
	mbedtls_stream *st = (mbedtls_stream *) stream;

	return git_stream_set_proxy(st->io, proxy_options);
}

ssize_t mbedtls_stream_write(git_stream *stream, const char *data, size_t len, int flags)
{
	size_t read = 0;
	mbedtls_stream *st = (mbedtls_stream *) stream;

	GIT_UNUSED(flags);

	do {
		int error = mbedtls_ssl_write(st->ssl, (const unsigned char *)data + read, len - read);
		if (error <= 0) {
			return ssl_set_error(st->ssl, error);
		}
		read += error;
	} while (read < len);

	return read;
}

ssize_t mbedtls_stream_read(git_stream *stream, void *data, size_t len)
{
	mbedtls_stream *st = (mbedtls_stream *) stream;
	int ret;

	if ((ret = mbedtls_ssl_read(st->ssl, (unsigned char *)data, len)) <= 0)
		ssl_set_error(st->ssl, ret);

	return ret;
}

int mbedtls_stream_close(git_stream *stream)
{
	mbedtls_stream *st = (mbedtls_stream *) stream;
	int ret = 0;

	if (st->connected && (ret = ssl_teardown(st->ssl)) != 0)
		return -1;

	st->connected = false;

	return git_stream_close(st->io);
}

void mbedtls_stream_free(git_stream *stream)
{
	mbedtls_stream *st = (mbedtls_stream *) stream;

	git__free(st->host);
	git__free(st->cert_info.data);
	git_stream_free(st->io);
	git__free(st->ssl);
	git__free(st);
}

int git_mbedtls_stream_new(git_stream **out, const char *host, const char *port)
{
	int error;
	mbedtls_stream *st;

	st = git__calloc(1, sizeof(mbedtls_stream));
	GITERR_CHECK_ALLOC(st);

#ifdef GIT_CURL
	error = git_curl_stream_new(&st->io, host, port);
#else
	error = git_socket_stream_new(&st->io, host, port);
#endif

	if (error < 0)
		goto out_err;

	st->ssl = git__malloc(sizeof(mbedtls_ssl_context));
	GITERR_CHECK_ALLOC(st->ssl);
	mbedtls_ssl_init(st->ssl);
	if (mbedtls_ssl_setup(st->ssl, git__ssl_conf)) {
		giterr_set(GITERR_SSL, "failed to create ssl object");
		error = -1;
		goto out_err;
	}

	st->host = git__strdup(host);
	GITERR_CHECK_ALLOC(st->host);

	st->parent.version = GIT_STREAM_VERSION;
	st->parent.encrypted = 1;
	st->parent.proxy_support = git_stream_supports_proxy(st->io);
	st->parent.connect = mbedtls_connect;
	st->parent.certificate = mbedtls_certificate;
	st->parent.set_proxy = mbedtls_set_proxy;
	st->parent.read = mbedtls_stream_read;
	st->parent.write = mbedtls_stream_write;
	st->parent.close = mbedtls_stream_close;
	st->parent.free = mbedtls_stream_free;

	*out = (git_stream *) st;
	return 0;

out_err:
	mbedtls_ssl_free(st->ssl);
	git_stream_free(st->io);
	git__free(st);

	return error;
}

int git_mbedtls__set_cert_location(const char *file, const char *path)
{
	int ret = 0;
	char errbuf[512];
	if (!file) {
		ret = mbedtls_x509_crt_parse_file(git__ssl_conf->ca_chain, file);
	} else if (!path) {
		ret = mbedtls_x509_crt_parse_path(git__ssl_conf->ca_chain, path);
	}
	if (ret != 0) {
		mbedtls_strerror( ret, errbuf, 512 );
		giterr_set(GITERR_NET, "SSL error: %d - %s", ret, errbuf);
		return -1;
	}
	return 0;
}

#else

#include "stream.h"

int git_mbedtls_stream_global_init(void)
{
	return 0;
}

int git_mbedtls_stream_new(git_stream **out, const char *host, const char *port)
{
	GIT_UNUSED(out);
	GIT_UNUSED(host);
	GIT_UNUSED(port);

	giterr_set(GITERR_SSL, "mbedTLS is not supported in this version");
	return -1;
}

int git_mbedtls__set_cert_location(const char *file, const char *path)
{
	GIT_UNUSED(file);
	GIT_UNUSED(path);

	giterr_set(GITERR_SSL, "mbedTLS is not supported in this version");
	return -1;
}

#endif
