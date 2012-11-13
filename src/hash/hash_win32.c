/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "global.h"
#include "hash.h"
#include "hash/hash_win32.h"

#include <wincrypt.h>
#include <strsafe.h>

/* Initialize CNG, if available */
GIT_INLINE(int) hash_cng_prov_init(git_hash_prov *prov)
{
	OSVERSIONINFOEX version_test = {0};
	DWORD version_test_mask;
	DWORDLONG version_condition_mask = 0;
	char dll_path[MAX_PATH];
	DWORD dll_path_len, size_len;

	/* Only use CNG on Windows 2008 / Vista SP1  or better (Windows 6.0 SP1) */
	version_test.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	version_test.dwMajorVersion = 6;
	version_test.dwMinorVersion = 0;
	version_test.wServicePackMajor = 1;
	version_test.wServicePackMinor = 0;

	version_test_mask = (VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR);

	VER_SET_CONDITION(version_condition_mask, VER_MAJORVERSION, VER_GREATER_EQUAL);
	VER_SET_CONDITION(version_condition_mask, VER_MINORVERSION, VER_GREATER_EQUAL);
	VER_SET_CONDITION(version_condition_mask, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
	VER_SET_CONDITION(version_condition_mask, VER_SERVICEPACKMINOR, VER_GREATER_EQUAL);

	if (!VerifyVersionInfo(&version_test, version_test_mask, version_condition_mask))
		return -1;

	/* Load bcrypt.dll explicitly from the system directory */
	if ((dll_path_len = GetSystemDirectory(dll_path, MAX_PATH)) == 0 || dll_path_len > MAX_PATH ||
		StringCchCat(dll_path, MAX_PATH, "\\") < 0 ||
		StringCchCat(dll_path, MAX_PATH, GIT_HASH_CNG_DLL_NAME) < 0 ||
		(prov->prov.cng.dll = LoadLibrary(dll_path)) == NULL)
		return -1;

	/* Load the function addresses */
	if ((prov->prov.cng.open_algorithm_provider = (hash_win32_cng_open_algorithm_provider_fn)GetProcAddress(prov->prov.cng.dll, "BCryptOpenAlgorithmProvider")) == NULL ||
		(prov->prov.cng.get_property = (hash_win32_cng_get_property_fn)GetProcAddress(prov->prov.cng.dll, "BCryptGetProperty")) == NULL ||
		(prov->prov.cng.create_hash = (hash_win32_cng_create_hash_fn)GetProcAddress(prov->prov.cng.dll, "BCryptCreateHash")) == NULL ||
		(prov->prov.cng.finish_hash = (hash_win32_cng_finish_hash_fn)GetProcAddress(prov->prov.cng.dll, "BCryptFinishHash")) == NULL ||
		(prov->prov.cng.hash_data = (hash_win32_cng_hash_data_fn)GetProcAddress(prov->prov.cng.dll, "BCryptHashData")) == NULL ||
		(prov->prov.cng.destroy_hash = (hash_win32_cng_destroy_hash_fn)GetProcAddress(prov->prov.cng.dll, "BCryptDestroyHash")) == NULL ||
		(prov->prov.cng.close_algorithm_provider = (hash_win32_cng_close_algorithm_provider_fn)GetProcAddress(prov->prov.cng.dll, "BCryptCloseAlgorithmProvider")) == NULL) {
		FreeLibrary(prov->prov.cng.dll);
		return -1;
	}

	/* Load the SHA1 algorithm */
	if (prov->prov.cng.open_algorithm_provider(&prov->prov.cng.handle, GIT_HASH_CNG_HASH_TYPE, NULL, GIT_HASH_CNG_HASH_REUSABLE) < 0) {
		FreeLibrary(prov->prov.cng.dll);
		return -1;
	}

	/* Get storage space for the hash object */
	if (prov->prov.cng.get_property(prov->prov.cng.handle, GIT_HASH_CNG_HASH_OBJECT_LEN, (PBYTE)&prov->prov.cng.hash_object_size, sizeof(DWORD), &size_len, 0) < 0) {
		prov->prov.cng.close_algorithm_provider(prov->prov.cng.handle, 0);
		FreeLibrary(prov->prov.cng.dll);
		return -1;
	}

	prov->type = CNG;
	return 0;
}

/* Initialize CryptoAPI */
GIT_INLINE(int) hash_cryptoapi_prov_init(git_hash_prov *prov)
{
	if (!CryptAcquireContext(&prov->prov.cryptoapi.handle, NULL, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
		return -1;

	prov->type = CRYPTOAPI;
	return 0;
}

static int hash_win32_prov_init(git_hash_prov *prov)
{
	int error = 0;

	assert(prov->type == INVALID);

	/* Try to load CNG */
	if ((error = hash_cng_prov_init(prov)) < 0)
		error = hash_cryptoapi_prov_init(prov);

	return error;
}

/* CryptoAPI: available in Windows XP and newer */

GIT_INLINE(int) hash_ctx_cryptoapi_init(git_hash_ctx *ctx, git_hash_prov *prov)
{
	ctx->type = CRYPTOAPI;
	ctx->prov = prov;

	return git_hash_init(ctx);
}

GIT_INLINE(int) hash_cryptoapi_init(git_hash_ctx *ctx)
{
	if (ctx->ctx.cryptoapi.valid)
		CryptDestroyHash(ctx->ctx.cryptoapi.hash_handle);

	if (!CryptCreateHash(ctx->prov->prov.cryptoapi.handle, CALG_SHA1, 0, 0, &ctx->ctx.cryptoapi.hash_handle)) {
		ctx->ctx.cryptoapi.valid = 0;
		return -1;
	}

	ctx->ctx.cryptoapi.valid = 1;
	return 0;
}

GIT_INLINE(int) hash_cryptoapi_update(git_hash_ctx *ctx, const void *data, size_t len)
{
	assert(ctx->ctx.cryptoapi.valid);

	if (!CryptHashData(ctx->ctx.cryptoapi.hash_handle, (const BYTE *)data, len, 0))
		return -1;

	return 0;
}

GIT_INLINE(int) hash_cryptoapi_final(git_oid *out, git_hash_ctx *ctx)
{
	DWORD len = 20;
	int error = 0;

	assert(ctx->ctx.cryptoapi.valid);

	if (!CryptGetHashParam(ctx->ctx.cryptoapi.hash_handle, HP_HASHVAL, out->id, &len, 0))
		error = -1;

	CryptDestroyHash(ctx->ctx.cryptoapi.hash_handle);
	ctx->ctx.cryptoapi.valid = 0;

	return error;
}

GIT_INLINE(void) hash_ctx_cryptoapi_cleanup(git_hash_ctx *ctx)
{
	if (ctx->ctx.cryptoapi.valid)
		CryptDestroyHash(ctx->ctx.cryptoapi.hash_handle);
}

/* CNG: Available in Windows Server 2008 and newer */

GIT_INLINE(int) hash_ctx_cng_init(git_hash_ctx *ctx, git_hash_prov *prov)
{
	if ((ctx->ctx.cng.hash_object = git__malloc(prov->prov.cng.hash_object_size)) == NULL)
		return -1;

	if (prov->prov.cng.create_hash(prov->prov.cng.handle, &ctx->ctx.cng.hash_handle, ctx->ctx.cng.hash_object, prov->prov.cng.hash_object_size, NULL, 0, 0) < 0) {
		git__free(ctx->ctx.cng.hash_object);
		return -1;
	}

	ctx->type = CNG;
	ctx->prov = prov;

	return 0;
}

GIT_INLINE(int) hash_cng_init(git_hash_ctx *ctx)
{
	BYTE hash[GIT_OID_RAWSZ];

	if (!ctx->ctx.cng.updated)
		return 0;

	/* CNG needs to be finished to restart */
	if (ctx->prov->prov.cng.finish_hash(ctx->ctx.cng.hash_handle, hash, GIT_OID_RAWSZ, 0) < 0)
		return -1;

	ctx->ctx.cng.updated = 0;

	return 0;
}

GIT_INLINE(int) hash_cng_update(git_hash_ctx *ctx, const void *data, size_t len)
{
	if (ctx->prov->prov.cng.hash_data(ctx->ctx.cng.hash_handle, (PBYTE)data, len, 0) < 0)
		return -1;

	return 0;
}

GIT_INLINE(int) hash_cng_final(git_oid *out, git_hash_ctx *ctx)
{
	if (ctx->prov->prov.cng.finish_hash(ctx->ctx.cng.hash_handle, out->id, GIT_OID_RAWSZ, 0) < 0)
		return -1;

	ctx->ctx.cng.updated = 0;

	return 0;
}

GIT_INLINE(void) hash_ctx_cng_cleanup(git_hash_ctx *ctx)
{
	ctx->prov->prov.cng.destroy_hash(ctx->ctx.cng.hash_handle);
	git__free(ctx->ctx.cng.hash_object);
}

/* Indirection between CryptoAPI and CNG */

int git_hash_ctx_init(git_hash_ctx *ctx)
{
	git_global_st *global_state;
	git_hash_prov *hash_prov;
	
	assert(ctx);

	memset(ctx, 0x0, sizeof(git_hash_ctx));

	if ((global_state = git__global_state()) == NULL)
		return -1;

	hash_prov = &global_state->hash_prov;

	if (hash_prov->type == INVALID && hash_win32_prov_init(hash_prov) < 0)
		return -1;

	return (hash_prov->type == CNG) ? hash_ctx_cng_init(ctx, hash_prov) : hash_ctx_cryptoapi_init(ctx, hash_prov);
}

int git_hash_init(git_hash_ctx *ctx)
{
	assert(ctx && ctx->type);
	return (ctx->type == CNG) ? hash_cng_init(ctx) : hash_cryptoapi_init(ctx);
}

int git_hash_update(git_hash_ctx *ctx, const void *data, size_t len)
{
	assert(ctx && ctx->type);
	return (ctx->type == CNG) ? hash_cng_update(ctx, data, len) : hash_cryptoapi_update(ctx, data, len);
}

int git_hash_final(git_oid *out, git_hash_ctx *ctx)
{
	assert(ctx && ctx->type);
	return (ctx->type == CNG) ? hash_cng_final(out, ctx) : hash_cryptoapi_final(out, ctx);
}

void git_hash_ctx_cleanup(git_hash_ctx *ctx)
{
	assert(ctx);

	if (ctx->type == CNG)
		hash_ctx_cng_cleanup(ctx);
	else if(ctx->type == CRYPTOAPI)
		hash_ctx_cryptoapi_cleanup(ctx);
}
