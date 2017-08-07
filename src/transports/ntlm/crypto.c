/*
   Copyright (C) 2013 Simo Sorce <simo@samba.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include <errno.h>
#include <string.h>

#include <openssl/des.h>
#include <openssl/rc4.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <zlib.h>

#include "crypto.h"

#if OPENSSL_VERSION_NUMBER < 0x10100000L
HMAC_CTX *HMAC_CTX_new(void)
{
    HMAC_CTX *ctx;

    ctx = OPENSSL_malloc(sizeof(HMAC_CTX));
    if (!ctx) return NULL;

    HMAC_CTX_init(ctx);

    return ctx;
}

void HMAC_CTX_free(HMAC_CTX *ctx)
{
    if (ctx == NULL) return;

    HMAC_CTX_cleanup(ctx);
    OPENSSL_free(ctx);
}

#define EVP_MD_CTX_new EVP_MD_CTX_create
#define EVP_MD_CTX_free EVP_MD_CTX_destroy

#endif

int RAND_BUFFER(struct ntlm_buffer *random)
{
    int ret;

    ret = RAND_bytes(random->data, random->length);
    if (ret != 1) {
        return ERR_CRYPTO;
    }
    return 0;
}

int HMAC_MD5_IOV(struct ntlm_buffer *key,
                 struct ntlm_iov *iov,
                 struct ntlm_buffer *result)
{
    HMAC_CTX *hmac_ctx;
    unsigned int len;
    size_t i;
    int ret = 0;

    if (result->length != 16) return EINVAL;

    hmac_ctx = HMAC_CTX_new();
    if (!hmac_ctx) {
        ret = ERR_CRYPTO;
        goto done;
    }

    ret = HMAC_Init_ex(hmac_ctx, key->data, key->length, EVP_md5(), NULL);
    if (ret == 0) {
        ret = ERR_CRYPTO;
        goto done;
    }

    for (i = 0; i < iov->num; i++) {
        ret = HMAC_Update(hmac_ctx, iov->data[i]->data, iov->data[i]->length);
        if (ret == 0) {
            ret = ERR_CRYPTO;
            goto done;
        }
    }

    ret = HMAC_Final(hmac_ctx, result->data, &len);
    if (ret == 0) {
        ret = ERR_CRYPTO;
        goto done;
    }

    ret = 0;

done:
    HMAC_CTX_free(hmac_ctx);
    return ret;
}

int HMAC_MD5(struct ntlm_buffer *key,
             struct ntlm_buffer *payload,
             struct ntlm_buffer *result)
{
    struct ntlm_iov iov;

    iov.num = 1;
    iov.data = &payload;
    return HMAC_MD5_IOV(key, &iov, result);
}

static int mdx_hash(const EVP_MD *type,
                    struct ntlm_buffer *payload,
                    struct ntlm_buffer *result)
{
    EVP_MD_CTX *ctx;
    unsigned int len;
    int ret;

    if (result->length != 16) return EINVAL;

    ctx = EVP_MD_CTX_new();
    if (!ctx) {
        ret = ERR_CRYPTO;
        goto done;
    }

    EVP_MD_CTX_init(ctx);
    ret = EVP_DigestInit_ex(ctx, type, NULL);
    if (ret == 0) {
        ret = ERR_CRYPTO;
        goto done;
    }

    ret = EVP_DigestUpdate(ctx, payload->data, payload->length);
    if (ret == 0) {
        ret = ERR_CRYPTO;
        goto done;
    }

    ret = EVP_DigestFinal_ex(ctx, result->data, &len);
    if (ret == 0) {
        ret = ERR_CRYPTO;
        goto done;
    }

    ret = 0;

done:
    if (ctx) EVP_MD_CTX_free(ctx);
    return ret;
}

int MD4_HASH(struct ntlm_buffer *payload,
             struct ntlm_buffer *result)
{
    return mdx_hash(EVP_md4(), payload, result);
}

int MD5_HASH(struct ntlm_buffer *payload,
             struct ntlm_buffer *result)
{
    return mdx_hash(EVP_md5(), payload, result);
}

struct ntlm_rc4_handle {
    RC4_KEY key;
};

int RC4_INIT(struct ntlm_buffer *rc4_key,
             enum ntlm_cipher_mode mode,
             struct ntlm_rc4_handle **out)
{
    struct ntlm_rc4_handle *handle;

    handle = malloc(sizeof(struct ntlm_rc4_handle));
    if (!handle) return ENOMEM;

    RC4_set_key(&handle->key, rc4_key->length, rc4_key->data);

    *out = handle;
    return 0;
}

int RC4_UPDATE(struct ntlm_rc4_handle *handle,
               struct ntlm_buffer *in, struct ntlm_buffer *out)
{
    if (out->length < in->length) return EINVAL;

    if (in->length > 0) {
        RC4(&handle->key, in->length, in->data, out->data);
    }

    out->length = in->length;
    return 0;
}

void RC4_FREE(struct ntlm_rc4_handle **handle)
{
    if (!handle || !*handle) return;
    safezero((uint8_t *)(&((*handle)->key)), sizeof(RC4_KEY));
    safefree(*handle);
}

int RC4_EXPORT(struct ntlm_rc4_handle *handle, struct ntlm_buffer *out)
{
    RC4_INT *data = (RC4_INT *)out->data;
    int len = 258 * sizeof(RC4_INT);

    if (out->length < len) return EINVAL;

    data[0] = handle->key.x;
    data[1] = handle->key.y;
    memcpy(&data[2], handle->key.data, sizeof(RC4_INT) * 256);
    out->length = len;
    return 0;
}

int RC4_IMPORT(struct ntlm_rc4_handle **_handle, struct ntlm_buffer *in)
{
    struct ntlm_rc4_handle *handle;
    RC4_INT *data = (RC4_INT *)in->data;
    int len = 258 * sizeof(RC4_INT);

    if (in->length != len) return EINVAL;

    handle = malloc(sizeof(struct ntlm_rc4_handle));
    if (!handle) return ENOMEM;

    handle->key.x = data[0];
    handle->key.y = data[1];
    memcpy(handle->key.data, &data[2], sizeof(RC4_INT) * 256);

    *_handle = handle;
    return 0;
}

int RC4K(struct ntlm_buffer *key,
         enum ntlm_cipher_mode mode,
         struct ntlm_buffer *payload,
         struct ntlm_buffer *result)
{
    struct ntlm_rc4_handle *handle;
    int ret;

    if (result->length < payload->length) return EINVAL;

    ret = RC4_INIT(key, mode, &handle);
    if (ret) return ret;

    ret = RC4_UPDATE(handle, payload, result);

    RC4_FREE(&handle);
    return ret;
}

int WEAK_DES(struct ntlm_buffer *key,
             struct ntlm_buffer *payload,
             struct ntlm_buffer *result)
{
    DES_key_schedule schedule;
    DES_cblock key8;

    if ((key->length != 7) ||
        (payload->length != 8) ||
        (result->length != 8)) {
        return EINVAL;
    }

    /* Undocumented shuffle needed before calling DES_set_key_unchecked */
    key8[0] =  key->data[0];
    key8[1] = (key->data[0] << 7) | (key->data[1] >> 1);
    key8[2] = (key->data[1] << 6) | (key->data[2] >> 2);
    key8[3] = (key->data[2] << 5) | (key->data[3] >> 3);
    key8[4] = (key->data[3] << 4) | (key->data[4] >> 4);
    key8[5] = (key->data[4] << 3) | (key->data[5] >> 5);
    key8[6] = (key->data[5] << 2) | (key->data[6] >> 6);
    key8[7] = (key->data[6] << 1);

    DES_set_key_unchecked(&key8, &schedule);
    DES_ecb_encrypt((DES_cblock *)payload->data,
                    (DES_cblock *)result->data, &schedule, 1);
    return 0;
}

int DESL(struct ntlm_buffer *key,
         struct ntlm_buffer *payload,
         struct ntlm_buffer *result)
{
    uint8_t buf7[7];
    struct ntlm_buffer key7;
    struct ntlm_buffer res8;

    if ((key->length != 16) ||
        (payload->length != 8) ||
        (result->length != 24)) {
        return EINVAL;
    }

    /* part 1 */
    key7.data = key->data;
    key7.length = 7;
    res8.data = result->data;
    res8.length = 8;
    WEAK_DES(&key7, payload, &res8);
    /* part 2 */
    key7.data = &key->data[7];
    key7.length = 7;
    res8.data = &result->data[8];
    res8.length = 8;
    WEAK_DES(&key7, payload, &res8);
    /* part 3 */
    memcpy(buf7, &key->data[14], 2);
    memset(&buf7[2], 0, 5);
    key7.data = buf7;
    key7.length = 7;
    res8.data = &result->data[16];
    res8.length = 8;
    WEAK_DES(&key7, payload, &res8);

    return 0;
}

uint32_t CRC32(uint32_t crc, struct ntlm_buffer *payload)
{
    return crc32(crc, payload->data, payload->length);
}
