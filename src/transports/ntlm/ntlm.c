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


/* This File implements the NTLM protocol as specified by:
 *      [MS-NLMP]: NT LAN Manager (NTLM) Authentication Protocol
 *
 * Additional cross checking with:
 * http://davenport.sourceforge.net/ntlm.html
 */

#include <alloca.h>
#include <endian.h>
#include <errno.h>
#include <iconv.h>
#include <stddef.h>
#include <string.h>
#include <sys/time.h>

#include <unicase.h>

#include "ntlm.h"

#pragma pack(push, 1)
struct wire_av_pair {
    uint16_t av_id;
    uint16_t av_len;
    uint8_t value[]; /* variable */
};
#pragma pack(pop)

enum msv_av_ids {
    MSV_AV_EOL = 0,
    MSV_AV_NB_COMPUTER_NAME,
    MSV_AV_NB_DOMAIN_NAME,
    MSV_AV_DNS_COMPUTER_NAME,
    MSV_AV_DNS_DOMAIN_NAME,
    MSV_AV_DNS_TREE_NAME,
    MSV_AV_FLAGS,
    MSV_AV_TIMESTAMP,
    MSV_AV_SINGLE_HOST,
    MSV_AV_TARGET_NAME,
    MSV_AV_CHANNEL_BINDINGS
};

/* Used only on the same host */
#pragma pack(push, 1)
struct wire_single_host_data {
    uint32_t size;
    uint32_t Z4;
    uint32_t data_present;
    uint32_t custom_data;
    uint8_t machine_id[32];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct wire_channel_binding {
    uint8_t md5_hash[16];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct wire_ntlm_cli_chal {
    uint8_t resp_type;
    uint8_t hi_resp_type;
    uint16_t reserved1;
    uint32_t reserved2;
    uint64_t timestamp;
    uint8_t cli_chal[8];
    uint32_t reserved3;
    uint8_t av_pairs[]; /* variable */
};
#pragma pack(pop)

struct ntlm_ctx {
    iconv_t from_oem;
    iconv_t to_oem;
};

int ntlm_init_ctx(struct ntlm_ctx **ctx)
{
    struct ntlm_ctx *_ctx;
    int ret = 0;

    _ctx = calloc(1, sizeof(struct ntlm_ctx));
    if (!_ctx) return ENOMEM;

    _ctx->from_oem = iconv_open("UCS-2LE", "UTF-8");
    if (_ctx->from_oem == (iconv_t) -1) {
        ret = errno;
    }

    _ctx->to_oem = iconv_open("UTF-8", "UCS-2LE");
    if (_ctx->to_oem == (iconv_t) -1) {
        iconv_close(_ctx->from_oem);
        ret = errno;
    }

    if (ret) {
        safefree(_ctx);
    } else {
        *ctx = _ctx;
    }
    return ret;
}

int ntlm_free_ctx(struct ntlm_ctx **ctx)
{
    int ret;

    if (!ctx || !*ctx) return 0;

    if ((*ctx)->from_oem) {
        ret = iconv_close((*ctx)->from_oem);
        if (ret) goto done;
    }

    if ((*ctx)->to_oem) {
        ret = iconv_close((*ctx)->to_oem);
    }

done:
    if (ret) ret = errno;
    safefree(*ctx);
    return ret;
}

void ntlm_free_buffer_data(struct ntlm_buffer *buf)
{
    if (!buf) return;

    safefree(buf->data);
    buf->length = 0;
}

/* A FILETIME structure is effectively a little endian 64 bit integer
 * with the time from January 1, 1601 UTC with 10s of microsecond resolution.
 */
#define FILETIME_EPOCH_VALUE 116444736000000000LL
uint64_t ntlm_timestamp_now(void)
{
    struct timeval tv;
    uint64_t filetime;

    gettimeofday(&tv, NULL);

    /* set filetime to the time representing the eopch */
    filetime = FILETIME_EPOCH_VALUE;
    /* add the number of seconds since the epoch */
    filetime += (uint64_t)tv.tv_sec * 10000000;
    /* add the number of microseconds since the epoch */
    filetime += tv.tv_usec * 10;

    return filetime;
}

bool ntlm_casecmp(const char *s1, const char *s2)
{
    size_t s1_len, s2_len;
    int ret, res;

    if (s1 == s2) return true;
    if (!s1 || !s2) return false;

    s1_len = strlen(s1);
    s2_len = strlen(s2);

    ret = ulc_casecmp(s1, s1_len, s2, s2_len,
                      uc_locale_language(), NULL, &res);
    if (ret || res != 0) return false;
    return true;
}


/**
 * @brief  Converts a string using the provided iconv context.
 *         This function is ok only to convert utf8<->ucs2
 *
 * @param cd        The iconv context
 * @param in        Input buffer
 * @param out       Output buffer
 * @param baselen   Input length
 * @param outlen    Returned length of out buffer
 *
 * NOTE: out must be preallocated to a size of baselen * 2
 *
 * @return 0 on success or a standard error value on error.
 */
static int ntlm_str_convert(iconv_t cd,
                            const char *in, char *out,
                            size_t baselen, size_t *outlen)
{
    char *_in;
    size_t inleft, outleft;
    size_t ret;

    ret = iconv(cd, NULL, NULL, NULL, NULL);
    if (ret == -1) return errno;

    _in = discard_const(in);
    inleft = baselen;
    /* conservative max_size calculation in case lots of octects end up
     * being multiple bytes in length (in both directions) */
    outleft = baselen * 2;

    ret = iconv(cd, &_in, &inleft, &out, &outleft);
    if (ret == -1) return errno;

    if (outlen) {
        *outlen = baselen * 2 - outleft;
    }
    return 0;
}


uint8_t ntlmssp_sig[8] = {'N', 'T', 'L', 'M', 'S', 'S', 'P', 0};

static void ntlm_encode_header(struct wire_msg_hdr *hdr, uint32_t msg_type)
{
    memcpy(hdr->signature, ntlmssp_sig, 8);
    hdr->msg_type = htole32(msg_type);
}

static int ntlm_decode_header(struct wire_msg_hdr *hdr, uint32_t *msg_type)
{
    if (memcmp(hdr->signature, ntlmssp_sig, 8) != 0) {
        return ERR_DECODE;
    }

    *msg_type = le32toh(hdr->msg_type);
    return 0;
}

static int ntlm_encode_oem_str(struct wire_field_hdr *hdr,
                               struct ntlm_buffer *buffer,
                               size_t *data_offs,
                               const char *str, int str_len)
{
    if (*data_offs + str_len > buffer->length) {
        return ERR_ENCODE;
    }

    memcpy(&buffer->data[*data_offs], str, str_len);
    hdr->len = htole16(str_len);
    hdr->max_len = htole16(str_len);
    hdr->offset = htole32(*data_offs);

    *data_offs += str_len;
    return 0;
}

static int ntlm_decode_oem_str(struct wire_field_hdr *str_hdr,
                               struct ntlm_buffer *buffer,
                               size_t payload_offs, char **_str)
{
    uint16_t str_len;
    uint32_t str_offs;
    char *str = NULL;

    str_len = le16toh(str_hdr->len);
    if (str_len == 0) goto done;

    str_offs = le32toh(str_hdr->offset);
    if ((str_offs < payload_offs) ||
        (str_offs > buffer->length) ||
        (str_offs + str_len > buffer->length)) {
        return ERR_DECODE;
    }

    str = strndup((const char *)&buffer->data[str_offs], str_len);
    if (!str) return ENOMEM;

done:
    *_str = str;
    return 0;
}

static int ntlm_encode_ucs2_str_hdr(struct ntlm_ctx *ctx,
                                    struct wire_field_hdr *hdr,
                                    struct ntlm_buffer *buffer,
                                    size_t *data_offs,
                                    const char *str, int str_len)
{
    char *out;
    size_t outlen;
    int ret;

    out = (char *)&buffer->data[*data_offs];

    ret = ntlm_str_convert(ctx->from_oem, str, out, str_len, &outlen);
    if (ret) return ret;

    hdr->len = htole16(outlen);
    hdr->max_len = htole16(outlen);
    hdr->offset = htole32(*data_offs);

    *data_offs += outlen;
    return 0;
}

static int ntlm_decode_ucs2_str_hdr(struct ntlm_ctx *ctx,
                                    struct wire_field_hdr *str_hdr,
                                    struct ntlm_buffer *buffer,
                                    size_t payload_offs, char **str)
{
    char *in, *out = NULL;
    uint16_t str_len;
    uint32_t str_offs;
    size_t outlen;
    int ret = 0;

    str_len = le16toh(str_hdr->len);
    if (str_len == 0) goto done;

    str_offs = le32toh(str_hdr->offset);
    if ((str_offs < payload_offs) ||
        (str_offs > buffer->length) ||
        (str_offs + str_len > buffer->length)) {
        return ERR_DECODE;
    }

    in = (char *)&buffer->data[str_offs];

    out = malloc(str_len * 2 + 1);
    if (!out) return ENOMEM;

    ret = ntlm_str_convert(ctx->to_oem, in, out, str_len, &outlen);

    /* make sure to terminate output string */
    out[outlen] = '\0';

done:
    if (ret) {
        safefree(out);
    }
    *str = out;
    return ret;
}

struct wire_version ntlmssp_version = {
    NTLMSSP_VERSION_MAJOR,
    NTLMSSP_VERSION_MINOR,
    NTLMSSP_VERSION_BUILD, /* 0 is always 0 even in LE */
    { 0, 0, 0 },
    NTLMSSP_VERSION_REV
};

void ntlm_internal_set_version(uint8_t major, uint8_t minor,
                               uint16_t build, uint8_t revision)
{
    ntlmssp_version.major = major;
    ntlmssp_version.minor = minor;
    ntlmssp_version.build = htole16(build);
    ntlmssp_version.revision = revision;
}

static int ntlm_encode_version(struct ntlm_ctx *ctx,
                               struct ntlm_buffer *buffer,
                               size_t *data_offs)
{
    if (*data_offs + sizeof(struct wire_version) > buffer->length) {
        return ERR_ENCODE;
    }

    memcpy(&buffer->data[*data_offs], &ntlmssp_version,
           sizeof(struct wire_version));
    *data_offs += sizeof(struct wire_version);
    return 0;
}

static int ntlm_encode_field(struct wire_field_hdr *hdr,
                             struct ntlm_buffer *buffer,
                             size_t *data_offs,
                             struct ntlm_buffer *field)
{
    if (*data_offs + field->length > buffer->length) {
        return ERR_ENCODE;
    }

    memcpy(&buffer->data[*data_offs], field->data, field->length);
    hdr->len = htole16(field->length);
    hdr->max_len = hdr->len;
    hdr->offset = htole32(*data_offs);

    *data_offs += field->length;
    return 0;
}

static int ntlm_decode_field(struct wire_field_hdr *hdr,
                             struct ntlm_buffer *buffer,
                             size_t payload_offs,
                             struct ntlm_buffer *field)
{
    struct ntlm_buffer b = { NULL, 0 };
    uint32_t offs;
    uint16_t len;

    len = le16toh(hdr->len);
    if (len == 0) goto done;

    offs = le32toh(hdr->offset);
    if ((offs < payload_offs) ||
        (offs > buffer->length) ||
        (offs + len > buffer->length)) {
        return ERR_DECODE;
    }

    b.data = malloc(len);
    if (!b.data) return ENOMEM;

    b.length = len;
    memcpy(b.data, &buffer->data[offs], b.length);

done:
    *field = b;
    return 0;
}

static int ntlm_encode_av_pair_ucs2_str(struct ntlm_ctx *ctx,
                                        struct ntlm_buffer *buffer,
                                        size_t *data_offs,
                                        enum msv_av_ids av_id,
                                        const char *str, size_t str_len)
{
    struct wire_av_pair *av_pair;
    char *out;
    size_t outlen;
    int ret;

    if (*data_offs + 4 + str_len > buffer->length) {
        return ERR_ENCODE;
    }

    av_pair = (struct wire_av_pair *)&buffer->data[*data_offs];
    out = (char *)av_pair->value;

    ret = ntlm_str_convert(ctx->from_oem, str, out, str_len, &outlen);
    if (ret) return ret;

    av_pair->av_len = htole16(outlen);
    av_pair->av_id = htole16(av_id);

    *data_offs += av_pair->av_len + 4;
    return 0;
}

static int ntlm_decode_av_pair_ucs2_str(struct ntlm_ctx *ctx,
                                        struct wire_av_pair *av_pair,
                                        char **str)
{
    char *in, *out;
    size_t inlen, outlen;
    int ret;

    in = (char *)av_pair->value;
    inlen = le16toh(av_pair->av_len);
    out = malloc(inlen * 2 + 1);

    ret = ntlm_str_convert(ctx->to_oem, in, out, inlen, &outlen);
    if (ret) {
        safefree(out);
        return ret;
    }
    /* terminate out string for sure */
    out[outlen] = '\0';

    *str = out;
    return 0;
}

static int ntlm_encode_av_pair_value(struct ntlm_buffer *buffer,
                                     size_t *data_offs,
                                     enum msv_av_ids av_id,
                                     struct ntlm_buffer *value)
{
    struct wire_av_pair *av_pair;

    if (*data_offs + 4 + value->length > buffer->length) {
        return ERR_ENCODE;
    }

    av_pair = (struct wire_av_pair *)&buffer->data[*data_offs];
    av_pair->av_id = htole16(av_id);
    av_pair->av_len = htole16(value->length);
    if (value->length) {
        memcpy(av_pair->value, value->data, value->length);
    }

    *data_offs += value->length + 4;
    return 0;
}

int ntlm_encode_target_info(struct ntlm_ctx *ctx, char *nb_computer_name,
                            char *nb_domain_name, char *dns_computer_name,
                            char *dns_domain_name, char *dns_tree_name,
                            uint32_t *av_flags, uint64_t *av_timestamp,
                            struct ntlm_buffer *av_single_host,
                            char *av_target_name, struct ntlm_buffer *av_cb,
                            struct ntlm_buffer *target_info)
{
    struct ntlm_buffer buffer;
    size_t data_offs;
    size_t max_size;
    size_t nb_computer_name_len = 0;
    size_t nb_domain_name_len = 0;
    size_t dns_computer_name_len = 0;
    size_t dns_domain_name_len = 0;
    size_t dns_tree_name_len = 0;
    size_t av_target_name_len = 0;
    struct ntlm_buffer value;
    int ret = 0;

    max_size = 4; /* MSV_AV_EOL */

    if (nb_computer_name) {
        nb_computer_name_len = strlen(nb_computer_name);
        max_size += 4 + nb_computer_name_len * 2;
    }
    if (nb_domain_name) {
        nb_domain_name_len = strlen(nb_domain_name);
        max_size += 4 + nb_domain_name_len * 2;
    }
    if (dns_computer_name) {
        dns_computer_name_len = strlen(dns_computer_name);
        max_size += 4 + dns_computer_name_len * 2;
    }
    if (dns_domain_name) {
        dns_domain_name_len = strlen(dns_domain_name);
        max_size += 4 + dns_domain_name_len * 2;
    }
    if (dns_tree_name) {
        dns_tree_name_len = strlen(dns_tree_name);
        max_size += 4 + dns_tree_name_len * 2;
    }
    if (av_flags) {
        max_size += 4 + 4;
    }
    if (av_timestamp) {
        max_size += 4 + 8;
    }
    if (av_single_host) {
        max_size += 4 + av_single_host->length;
    }
    if (av_target_name) {
        av_target_name_len = strlen(av_target_name);
        max_size += 4 + av_target_name_len * 2;
    }
    if (av_cb) {
        max_size += 4 + av_cb->length;
    }

    data_offs = 0;
    buffer.length = max_size;
    buffer.data = calloc(1, buffer.length);
    if (!buffer.data) return ENOMEM;

    if (nb_computer_name) {
        ret = ntlm_encode_av_pair_ucs2_str(ctx, &buffer, &data_offs,
                                           MSV_AV_NB_COMPUTER_NAME,
                                           nb_computer_name,
                                           nb_computer_name_len);
        if (ret) goto done;
    }
    if (nb_domain_name) {
        ret = ntlm_encode_av_pair_ucs2_str(ctx, &buffer, &data_offs,
                                           MSV_AV_NB_DOMAIN_NAME,
                                           nb_domain_name,
                                           nb_domain_name_len);
        if (ret) goto done;
    }
    if (dns_computer_name) {
        ret = ntlm_encode_av_pair_ucs2_str(ctx, &buffer, &data_offs,
                                           MSV_AV_DNS_COMPUTER_NAME,
                                           dns_computer_name,
                                           dns_computer_name_len);
        if (ret) goto done;
    }
    if (dns_domain_name) {
        ret = ntlm_encode_av_pair_ucs2_str(ctx, &buffer, &data_offs,
                                           MSV_AV_DNS_DOMAIN_NAME,
                                           dns_domain_name,
                                           dns_domain_name_len);
        if (ret) goto done;
    }
    if (dns_tree_name) {
        ret = ntlm_encode_av_pair_ucs2_str(ctx, &buffer, &data_offs,
                                           MSV_AV_DNS_TREE_NAME,
                                           dns_tree_name,
                                           dns_tree_name_len);
        if (ret) goto done;
    }
    if (av_flags) {
        uint32_t flags = htole32(*av_flags);
        value.data = (uint8_t *)&flags;
        value.length = 4;
        ret = ntlm_encode_av_pair_value(&buffer, &data_offs,
                                        MSV_AV_FLAGS, &value);
        if (ret) goto done;
    }
    if (av_timestamp) {
        uint64_t timestamp = htole64(*av_timestamp);
        value.data = (uint8_t *)&timestamp;
        value.length = 8;
        ret = ntlm_encode_av_pair_value(&buffer, &data_offs,
                                        MSV_AV_TIMESTAMP, &value);
        if (ret) goto done;
    }
    if (av_single_host) {
        ret = ntlm_encode_av_pair_value(&buffer, &data_offs,
                                        MSV_AV_SINGLE_HOST, av_single_host);
        if (ret) goto done;
    }
    if (av_target_name) {
        ret = ntlm_encode_av_pair_ucs2_str(ctx, &buffer, &data_offs,
                                           MSV_AV_TARGET_NAME,
                                           av_target_name,
                                           av_target_name_len);
        if (ret) goto done;
    }
    if (av_cb) {
        ret = ntlm_encode_av_pair_value(&buffer, &data_offs,
                                        MSV_AV_CHANNEL_BINDINGS, av_cb);
        if (ret) goto done;
    }

    value.length = 0;
    value.data = NULL;
    ret = ntlm_encode_av_pair_value(&buffer, &data_offs, MSV_AV_EOL, &value);
    buffer.length = data_offs;

done:
    if (ret) {
       safefree(buffer.data);
    } else {
        *target_info = buffer;
    }
    return ret;
}

int ntlm_decode_target_info(struct ntlm_ctx *ctx, struct ntlm_buffer *buffer,
                            char **nb_computer_name, char **nb_domain_name,
                            char **dns_computer_name, char **dns_domain_name,
                            char **dns_tree_name, char **av_target_name,
                            uint32_t *av_flags, uint64_t *av_timestamp,
                            struct ntlm_buffer *av_single_host,
                            struct ntlm_buffer *av_cb)
{
    struct wire_av_pair *av_pair;
    uint16_t av_id = (uint16_t)-1;
    uint16_t av_len = (uint16_t)-1;
    struct ntlm_buffer sh = { NULL, 0 };
    struct ntlm_buffer cb = { NULL, 0 };
    char *nb_computer = NULL;
    char *nb_domain = NULL;
    char *dns_computer = NULL;
    char *dns_domain = NULL;
    char *dns_tree = NULL;
    char *av_target = NULL;
    size_t data_offs = 0;
    uint64_t timestamp = 0;
    uint32_t flags = 0;
    int ret = 0;

    while (data_offs + 4 <= buffer->length) {
        av_pair = (struct wire_av_pair *)&buffer->data[data_offs];
        data_offs += 4;
        av_id = le16toh(av_pair->av_id);
        av_len = le16toh(av_pair->av_len);
        if (av_len > buffer->length - data_offs) {
            ret = ERR_DECODE;
            goto done;
        }
        data_offs += av_len;

        switch (av_id) {
        case MSV_AV_CHANNEL_BINDINGS:
            if (!av_cb) continue;
            cb.data = av_pair->value;
            cb.length = av_len;
            break;
        case MSV_AV_TARGET_NAME:
            if (!av_target_name) continue;
            ret = ntlm_decode_av_pair_ucs2_str(ctx, av_pair, &av_target);
            if (ret) goto done;
            break;
        case MSV_AV_SINGLE_HOST:
            if (!av_single_host) continue;
            sh.data = av_pair->value;
            sh.length = av_len;
            break;
        case MSV_AV_TIMESTAMP:
            if (!av_timestamp) continue;
            memcpy(&timestamp, av_pair->value, sizeof(timestamp));
            timestamp = le64toh(timestamp);
            break;
        case MSV_AV_FLAGS:
            if (!av_flags) continue;
            memcpy(&flags, av_pair->value, sizeof(flags));
            flags = le32toh(flags);
            break;
        case MSV_AV_DNS_TREE_NAME:
            if (!dns_tree_name) continue;
            ret = ntlm_decode_av_pair_ucs2_str(ctx, av_pair, &dns_tree);
            if (ret) goto done;
            break;
        case MSV_AV_DNS_DOMAIN_NAME:
            if (!dns_domain_name) continue;
            ret = ntlm_decode_av_pair_ucs2_str(ctx, av_pair, &dns_domain);
            if (ret) goto done;
            break;
        case MSV_AV_DNS_COMPUTER_NAME:
            if (!dns_computer_name) continue;
            ret = ntlm_decode_av_pair_ucs2_str(ctx, av_pair, &dns_computer);
            if (ret) goto done;
            break;
        case MSV_AV_NB_DOMAIN_NAME:
            if (!nb_domain_name) continue;
            ret = ntlm_decode_av_pair_ucs2_str(ctx, av_pair, &nb_domain);
            if (ret) goto done;
            break;
        case MSV_AV_NB_COMPUTER_NAME:
            if (!nb_computer_name) continue;
            ret = ntlm_decode_av_pair_ucs2_str(ctx, av_pair, &nb_computer);
            if (ret) goto done;
            break;
        default:
            /* unknown av_pair, or EOL */
            break;
        }
        if (av_id == MSV_AV_EOL) break;
    }

    if (av_id != MSV_AV_EOL || av_len != 0) {
        ret = ERR_DECODE;
    }

done:
    if (ret) {
        ntlm_free_buffer_data(&sh);
        ntlm_free_buffer_data(&cb);
        safefree(nb_computer);
        safefree(nb_domain);
        safefree(dns_computer);
        safefree(dns_domain);
        safefree(dns_tree);
        safefree(av_target);
    } else {
        if (nb_computer_name) *nb_computer_name = nb_computer;
        if (nb_domain_name) *nb_domain_name = nb_domain;
        if (dns_computer_name) *dns_computer_name = dns_computer;
        if (dns_domain_name) *dns_domain_name = dns_domain;
        if (dns_tree_name) *dns_tree_name = dns_tree;
        if (av_target_name) *av_target_name = av_target;
        if (av_timestamp) *av_timestamp = timestamp;
        if (av_single_host) *av_single_host = sh;
        if (av_flags) *av_flags = flags;
        if (av_cb) *av_cb = cb;
    }
    return ret;
}

int ntlm_process_target_info(struct ntlm_ctx *ctx, bool protect,
                             struct ntlm_buffer *in,
                             const char *server,
                             struct ntlm_buffer *unhashed_cb,
                             struct ntlm_buffer *out,
                             uint64_t *out_srv_time,
                             bool *add_mic)
{
    char *nb_computer_name = NULL;
    char *nb_domain_name = NULL;
    char *dns_computer_name = NULL;
    char *dns_domain_name = NULL;
    char *dns_tree_name = NULL;
    char *av_target_name = NULL;
    uint32_t av_flags = 0;
    uint64_t srv_time = 0;
    uint8_t cb[16] = { 0 };
    struct ntlm_buffer av_cb = { cb, 16 };
    int ret = 0;

    /* TODO: check that returned netbios/dns names match ? */
    /* TODO: support SingleHost buffers */
    ret = ntlm_decode_target_info(ctx, in,
                                  &nb_computer_name, &nb_domain_name,
                                  &dns_computer_name, &dns_domain_name,
                                  &dns_tree_name, &av_target_name,
                                  &av_flags, &srv_time, NULL, NULL);
    if (ret) goto done;

    if (protect && (!nb_computer_name || nb_computer_name[0] == '\0')) {
        ret = EINVAL;
        goto done;
    }

    if (server && av_target_name) {
        if (strcasecmp(server, av_target_name) != 0) {
            ret = EINVAL;
            goto done;
        }
    }

    /* the server did not send the timestamp, use current time */
    if (srv_time == 0) {
        srv_time = ntlm_timestamp_now();
    } else if (add_mic) {
        av_flags |= MSVAVFLAGS_MIC_PRESENT;
        *add_mic = true;
    }

    if (unhashed_cb->length > 0) {
        ret = ntlm_hash_channel_bindings(unhashed_cb, &av_cb);
        if (ret) goto done;
    }

    if (!av_target_name && server) {
        av_target_name = strdup(server);
        if (!av_target_name) {
            ret = ENOMEM;
            goto done;
        }
    }
    /* TODO: add way to tell if the target name is verified o not,
     * if not set av_flags |= MSVAVFLAGS_UNVERIFIED_SPN; */

    ret = ntlm_encode_target_info(ctx,
                                  nb_computer_name, nb_domain_name,
                                  dns_computer_name, dns_domain_name,
                                  dns_tree_name, &av_flags, &srv_time,
                                  NULL, av_target_name, &av_cb, out);

done:
    safefree(nb_computer_name);
    safefree(nb_domain_name);
    safefree(dns_computer_name);
    safefree(dns_domain_name);
    safefree(dns_tree_name);
    safefree(av_target_name);
    *out_srv_time = srv_time;
    return ret;
}

int ntlm_decode_msg_type(struct ntlm_ctx *ctx,
                         struct ntlm_buffer *buffer,
                         uint32_t *type)
{
    struct wire_neg_msg *msg;
    uint32_t msg_type;
    int ret;

    if (!ctx) return EINVAL;

    if (buffer->length < sizeof(struct wire_msg_hdr)) {
        return ERR_DECODE;
    }

    msg = (struct wire_neg_msg *)buffer->data;

    ret = ntlm_decode_header(&msg->header, &msg_type);
    if (ret) goto done;

    switch (msg_type) {
    case NEGOTIATE_MESSAGE:
        if (buffer->length < sizeof(struct wire_neg_msg)) {
            return ERR_DECODE;
        }
        break;
    case CHALLENGE_MESSAGE:
        if (buffer->length < sizeof(struct wire_chal_msg) &&
            buffer->length != sizeof(struct wire_chal_msg_old)) {
            return ERR_DECODE;
        }
        break;
    case AUTHENTICATE_MESSAGE:
        if (buffer->length < sizeof(struct wire_auth_msg)) {
            return ERR_DECODE;
        }
        break;
    default:
        ret = ERR_DECODE;
        break;
    }

done:
    if (ret == 0) {
        *type = msg_type;
    }
    return ret;
}

int ntlm_encode_neg_msg(struct ntlm_ctx *ctx, uint32_t flags,
                        const char *domain, const char *workstation,
                        struct ntlm_buffer *message)
{
    struct wire_neg_msg *msg;
    struct ntlm_buffer buffer;
    size_t data_offs;
    size_t dom_len = 0;
    size_t wks_len = 0;
    int ret = 0;

    if (!ctx) return EINVAL;

    buffer.length = sizeof(struct wire_neg_msg);

    /* Strings MUST use OEM charset in negotiate message */
    if (flags & NTLMSSP_NEGOTIATE_OEM_DOMAIN_SUPPLIED) {
        if (!domain) return EINVAL;
        dom_len = strlen(domain);
        buffer.length += dom_len;
    }
    if (flags & NTLMSSP_NEGOTIATE_OEM_WORKSTATION_SUPPLIED) {
        if (!workstation) return EINVAL;
        wks_len = strlen(workstation);
        buffer.length += wks_len;
    }

    buffer.data = calloc(1, buffer.length);
    if (!buffer.data) return ENOMEM;

    msg = (struct wire_neg_msg *)buffer.data;
    data_offs = (char *)msg->payload - (char *)msg;

    ntlm_encode_header(&msg->header, NEGOTIATE_MESSAGE);

    msg->neg_flags = htole32(flags);

    if (dom_len) {
        ret = ntlm_encode_oem_str(&msg->domain_name, &buffer,
                                  &data_offs, domain, dom_len);
        if (ret) goto done;
    }

    if (wks_len) {
        ret = ntlm_encode_oem_str(&msg->workstation_name, &buffer,
                                  &data_offs, workstation, wks_len);
        if (ret) goto done;
    }

done:
    if (ret) {
        safefree(buffer.data);
    } else {
        *message = buffer;
    }
    return ret;
}

int ntlm_decode_neg_msg(struct ntlm_ctx *ctx,
                        struct ntlm_buffer *buffer, uint32_t *flags,
                        char **domain, char **workstation)
{
    struct wire_neg_msg *msg;
    size_t payload_offs;
    uint32_t neg_flags;
    char *dom = NULL;
    char *wks = NULL;
    int ret = 0;

    if (!ctx) return EINVAL;

    msg = (struct wire_neg_msg *)buffer->data;
    payload_offs = (char *)msg->payload - (char *)msg;

    neg_flags = le32toh(msg->neg_flags);

    if (domain &&
        (neg_flags & NTLMSSP_NEGOTIATE_OEM_DOMAIN_SUPPLIED)) {
        ret = ntlm_decode_oem_str(&msg->domain_name, buffer,
                                  payload_offs, &dom);
        if (ret) goto done;
    }
    if (workstation &&
        (neg_flags & NTLMSSP_NEGOTIATE_OEM_WORKSTATION_SUPPLIED)) {
        ret = ntlm_decode_oem_str(&msg->workstation_name, buffer,
                                  payload_offs, &wks);
        if (ret) goto done;
    }

done:
    if (ret) {
        safefree(dom);
        safefree(wks);
    } else {
        *flags = neg_flags;
        if (domain) *domain = dom;
        if (workstation) *workstation = wks;
    }
    return ret;
}

/* TODO: support datagram style */
int ntlm_encode_chal_msg(struct ntlm_ctx *ctx,
                         uint32_t flags,
                         const char *target_name,
                         struct ntlm_buffer *challenge,
                         struct ntlm_buffer *target_info,
                         struct ntlm_buffer *message)
{
    struct wire_chal_msg *msg;
    struct ntlm_buffer buffer;
    size_t data_offs;
    size_t target_len = 0;
    int ret = 0;

    if (!ctx) return EINVAL;

    if (!challenge || challenge->length != 8) return EINVAL;

    buffer.length = sizeof(struct wire_chal_msg);

    if (flags & NTLMSSP_NEGOTIATE_VERSION) {
        buffer.length += sizeof(struct wire_version);
    }

    if ((flags & NTLMSSP_TARGET_TYPE_SERVER)
        || (flags & NTLMSSP_TARGET_TYPE_DOMAIN)) {
        if (!target_name) return EINVAL;

        target_len = strlen(target_name);
        if (flags & NTLMSSP_NEGOTIATE_UNICODE) {
            buffer.length += target_len * 2;
        } else {
            buffer.length += target_len;
        }
    }

    if (flags & NTLMSSP_NEGOTIATE_TARGET_INFO) {
        if (!target_info) return EINVAL;

        buffer.length += target_info->length;
    }

    buffer.data = calloc(1, buffer.length);
    if (!buffer.data) return ENOMEM;

    msg = (struct wire_chal_msg *)buffer.data;
    data_offs = (char *)msg->payload - (char *)msg;

    ntlm_encode_header(&msg->header, CHALLENGE_MESSAGE);

    /* this must be first as it pushes the payload further down */
    if (flags & NTLMSSP_NEGOTIATE_VERSION) {
        ret = ntlm_encode_version(ctx, &buffer, &data_offs);
        if (ret) goto done;
    }

    if ((flags & NTLMSSP_TARGET_TYPE_SERVER)
        || (flags & NTLMSSP_TARGET_TYPE_DOMAIN)) {
        if (flags & NTLMSSP_NEGOTIATE_UNICODE) {
            ret = ntlm_encode_ucs2_str_hdr(ctx, &msg->target_name, &buffer,
                                           &data_offs, target_name, target_len);
        } else {
            ret = ntlm_encode_oem_str(&msg->target_name, &buffer,
                                      &data_offs, target_name, target_len);
        }
        if (ret) goto done;
    }

    msg->neg_flags = htole32(flags);
    memcpy(msg->server_challenge, challenge->data, 8);

    if (flags & NTLMSSP_NEGOTIATE_TARGET_INFO) {
        ret = ntlm_encode_field(&msg->target_info, &buffer,
                                &data_offs, target_info);
        if (ret) goto done;
    }

done:
    if (ret) {
        safefree(buffer.data);
    } else {
        *message = buffer;
    }
    return ret;
}

int ntlm_decode_chal_msg(struct ntlm_ctx *ctx,
                         struct ntlm_buffer *buffer,
                         uint32_t *_flags, char **target_name,
                         struct ntlm_buffer *challenge,
                         struct ntlm_buffer *target_info)
{
    struct wire_chal_msg *msg;
    size_t payload_offs;
    uint32_t flags;
    char *trg = NULL;
    int ret = 0;

    if (!ctx) return EINVAL;

    if (challenge->length < 8) return EINVAL;

    msg = (struct wire_chal_msg *)buffer->data;
    payload_offs = (char *)msg->payload - (char *)msg;

    flags = le32toh(msg->neg_flags);

    if ((flags & NTLMSSP_TARGET_TYPE_SERVER)
        || (flags & NTLMSSP_TARGET_TYPE_DOMAIN)) {
        if (flags & NTLMSSP_NEGOTIATE_UNICODE) {
            ret = ntlm_decode_ucs2_str_hdr(ctx, &msg->target_name, buffer,
                                           payload_offs, &trg);
        } else {
            ret = ntlm_decode_oem_str(&msg->target_name, buffer,
                                      payload_offs, &trg);
        }
        if (ret) goto done;
    }

    memcpy(challenge->data, msg->server_challenge, 8);
    challenge->length = 8;

    /* if we allowed a broken short challenge message from an old
     * server we must stop here */
    if (buffer->length < sizeof(struct wire_chal_msg)) {
        if (flags & NTLMSSP_NEGOTIATE_TARGET_INFO) {
            ret = ERR_DECODE;
        }
        goto done;
    }

    if (flags & NTLMSSP_NEGOTIATE_TARGET_INFO) {
        ret = ntlm_decode_field(&msg->target_info, buffer,
                                payload_offs, target_info);
        if (ret) goto done;
    }

done:
    if (ret) {
        safefree(trg);
    } else {
        *_flags = flags;
        *target_name = trg;
    }
    return ret;
}

int ntlm_encode_auth_msg(struct ntlm_ctx *ctx,
                         uint32_t flags,
                         struct ntlm_buffer *lm_chalresp,
                         struct ntlm_buffer *nt_chalresp,
                         char *domain_name, char *user_name,
                         char *workstation,
                         struct ntlm_buffer *enc_sess_key,
                         struct ntlm_buffer *mic,
                         struct ntlm_buffer *message)
{
    struct wire_auth_msg *msg;
    struct ntlm_buffer buffer;
    struct ntlm_buffer empty_chalresp = { 0 };
    size_t data_offs;
    size_t domain_name_len = 0;
    size_t user_name_len = 0;
    size_t workstation_len = 0;
    int ret = 0;

    if (!ctx) return EINVAL;

    buffer.length = sizeof(struct wire_auth_msg);

    if (lm_chalresp) {
        buffer.length += lm_chalresp->length;
    } else {
        lm_chalresp = &empty_chalresp;
    }
    if (nt_chalresp) {
        buffer.length += nt_chalresp->length;
    } else {
        nt_chalresp = &empty_chalresp;
    }
    if (domain_name) {
        domain_name_len = strlen(domain_name);
        if (flags & NTLMSSP_NEGOTIATE_UNICODE) {
            buffer.length += domain_name_len * 2;
        } else {
            buffer.length += domain_name_len;
        }
    }
    if (user_name) {
        user_name_len = strlen(user_name);
        if (flags & NTLMSSP_NEGOTIATE_UNICODE) {
            buffer.length += user_name_len * 2;
        } else {
            buffer.length += user_name_len;
        }
    }
    if (workstation) {
        workstation_len = strlen(workstation);
        if (flags & NTLMSSP_NEGOTIATE_UNICODE) {
            buffer.length += workstation_len * 2;
        } else {
            buffer.length += workstation_len;
        }
    }
    if (enc_sess_key) {
        buffer.length += enc_sess_key->length;
    }
    if (flags & NTLMSSP_NEGOTIATE_VERSION) {
        buffer.length += sizeof(struct wire_version);
    }
    if (mic) {
        buffer.length += 16;
    }

    buffer.data = calloc(1, buffer.length);
    if (!buffer.data) return ENOMEM;

    msg = (struct wire_auth_msg *)buffer.data;
    data_offs = (char *)msg->payload - (char *)msg;

    ntlm_encode_header(&msg->header, AUTHENTICATE_MESSAGE);

    /* this must be first as it pushes the payload further down */
    if (flags & NTLMSSP_NEGOTIATE_VERSION) {
        ret = ntlm_encode_version(ctx, &buffer, &data_offs);
        if (ret) goto done;
    }

    /* this must be second as it pushes the payload further down */
    if (mic) {
        memset(&buffer.data[data_offs], 0, mic->length);
        /* return the actual pointer back in the mic, as it will
         * be backfilled later by the caller */
        mic->data = &buffer.data[data_offs];
        data_offs += mic->length;
    }

    ret = ntlm_encode_field(&msg->lm_chalresp, &buffer,
                            &data_offs, lm_chalresp);
    if (ret) goto done;

    ret = ntlm_encode_field(&msg->nt_chalresp, &buffer,
                            &data_offs, nt_chalresp);
    if (ret) goto done;

    if (domain_name_len) {
        if (flags & NTLMSSP_NEGOTIATE_UNICODE) {
            ret = ntlm_encode_ucs2_str_hdr(ctx, &msg->domain_name,
                                           &buffer, &data_offs,
                                           domain_name, domain_name_len);
        } else {
            ret = ntlm_encode_oem_str(&msg->domain_name,
                                      &buffer, &data_offs,
                                      domain_name, domain_name_len);
        }
        if (ret) goto done;
    }
    if (user_name_len) {
        if (flags & NTLMSSP_NEGOTIATE_UNICODE) {
            ret = ntlm_encode_ucs2_str_hdr(ctx, &msg->user_name,
                                           &buffer, &data_offs,
                                           user_name, user_name_len);
        } else {
            ret = ntlm_encode_oem_str(&msg->user_name,
                                      &buffer, &data_offs,
                                      user_name, user_name_len);
        }
        if (ret) goto done;
    }
    if (workstation_len) {
        if (flags & NTLMSSP_NEGOTIATE_UNICODE) {
            ret = ntlm_encode_ucs2_str_hdr(ctx, &msg->workstation,
                                           &buffer, &data_offs,
                                           workstation, workstation_len);
        } else {
            ret = ntlm_encode_oem_str(&msg->workstation,
                                      &buffer, &data_offs,
                                      workstation, workstation_len);
        }
        if (ret) goto done;
    }
    if (enc_sess_key) {
        ret = ntlm_encode_field(&msg->enc_sess_key, &buffer,
                                &data_offs, enc_sess_key);
        if (ret) goto done;
    }

    msg->neg_flags = htole32(flags);

done:
    if (ret) {
        safefree(buffer.data);
    } else {
        *message = buffer;
    }
    return ret;
}

int ntlm_decode_auth_msg(struct ntlm_ctx *ctx,
                         struct ntlm_buffer *buffer,
                         uint32_t flags,
                         struct ntlm_buffer *lm_chalresp,
                         struct ntlm_buffer *nt_chalresp,
                         char **domain_name, char **user_name,
                         char **workstation,
                         struct ntlm_buffer *enc_sess_key,
                         struct ntlm_buffer *target_info,
                         struct ntlm_buffer *mic)
{
    struct wire_auth_msg *msg;
    size_t payload_offs;
    char *dom = NULL;
    char *usr = NULL;
    char *wks = NULL;
    int ret = 0;

    if (!ctx) return EINVAL;

    if (lm_chalresp) lm_chalresp->data = NULL;
    if (nt_chalresp) nt_chalresp->data = NULL;
    if (enc_sess_key) enc_sess_key->data = NULL;

    msg = (struct wire_auth_msg *)buffer->data;
    payload_offs = (char *)msg->payload - (char *)msg;

    /* this must be first as it pushes the payload further down */
    if (flags & NTLMSSP_NEGOTIATE_VERSION) {
        /* skip version for now */
        payload_offs += sizeof(struct wire_version);
    }

    /* Unconditionally copy 16 bytes for the MIC, if it was really
     * added by the client it will be flagged in the AV_PAIR contained
     * in the NT Response, that will be fully decoded later by the caller
     * and the MIC checked otherwise these 16 bytes will just be ignored */
    if (mic) {
        if (mic->length < 16) return ERR_DECODE;
        /* mic is at payload_offs right now */
        if (buffer->length - payload_offs < 16) return ERR_DECODE;
        memcpy(mic->data, &buffer->data[payload_offs], 16);
        /* NOTE: we do not push down the payload because we do not know that
         * the MIC is actually present yet for real */
    }

    if (msg->lm_chalresp.len != 0 && lm_chalresp) {
        ret = ntlm_decode_field(&msg->lm_chalresp, buffer,
                                payload_offs, lm_chalresp);
        if (ret) goto done;
    }
    if (msg->nt_chalresp.len != 0 && nt_chalresp) {
        ret = ntlm_decode_field(&msg->nt_chalresp, buffer,
                                payload_offs, nt_chalresp);
        if (ret) goto done;

        if (target_info) {
            union wire_ntlm_response *resp;
            struct wire_ntlmv2_cli_chal *chal;
            uint8_t *data;
            int len;
            resp = (union wire_ntlm_response *)nt_chalresp->data;
            chal = (struct wire_ntlmv2_cli_chal *)resp->v2.cli_chal;
            len = nt_chalresp->length - sizeof(resp->v2.resp)
                    - offsetof(struct wire_ntlmv2_cli_chal, target_info);
            if (len > 0) {
                data = chal->target_info;
                target_info->data = malloc(len);
                if (!target_info->data) {
                    ret = ENOMEM;
                    goto done;
                }
                memcpy(target_info->data, data, len);
                target_info->length = len;
            }
        }
    }
    if (msg->domain_name.len != 0 && domain_name) {
        if (flags & NTLMSSP_NEGOTIATE_UNICODE) {
            ret = ntlm_decode_ucs2_str_hdr(ctx, &msg->domain_name, buffer,
                                           payload_offs, &dom);
        } else {
            ret = ntlm_decode_oem_str(&msg->domain_name, buffer,
                                      payload_offs, &dom);
        }
        if (ret) goto done;
    }
    if (msg->user_name.len != 0 && user_name) {
        if (flags & NTLMSSP_NEGOTIATE_UNICODE) {
            ret = ntlm_decode_ucs2_str_hdr(ctx, &msg->user_name, buffer,
                                           payload_offs, &usr);
        } else {
            ret = ntlm_decode_oem_str(&msg->user_name, buffer,
                                      payload_offs, &usr);
        }
        if (ret) goto done;
    }
    if (msg->workstation.len != 0 && workstation) {
        if (flags & NTLMSSP_NEGOTIATE_UNICODE) {
            ret = ntlm_decode_ucs2_str_hdr(ctx, &msg->workstation, buffer,
                                           payload_offs, &wks);
        } else {
            ret = ntlm_decode_oem_str(&msg->workstation, buffer,
                                      payload_offs, &wks);
        }
        if (ret) goto done;
    }
    if (msg->enc_sess_key.len != 0 && enc_sess_key) {
        ret = ntlm_decode_field(&msg->enc_sess_key, buffer,
                                payload_offs, enc_sess_key);
    }

    /* ignore returned flags, our flags are authoritative
    flags = le32toh(msg->neg_flags);
    */

done:
    if (ret) {
        if (lm_chalresp) safefree(lm_chalresp->data);
        if (nt_chalresp) safefree(nt_chalresp->data);
        if (enc_sess_key) safefree(enc_sess_key->data);
        safefree(dom);
        safefree(usr);
        safefree(wks);
    } else {
        if (domain_name) *domain_name = dom;
        if (user_name) *user_name = usr;
        if (workstation) *workstation = wks;
    }
    return ret;
}
