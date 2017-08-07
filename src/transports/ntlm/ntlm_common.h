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

#ifndef _NTLM_COMMON_H_
#define _NTLM_COMMON_H_

#include <stdint.h>
#include <stdlib.h>

enum ntlm_err_code {
    ERR_BASE = 0x4E540000, /* base error space at 'NT00' */
    ERR_DECODE,
    ERR_ENCODE,
    ERR_CRYPTO,
    ERR_NOARG,
    ERR_BADARG,
    ERR_NONAME,
    ERR_NOSRVNAME,
    ERR_NOUSRNAME,
    ERR_BADLMLVL,
    ERR_IMPOSSIBLE,
    ERR_BADCTX,
    ERR_WRONGCTX,
    ERR_WRONGMSG,
    ERR_REQNEGFLAG,
    ERR_FAILNEGFLAGS,
    ERR_BADNEGFLAGS,
    ERR_NOSRVCRED,
    ERR_NOUSRCRED,
    ERR_BADCRED,
    ERR_NOTOKEN,
    ERR_NOTSUPPORTED,
    ERR_NOTAVAIL,
    ERR_NAMETOOLONG,
    ERR_NOBINDINGS,
    ERR_TIMESKEW,
    ERR_EXPIRED,
    ERR_KEYLEN,
    ERR_NONTLMV1,
    ERR_NOUSRFOUND,
    ERR_LAST
};
#define NTLM_ERR_MASK 0x4E54FFFF
#define IS_NTLM_ERR_CODE(x) (((x) & NTLM_ERR_MASK) ? true : false)

#define discard_const(ptr) ((void *)((uintptr_t)(ptr)))
#define safefree(x) do { free(x); x = NULL; } while(0)
#define safezero(x, s) do { \
    volatile uint8_t *p = (x); \
    size_t size = (s); \
    while (size--) { *p++ = 0; } \
} while(0)


struct ntlm_buffer {
    uint8_t *data;
    size_t length;
};

struct ntlm_iov {
    struct ntlm_buffer **data;
    size_t num;
};

struct ntlm_rc4_handle;

enum ntlm_cipher_mode {
    NTLM_CIPHER_IGNORE,
    NTLM_CIPHER_ENCRYPT,
    NTLM_CIPHER_DECRYPT,
};

#pragma pack(push, 1)
struct wire_msg_hdr {
    uint8_t signature[8];
    uint32_t msg_type;
};
#pragma pack(pop)

/* A wire string, the offset is relative to the mesage and must fall into the
 * payload section.
 * max_len should be set equal to len and ignored by servers.
 */
#pragma pack(push, 1)
struct wire_field_hdr {
    uint16_t len;
    uint16_t max_len;
    uint32_t offset;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct wire_neg_msg {
    struct wire_msg_hdr header;
    uint32_t neg_flags;
    struct wire_field_hdr domain_name;
    struct wire_field_hdr workstation_name;
    uint8_t payload[]; /* variable */
};
#pragma pack(pop)

#pragma pack(push, 1)
struct wire_chal_msg {
    struct wire_msg_hdr header;
    struct wire_field_hdr target_name;
    uint32_t neg_flags;
    uint8_t server_challenge[8];
    uint8_t reserved[8];
    struct wire_field_hdr target_info;
    uint8_t payload[]; /* variable */
};
#pragma pack(pop)

/* We have evidence of at least one old broken server
 * that send shorter CHALLENGE msgs like this: */
#pragma pack(push, 1)
struct wire_chal_msg_old {
    struct wire_msg_hdr header;
    struct wire_field_hdr target_name;
    uint32_t neg_flags;
    uint8_t server_challenge[8];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct wire_auth_msg {
    struct wire_msg_hdr header;
    struct wire_field_hdr lm_chalresp;
    struct wire_field_hdr nt_chalresp;
    struct wire_field_hdr domain_name;
    struct wire_field_hdr user_name;
    struct wire_field_hdr workstation;
    struct wire_field_hdr enc_sess_key;
    uint32_t neg_flags;
    uint8_t payload[]; /* variable */
};
#pragma pack(pop)

/* Version information.
 * Used only for debugging and usually placed as the head of the payload when
 * used */
#pragma pack(push, 1)
struct wire_version {
    uint8_t major;
    uint8_t minor;
    uint16_t build;
    uint8_t reserved[3];
    uint8_t revision;
};
#pragma pack(pop)

/* ln/ntlm response, v1 or v2 */
#pragma pack(push, 1)
union wire_ntlm_response {
    struct {
        uint8_t resp[24];
    } v1;
    struct {
        uint8_t resp[16];
        uint8_t cli_chal[];
    } v2;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct wire_ntlmv2_cli_chal {
    uint8_t resp_version;
    uint8_t hi_resp_version;
    uint8_t zero_6[6];
    uint64_t timestamp;
    uint8_t client_chal[8];
    uint8_t zero_4[4];
    uint8_t target_info[];
        /* NOTE: the target_info array must terminate with 4 zero bytes.
         * This is consistent with just copying the target_info array
         * returned in the challenge message as the last AV_PAIR there is
         * always MSV_AV_EOL which happens to be 4 bytes of zeros */

};
#pragma pack(pop)

#endif /* _NTLM_COMMON_H_ */
