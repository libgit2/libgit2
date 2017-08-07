/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2.h"
#include "common.h"
#include "buffer.h"
#include "auth.h"
#include "auth_ntlm.h"

#ifdef GIT_NTLM

#include <transports/ntlm/ntlm.h>
#include <transports/ntlm/crypto.h>

typedef struct {
	git_http_auth_context parent;
	char *challenge;
	unsigned int state;
	struct ntlm_ctx *ntctx;
} http_auth_ntlm_context;

static int ntlm_set_challenge(
	git_http_auth_context *c,
	const char *challenge)
{
	http_auth_ntlm_context *ctx = (http_auth_ntlm_context *)c;

	assert(ctx && ctx->state && challenge);

	git__free(ctx->challenge);

	ctx->challenge = git__strdup(challenge);
	GITERR_CHECK_ALLOC(ctx->challenge);

	return 0;
}

static int ntlm_next_token(
	git_buf *buf,
	git_http_auth_context *c,
	git_cred *cred)
{
	http_auth_ntlm_context *ctx = (http_auth_ntlm_context *)c;
	git_cred_userpass_plaintext *userpw;
	uint32_t flags = 0;
	char *target = NULL;
	uint8_t s_chal_data[8] = { 0 };
	uint8_t c_chal_data[8] = { 0 };
	uint8_t msg_data = 0;
	struct ntlm_buffer msg = { 0 };
	struct ntlm_buffer s_chal = { (uint8_t *)&s_chal_data, 8 };
	struct ntlm_buffer c_chal = { (uint8_t *)&c_chal_data, 8 };
	struct ntlm_buffer s_ti = { 0 };
	struct ntlm_buffer c_ti = { 0 };
	struct ntlm_buffer cb = { 0 };
	struct ntlm_buffer nt_resp = { 0 };
	struct ntlm_buffer nt_proof = { 0 };
	struct ntlm_buffer s_key = { 0 };
	struct ntlm_key nt_key = { .length = 16 };
	struct ntlm_key nt_key2 = { .length = 16 };
	struct ntlm_key kex_key = { .length = 16 };
	struct ntlm_key exp_key = { .length = 16 };
	struct ntlm_key enc_key = { .length = 16 };
	git_buf challenge_buf = GIT_BUF_INIT;
	size_t challenge_len;
	uint64_t srv_time = 0;
	bool protect = false, kex = false;
	int error = 0, ret = 0;

	assert(buf && ctx && ctx->state && cred &&
			cred->credtype == GIT_CREDTYPE_USERPASS_PLAINTEXT);
	userpw = (git_cred_userpass_plaintext *)cred;

	if (ctx->state == NTLMSSP_STAGE_NEGOTIATE) {
		flags = NTLMSSP_NEGOTIATE_UNICODE | NTLMSSP_REQUEST_TARGET;

		if ((ret = ntlm_encode_neg_msg(ctx->ntctx, flags, NULL, NULL, &msg))) {
			giterr_set(GITERR_NET, "failed to encode NTLM negotiate message (%d)", ret);
			error = -1;
			goto done;
		}

		ctx->state = NTLMSSP_STAGE_CHALLENGE;
	} else if (ctx->state == NTLMSSP_STAGE_CHALLENGE) {
		if ((challenge_len = ctx->challenge ? strlen(ctx->challenge) : 0) < 6) {
			giterr_set(GITERR_NET, "no NTLM challenge sent from server");
			error = -1;
			goto done;
		}

		if (git_buf_decode_base64(
						&challenge_buf,
						ctx->challenge + 5,
						challenge_len - 5)) {
			giterr_set(GITERR_NET, "invalid NTLM challenge from server");
			error = -1;
			goto done;
		}

		msg.data = (uint8_t *)challenge_buf.ptr;
		msg.length = challenge_buf.size;

		if ((ret = ntlm_decode_chal_msg(
						ctx->ntctx,
						&msg,
						&flags,
						&target,
						&s_chal,
						&s_ti))) {
			git_buf_free(&challenge_buf);
			giterr_set(GITERR_NET, "failed to decode NTLM challenge message (%d)", ret);
			error = -1;
			goto done;
		} else {
			git_buf_free(&challenge_buf);
			memset(&msg, 0, sizeof(msg));
		}

		protect = flags & (NTLMSSP_NEGOTIATE_SIGN | NTLMSSP_NEGOTIATE_SEAL);

		if ((ret = ntlm_process_target_info(
						ctx->ntctx,
						protect,
						&s_ti,
						NULL,
						&cb,
						&c_ti,
						&srv_time,
						NULL))) {
			giterr_set(GITERR_NET, "failed to process server target info (%d)", ret);
			error = -1;
			goto done;
		}

		if ((ret = NTOWFv1(userpw->password, &nt_key))) {
			giterr_set(GITERR_NET, "failed to compute NTLM key (%d)", ret);
			error = -1;
			goto done;
		}

		if ((ret = NTOWFv2(
						ctx->ntctx,
						&nt_key,
						userpw->username,
						target,
						&nt_key2))) {
			giterr_set(GITERR_NET, "failed to compute NTLMv2 key (%d)", ret);
			error = -1;
			goto done;
		}

		if ((ret = RAND_BUFFER(&c_chal))) {
			giterr_set(GITERR_NET, "failed to compute client challenge (%d)", ret);
			error = -1;
			goto done;
		}

		if ((ret = ntlmv2_compute_nt_response(
						&nt_key2,
						s_chal.data,
						c_chal.data,
						srv_time,
						&c_ti,
						&nt_resp))) {
			giterr_set(GITERR_NET, "failed to compute NTLMv2 response (%d)", ret);
			error = -1;
			goto done;
		}

		kex = flags & NTLMSSP_NEGOTIATE_KEY_EXCH;

		if (kex) {
			nt_proof.data = nt_resp.data;
			nt_proof.length = 16;

			if ((ret = ntlmv2_session_base_key(&nt_key2, &nt_proof, &kex_key))) {
				giterr_set(GITERR_NET, "failed to compute session key (%d)", ret);
				error = -1;
				goto done;
			}

			if ((ret = ntlm_exported_session_key(&kex_key, kex, &exp_key))) {
				giterr_set(GITERR_NET, "failed to export session key (%d)", ret);
				error = -1;
				goto done;
			}

			if ((ret = ntlm_encrypted_session_key(&kex_key, &exp_key, &enc_key))) {
				giterr_set(GITERR_NET, "failed to encrypt session key (%d)", ret);
				error = -1;
				goto done;
			}

			s_key.data = enc_key.data;
			s_key.length = enc_key.length;
		}

		msg.data = &msg_data;
		msg.length = sizeof(msg_data);

		if ((ret = ntlm_encode_auth_msg(
						ctx->ntctx,
						flags,
						NULL,
						&nt_resp,
						target,
						userpw->username,
						NULL,
						&s_key,
						NULL,
						&msg))) {
			giterr_set(GITERR_NET, "failed to encode NTLM auth message (%d)", ret);
			error = -1;
			goto done;
		}

		ctx->state = NTLMSSP_STAGE_AUTHENTICATE;
	} else if (ctx->state == NTLMSSP_STAGE_AUTHENTICATE) {
		if ((ctx->challenge ? strlen(ctx->challenge) : 0) == 4) {
			giterr_set(GITERR_NET, "authentication failure");
			error = -1;
			goto done;
		} else {
			goto done;
		}
	} else {
		giterr_set(GITERR_NET, "unknown NTLM handshake state (%d)", ctx->state);
		error = -1;
		goto done;
	}

	git_buf_puts(buf, "Authorization: NTLM ");
	git_buf_encode_base64(buf, (const char *)msg.data, msg.length);
	git_buf_puts(buf, "\r\n");

	ntlm_free_buffer_data(&msg);

	if (git_buf_oom(buf))
		error = -1;

done:
	if (target) free(target);
	if (nt_resp.length) ntlm_free_buffer_data(&nt_resp);
	if (s_ti.length) ntlm_free_buffer_data(&s_ti);
	if (c_ti.length) ntlm_free_buffer_data(&c_ti);

	return error;
}

static void ntlm_context_free(git_http_auth_context *c)
{
	http_auth_ntlm_context *ctx = (http_auth_ntlm_context *)c;

	git__free(ctx->challenge);
	ctx->state = NTLMSSP_STAGE_INIT;

	if (ctx->ntctx) {
		ntlm_free_ctx(&(ctx->ntctx));
		ctx->ntctx = NULL;
	}

	git__free(ctx);
}

int git_http_auth_ntlm(
	git_http_auth_context **out,
	const gitno_connection_data *connection_data)
{
	http_auth_ntlm_context *ctx;

	GIT_UNUSED(connection_data);

	*out = NULL;

	ctx = git__calloc(1, sizeof(http_auth_ntlm_context));
	GITERR_CHECK_ALLOC(ctx);

	ctx->parent.type = GIT_AUTHTYPE_NTLM;
	ctx->parent.credtypes = GIT_CREDTYPE_USERPASS_PLAINTEXT;
	ctx->parent.set_challenge = ntlm_set_challenge;
	ctx->parent.next_token = ntlm_next_token;
	ctx->parent.free = ntlm_context_free;
	ctx->state = NTLMSSP_STAGE_NEGOTIATE;
	ctx->ntctx = NULL;

	ntlm_init_ctx(&(ctx->ntctx));

	*out = (git_http_auth_context *)ctx;

	return 0;
}

#endif /* GIT_NTLM */

