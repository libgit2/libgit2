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

#ifndef _NTLM_H_
#define _NTLM_H

#include <stdbool.h>

#include "ntlm_common.h"

/* Negotiate Flags */
#define NTLMSSP_NEGOTIATE_56                        (1 << 31)
#define NTLMSSP_NEGOTIATE_KEY_EXCH                  (1 << 30)
#define NTLMSSP_NEGOTIATE_128                       (1 << 29)
#define UNUSED_R1                                   (1 << 28)
#define UNUSED_R2                                   (1 << 27)
#define UNUSED_R3                                   (1 << 26)
#define NTLMSSP_NEGOTIATE_VERSION                   (1 << 25)
#define UNUSED_R4                                   (1 << 24)
#define NTLMSSP_NEGOTIATE_TARGET_INFO               (1 << 23)
#define NTLMSSP_REQUEST_NON_NT_SESSION_KEY          (1 << 22)
#define UNUSED_R5 /* Davenport: NEGOTIATE_ACCEPT */ (1 << 21)
#define NTLMSSP_NEGOTIATE_IDENTIFY                  (1 << 20)
#define NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY  (1 << 19)
#define UNUSED_R6 /* Davenport:TARGET_TYPE_SHARE */ (1 << 18)
#define NTLMSSP_TARGET_TYPE_SERVER                  (1 << 17)
#define NTLMSSP_TARGET_TYPE_DOMAIN                  (1 << 16)
#define NTLMSSP_NEGOTIATE_ALWAYS_SIGN               (1 << 15)
#define UNUSED_R7 /* Davenport:LOCAL_CALL */        (1 << 14)
#define NTLMSSP_NEGOTIATE_OEM_WORKSTATION_SUPPLIED  (1 << 13)
#define NTLMSSP_NEGOTIATE_OEM_DOMAIN_SUPPLIED       (1 << 12)
#define NTLMSSP_ANONYMOUS                           (1 << 11)
#define UNUSED_R8                                   (1 << 10)
#define NTLMSSP_NEGOTIATE_NTLM                      (1 << 9)
#define UNUSED_R9                                   (1 << 8)
#define NTLMSSP_NEGOTIATE_LM_KEY                    (1 << 7)
#define NTLMSSP_NEGOTIATE_DATAGRAM                  (1 << 6)
#define NTLMSSP_NEGOTIATE_SEAL                      (1 << 5)
#define NTLMSSP_NEGOTIATE_SIGN                      (1 << 4)
#define UNUSED_R10                                  (1 << 3)
#define NTLMSSP_REQUEST_TARGET                      (1 << 2)
#define NTLMSSP_NEGOTIATE_OEM                       (1 << 1)
#define NTLMSSP_NEGOTIATE_UNICODE                   (1 << 0)

/* (2.2.2.10 VERSION) */
#define WINDOWS_MAJOR_VERSION_5 0x05
#define WINDOWS_MAJOR_VERSION_6 0x06
#define WINDOWS_MINOR_VERSION_0 0x00
#define WINDOWS_MINOR_VERSION_1 0x01
#define WINDOWS_MINOR_VERSION_2 0x02
#define NTLMSSP_REVISION_W2K3 0x0F

#define NTLMSSP_VERSION_MAJOR WINDOWS_MAJOR_VERSION_6
#define NTLMSSP_VERSION_MINOR WINDOWS_MINOR_VERSION_2
#define NTLMSSP_VERSION_BUILD 0
#define NTLMSSP_VERSION_REV NTLMSSP_REVISION_W2K3

#define NTLMSSP_MESSAGE_SIGNATURE_VERSION 0x00000001

#define NEGOTIATE_MESSAGE       0x00000001
#define CHALLENGE_MESSAGE       0x00000002
#define AUTHENTICATE_MESSAGE    0x00000003

#define MSVAVFLAGS_AUTH_CONSTRAINED 0x01
#define MSVAVFLAGS_MIC_PRESENT      0x02
#define MSVAVFLAGS_UNVERIFIED_SPN   0x04

#define NTLM_SIGNATURE_SIZE 16

struct ntlm_ctx;

/**
 * @brief           Create a ntlm_ctx initialized to the initial state
 *
 * @param ctx       The returned context
 *
 * @return 0 if successful, an error otherwise
 */
int ntlm_init_ctx(struct ntlm_ctx **ctx);

/**
 * @brief           Frees a ntlm_ctx
 *
 * @param ctx       Pointer to a context to be freed
 *
 * @return 0 if successful, an error otherwise
 * NOTE: even if an error is returned the contetx is freed and NULLed
 */
int ntlm_free_ctx(struct ntlm_ctx **ctx);

void ntlm_free_buffer_data(struct ntlm_buffer *buf);

uint64_t ntlm_timestamp_now(void);

bool ntlm_casecmp(const char *s1, const char *s2);

/**
 * @brief Sets the NTLMSSP version
 *        Mostly used to emulate Windows versions for test vectors
 *
 * @param major     major version number (ex. 6)
 * @param minor     minor version number (ex. 1)
 * @param build     build version number (ex. 7600)
 * @param revision  revision version number (ex. 16)
 */
void ntlm_internal_set_version(uint8_t major, uint8_t minor,
                               uint16_t build, uint8_t revision);

/* ############### CRYPTO FUNCTIONS ################ */

struct ntlm_key {
    uint8_t data[16]; /* up to 16 bytes (128 bits) */
    size_t length;
};

struct ntlm_signseal_handle {
    struct ntlm_key sign_key;
    struct ntlm_key seal_key;
    struct ntlm_rc4_handle *seal_handle;
    uint32_t seq_num;
};

struct ntlm_signseal_state {
    struct ntlm_signseal_handle send;
    struct ntlm_signseal_handle recv;
    bool datagram;
    bool ext_sec;
};

#define NTLM_SEND 1
#define NTLM_RECV 2

/**
 * @brief   Turns a utf8 password into an NT Hash
 *
 * @param password      The password
 * @param result        The returned hash
 *
 * @return 0 on success or an error;
 */
int NTOWFv1(const char *password, struct ntlm_key *result);

/**
 * @brief   Turns a utf8 password into an LM Hash
 *
 * @param password      The password
 * @param result        The returned hash
 *
 * @return 0 on success or an error;
 */
int LMOWFv1(const char *password, struct ntlm_key *result);

/**
 * @brief   Generate the challenge used in NTLMv1 w/ Extended Security
 *
 * @param server_chal   An 8 byte long buffer w/ the server challenge
 * @param client_chal   An 8 byte long buffer w/ the client challenge
 * @param result_chal   An 8 byte long buffer w/ for the result
 *
 * @return 0 on success or ERR_CRYPTO
 */
int ntlm_compute_ext_sec_challenge(uint8_t *server_chal,
                                   uint8_t *client_chal,
                                   uint8_t *result_chal);
/**
 * @brief   Generates a v1 NT Response
 *
 * @param nt_key            The NTLMv1 key computed by NTOWFv1()
 * @param ext_sec           Whether Extended Security has been negotiated
 * @param server_chal[8]    The server challenge
 * @param client_chal[8]    The client challenge (only with Extended Security)
 * @param nt_response       The output buffer (must be 24 bytes preallocated)
 *
 * @return 0 on success or ERR_CRYPTO
 */
int ntlm_compute_nt_response(struct ntlm_key *nt_key, bool ext_sec,
                             uint8_t server_chal[8], uint8_t client_chal[8],
                             struct ntlm_buffer *nt_response);

/**
 * @brief   Generates a v1 LM Response
 *
 * @param lm_key            The LMv1 key computed by LMOWFv1()
 * @param ext_sec           Whether Extended Security has been negotiated
 * @param server_chal[8]    The server challenge
 * @param client_chal[8]    The client challenge (only with Extended Security)
 * @param lm_response       The output buffer (must be 24 bytes preallocated)
 *
 * @return 0 on success or ERR_CRYPTO
 */
int ntlm_compute_lm_response(struct ntlm_key *lm_key, bool ext_sec,
                             uint8_t server_chal[8], uint8_t client_chal[8],
                             struct ntlm_buffer *lm_response);

/**
 * @brief   Returns the v1 session key
 *
 * @param nt_key            The NTLMv1 key computed by NTOWFv1()
 * @param session_base_key  The output buffer (must be 16 bytes preallocated)
 *
 * @return 0 on success or ERR_CRYPTO
 */
int ntlm_session_base_key(struct ntlm_key *nt_key,
                          struct ntlm_key *session_base_key);

/**
 * @brief   V1 Key Exchange Key calculation
 *
 * @param ctx               An ntlm context
 * @param ext_sec           Whether Extended Security has been negotiated
 * @param neg_lm_key        Whether LM KEY has been negotiated
 * @param non_nt_sess_key   Whether non NT Session Key has been negotiated
 * @param server_chal       The server challenge (only with Extended Security)
 * @param lm_key            The LMv1 key computed by LMOWFv1()
 * @param session_base_key  The Session Base Key
 * @param lm_response       The LM v1 Response
 * @param key_exchange_key  The output buffer (must be 16 bytes preallocated)
 *
 * @return 0 on success or ERR_CRYPTO
 */
int KXKEY(struct ntlm_ctx *ctx,
          bool ext_sec,
          bool neg_lm_key,
          bool non_nt_sess_key,
          uint8_t server_chal[8],
          struct ntlm_key *lm_key,
          struct ntlm_key *session_base_key,
          struct ntlm_buffer *lm_response,
          struct ntlm_key *key_exchange_key);

/**
 * @brief   Generates a NTLMv2 Response Key
 *
 * @param ctx           An ntlm context
 * @param nt_hash       The NT Hash of the user password
 * @param user          The user name
 * @param domain        The user's domain
 * @param result        The resulting key
 *                          (must be a preallocated 16 bytes buffer)
 *
 * @return 0 on success or ERR_CRYPTO
 */
int NTOWFv2(struct ntlm_ctx *ctx, struct ntlm_key *nt_hash,
            const char *user, const char *domain, struct ntlm_key *result);

/**
 * @brief   Computes The NTLMv2 Response
 *
 * @param ntlmv2_key         The NTLMv2 key computed by NTOWFv2()
 * @param server_chal[8]     The server provided challenge
 * @param client_chal[8]     A client generated random challenge
 * @param timestamp          A FILETIME timestamp
 * @param target_info        The target info
 * @param nt_response        The resulting nt_response buffer
 *
 * @return 0 on success or error.
 */
int ntlmv2_compute_nt_response(struct ntlm_key *ntlmv2_key,
                               uint8_t server_chal[8], uint8_t client_chal[8],
                               uint64_t timestamp,
                               struct ntlm_buffer *target_info,
                               struct ntlm_buffer *nt_response);

/**
 * @brief   Computes The LMv2 Response
 *
 * @param ntlmv2_key         The NTLMv2 key computed by NTOWFv2()
 * @param server_chal[8]     The server provided challenge
 * @param client_chal[8]     A client generated random challenge
 * @param lm_response        The resulting lm_response buffer
 *
 * @return 0 on success or error.
 */
int ntlmv2_compute_lm_response(struct ntlm_key *ntlmv2_key,
                               uint8_t server_chal[8], uint8_t client_chal[8],
                               struct ntlm_buffer *lm_response);

/**
 * @brief   Computes the NTLMv2 SessionBaseKey
 *
 * @param ntlmv2_key            The NTLMv2 key computed by NTOWFv2()
 * @param nt_response           The NTLMv2 response
 * @param session_base_key      The resulting session key
 *
 * @return 0 on success or error.
 */
int ntlmv2_session_base_key(struct ntlm_key *ntlmv2_key,
                            struct ntlm_buffer *nt_response,
                            struct ntlm_key *session_base_key);

/**
 * @brief   Comutes the NTLM session key
 *
 * @param key_exchange_key[16]          The Key Exchange Key
 * @param key_exch                      KEY_EXCH has been negotited
 * @param exported_session_key[16]      Resulting exported session key
 *
 * @return 0 on success or error.
 */
int ntlm_exported_session_key(struct ntlm_key *key_exchange_key,
                              bool key_exch,
                              struct ntlm_key *exported_session_key);

/**
 * @brief   Encrypts or Decrypts the NTLM session key using RC4K
 *
 * @param key_exchange_key[16]          The Key Exchange Key
 * @param exported_session_key[16]      Resulting exported session key
 * @param encrypted_random_session_key  Resulting encrypted session key
 *
 * @return 0 on success or error.
 */
int ntlm_encrypted_session_key(struct ntlm_key *key,
                               struct ntlm_key *in, struct ntlm_key *out);

/**
 * @brief   Computes the extended security keys from the session key
 *
 * @param flags                 Incoming challenge/authenticate flags
 * @param client                Wheter this ia a client or a server
 * @param session_key           The session key
 * @param signseal_state        Sign and seal keys and state
 *
 * @return 0 on success or error.
 */
int ntlm_signseal_keys(uint32_t flags, bool client,
                       struct ntlm_key *session_key,
                       struct ntlm_signseal_state *signseal_state);

/**
 * @brief   Resets the RC4 state for the send or receive handle
 *
 * @param flags                 Incoming challenge/authenticate flags
 * @param recv                  Wheter to reset the send or recive buffer
 * @param session_key           The session key
 * @param signseal_state        Sign and seal keys and state
 *
 * @return 0 on success or error.
 */
int ntlm_reset_rc4_state(uint32_t flags, bool recv,
                         struct ntlm_key *session_key,
                         struct ntlm_signseal_state *state);

/**
 * @brief   Verifies a NTLM v1 NT Response
 *
 * @param nt_response       The NT Response buffer
 * @param nt_key            The NTLMv1 NT Key
 * @param ext_sec           Whether Extended Security was negotiated
 * @param server_chal[8]    The Server Challenge
 * @param client_chal[8]    The Client Challenge
 *
 * @return 0 on success, or an error
 */
int ntlm_verify_nt_response(struct ntlm_buffer *nt_response,
                            struct ntlm_key *nt_key, bool ext_sec,
                            uint8_t server_chal[8], uint8_t client_chal[8]);

/**
 * @brief   Verifies a NTLM v1 LM Response
 *
 * @param lm_response       The LM Response buffer
 * @param lm_key            The NTLMv1 LM Key
 * @param ext_sec           Whether Extended Security was negotiated
 * @param server_chal[8]    The Server Challenge
 * @param client_chal[8]    The Client Challenge
 *
 * @return 0 on success, or an error
 */
int ntlm_verify_lm_response(struct ntlm_buffer *lm_response,
                            struct ntlm_key *lm_key, bool ext_sec,
                            uint8_t server_chal[8], uint8_t client_chal[8]);

/**
 * @brief   Verifies a NTLM v1 NT Response
 *
 * @param nt_response       The NT Response buffer
 * @param nt_key            The NTLMv1 NT Key
 * @param ext_sec           Whether Extended Security was negotiated
 * @param server_chal[8]    The Server Challenge
 * @param client_chal[8]    The Client Challenge
 *
 * @return 0 on success, or an error
 */
int ntlm_verify_nt_response(struct ntlm_buffer *nt_response,
                            struct ntlm_key *nt_key, bool ext_sec,
                            uint8_t server_chal[8], uint8_t client_chal[8]);

/**
 * @brief   Verifies a NTLM v1 LM Response
 *
 * @param lm_response       The LM Response buffer
 * @param lm_key            The NTLMv1 LM Key
 * @param ext_sec           Whether Extended Security was negotiated
 * @param server_chal[8]    The Server Challenge
 * @param client_chal[8]    The Client Challenge
 *
 * @return 0 on success, or an error
 */
int ntlm_verify_lm_response(struct ntlm_buffer *lm_response,
                            struct ntlm_key *lm_key, bool ext_sec,
                            uint8_t server_chal[8], uint8_t client_chal[8]);

/**
 * @brief   Verifies a 16 bit NT Response
 *
 * @param nt_response       The NT Response buffer including client challenge
 * @param ntlmv2_key        The NTLMv2 key
 * @param server_chal[8]    The server challenge used to compute the response
 *
 * @return 0 on success, or an error
 */
int ntlmv2_verify_nt_response(struct ntlm_buffer *nt_response,
                              struct ntlm_key *ntlmv2_key,
                              uint8_t server_chal[8]);

/**
 * @brief   Verifies a 16 bit LM Response
 *
 * @param nt_response       The LM Response buffer including client challenge
 * @param ntlmv2_key        The NTLMv2 key
 * @param server_chal[8]    The server challenge used to compute the response
 *
 * @return 0 on success, or an error
 */
int ntlmv2_verify_lm_response(struct ntlm_buffer *nt_response,
                              struct ntlm_key *ntlmv2_key,
                              uint8_t server_chal[8]);

/**
 * @brief Create NTLM signature for the provided message
 *
 * @param flags         Negotiated flags
 * @param state         Sign and seal keys and state
 * @param direction     Direction (true for send)
 * @param message       Message buffer
 * @param signature     Preallocated byffer of 16 bytes for signature
 *
 * @return 0 on success, or an error
 */
int ntlm_sign(uint32_t flags, int direction,
              struct ntlm_signseal_state *state,
              struct ntlm_buffer *message,
              struct ntlm_buffer *signature);

/**
 * @brief   NTLM seal the provided message
 *
 * @param flags         Negotiated flags
 * @param state         Sign and seal keys and state
 * @param message       Message buffer
 * @param output        Output buffer
 * @param signature     Signature
 *
 * @return 0 on success, or an error
 */
int ntlm_seal(uint32_t flags,
              struct ntlm_signseal_state *state,
              struct ntlm_buffer *message,
              struct ntlm_buffer *output,
              struct ntlm_buffer *signature);

/**
 * @brief   NTLM unseal the provided message
 *
 * @param flags         Negotiated flags
 * @param state         Sign and seal keys and state
 * @param message       Message buffer
 * @param output        Output buffer
 * @param signature     Signature
 *
 * @return 0 on success, or an error
 */
int ntlm_unseal(uint32_t flags,
                struct ntlm_signseal_state *state,
                struct ntlm_buffer *message,
                struct ntlm_buffer *output,
                struct ntlm_buffer *signature);

/**
 * @brief   Creates a NTLM MIC
 *
 * @param exported_session_key      The Exported Session Key
 * @param negotiate_message         The NTLM Negotiate Message (or empty)
 * @param challenge_message         The NTLM Challenge Message
 * @param authenticate_message      The NTLM Authenticate Message
 * @param mic                       Preallocated byffer of 16 bytes
 *
 * @return 0 on success, or an error
 */
int ntlm_mic(struct ntlm_key *exported_session_key,
             struct ntlm_buffer *negotiate_message,
             struct ntlm_buffer *challenge_message,
             struct ntlm_buffer *authenticate_message,
             struct ntlm_buffer *mic);

/**
 * @brief  Verifies a MIC
 *
 * @param key                       The keys used to generate the original MIC
 * @param negotiate_message         The NTLM Negotiate Message (or empty)
 * @param challenge_message         The NTLM Challenge Message
 * @param authenticate_message      The NTLM Authenticate Message
 * @param mic                       The original MIC
 *
 * NOTE: This function zeros the area of memory where the MIC is held in the
 *       Authenticate Message
 *
 * @return 0 on success, EACCES if the MIC fails to verify, or an error
 */
int ntlm_verify_mic(struct ntlm_key *key,
                    struct ntlm_buffer *negotiate_message,
                    struct ntlm_buffer *challenge_message,
                    struct ntlm_buffer *authenticate_message,
                    struct ntlm_buffer *mic);

/**
 * @brief   NTLM hash client channel binding unhashed data
 *
 * @param unhashed      The unhashed channel bindings data
 * @param signature     The MD5 signature
 *
 * @return 0 on success, or an error
 */
int ntlm_hash_channel_bindings(struct ntlm_buffer *unhashed,
                               struct ntlm_buffer *signature);

/**
 * @brief   Verifies Channel binding signature from unhashed data.
 *
 * @param unhashed      The unhashed channel bindings data
 * @param signature     The recieved MD5 signature to check against
 *
 * @return 0 on success, EACCES if the CBT fails to verify, or an error
 */
int ntlm_verify_channel_bindings(struct ntlm_buffer *unhashed,
                                 struct ntlm_buffer *signature);

/* ############## ENCODING / DECODING ############## */

/**
 * @brief   A utility function to construct a target_info structure
 *
 * @param ctx                   The ntlm context
 * @param nb_computer_name      The NetBIOS Computer Name
 * @param nb_domain_name        The NetBIOS Domain Name
 * @param dns_computer_name     The DNS Fully Qualified Computer Name
 * @param dns_domain_name       The DNS Fully Qualified Domain Name
 * @param dns_tree_name         The DNS Tree Name
 * @param av_flags              The av flags
 * @param av_timestamp          A 64 bit FILETIME timestamp
 * @param av_single_host        A ntlm_buffer with the single host data
 * @param av_target_name        The target name
 * @param av_cb                 A ntlm_buffer with channel binding data
 * @param target_info           The buffer in which target_info is returned.
 *
 * NOTE: The caller is responsible for free()ing the buffer
 *
 * @return      0 if everyting parses correctly, or an error code
 */
int ntlm_encode_target_info(struct ntlm_ctx *ctx, char *nb_computer_name,
                            char *nb_domain_name, char *dns_computer_name,
                            char *dns_domain_name, char *dns_tree_name,
                            uint32_t *av_flags, uint64_t *av_timestamp,
                            struct ntlm_buffer *av_single_host,
                            char *av_target_name, struct ntlm_buffer *av_cb,
                            struct ntlm_buffer *target_info);


/**
 * @brief   A utility function to parse a target_info structure
 *
 * @param ctx                   The ntlm context
 * @param buffer                A ntlm_buffer containing the info to be parsed
 * @param nb_computer_name      The NetBIOS Computer Name
 * @param nb_domain_name        The NetBIOS Domain Name
 * @param dns_computer_name     The DNS Fully Qualified Computer Name
 * @param dns_domain_name       The DNS Fully Qualified Domain Name
 * @param dns_tree_name         The DNS Tree Name
 * @param av_flags              The av flags
 * @param av_timestamp          A 64 bit FILETIME timestamp
 * @param av_single_host        A ntlm_buffer with the single host data
 * @param av_target_name        The target name
 * @param av_cb                 A ntlm_buffer with channel binding data
 *
 * NOTE: The caller is responsible for free()ing all strings, while the
 *       ntlm_buffer types point directly at data in the provided buffer.
 *
 * @return      0 if everyting parses correctly, or an error code
 */
int ntlm_decode_target_info(struct ntlm_ctx *ctx, struct ntlm_buffer *buffer,
                            char **nb_computer_name, char **nb_domain_name,
                            char **dns_computer_name, char **dns_domain_name,
                            char **dns_tree_name, char **av_target_name,
                            uint32_t *av_flags, uint64_t *av_timestamp,
                            struct ntlm_buffer *av_single_host,
                            struct ntlm_buffer *av_cb);

/**
 * @brief   A utility function to process a target_info structure
 *
 * @param ctx                   The ntlm context
 * @param protect               Set if signing or sealing has been requested
 * @param in                    A ntlm_buffer containing the received info
 * @param server                The Client Supplied Server Name if available
 * @param unhashed_cb           A ntlm_buffer with channel binding data
 * @param out                   The processed target_info buffer
 * @param out_srv_time          A 64 bit FILETIME timestamp
 * @param add_mic               A pointer to a boolean. If NULL MIC flags will
 *                               not be set, otherwise if allowed the MIC flag
 *                               will be set and true will be returned.
 *
 * @return      0 if everyting parses correctly, or an error code
 */
int ntlm_process_target_info(struct ntlm_ctx *ctx, bool protect,
                             struct ntlm_buffer *in,
                             const char *server,
                             struct ntlm_buffer *unhashed_cb,
                             struct ntlm_buffer *out,
                             uint64_t *out_srv_time,
                             bool *add_mic);

/**
 * @brief Verifies the message signature is valid and the message
 * in sequence with the expected state
 *
 * @param ctx           The conversation context.
 * @param buffer        A ntlm_buffer containing the raw NTLMSSP packet
 *
 * @return      0 if everyting parses correctly, or an error code
 *
 * NOTE: Always use ntlm_detect_msg_type before calling other functions,
 * so that the signature and message type are checked, and the state is
 * validated.
 */
int ntlm_decode_msg_type(struct ntlm_ctx *ctx,
                         struct ntlm_buffer *buffer,
                         uint32_t *type);

/**
 * @brief This function encodes a NEGTIATE_MESSAGE which is the first message
 * a client will send to a server. It also updates the stage in the context.
 *
 * @param ctx           A fresh ntlm context.
 * @param flags         Requested flags
 * @param domain        Optional Domain Name
 * @param workstation   Optional Workstation Name
 * @param message       A ntlm_buffer containing the returned message
 *
 * NOTE: the caller is responsible for free()ing the message buffer.
 *
 * @return      0 if everyting encodes correctly, or an error code
 */
int ntlm_encode_neg_msg(struct ntlm_ctx *ctx, uint32_t flags,
                        const char *domain, const char *workstation,
                        struct ntlm_buffer *message);

/**
 * @brief This function decodes a NTLMSSP NEGTIATE_MESSAGE.
 *
 * @param ctx           A fresh ntlm context
 * @param buffer        A ntlm_buffer containing the raw NTLMSSP packet
 * @param flags         Returns the flags requested by the client
 * @param domain        Returns the domain provided by the client if any
 * @param workstation   Returns the workstation provided by the client if any
 *
 * @return      0 if everyting parses correctly, or an error code
 *
 */
int ntlm_decode_neg_msg(struct ntlm_ctx *ctx,
                        struct ntlm_buffer *buffer, uint32_t *flags,
                        char **domain, char **workstation);

/**
 * @brief This function encodes a CHALLENGE_MESSAGE which is the first message
 * a server will send to a client. It also updates the stage in the context.
 *
 * @param ctx           The ntlm context
 * @param flags         The challenge flags
 * @param target_name   The target name
 * @param challenge     A 64 bit value with a challenge
 * @param target_info   A buffer containing target_info data
 * @param message       A ntlm_buffer containing the encoded message
 *
 * NOTE: the caller is responsible for free()ing the message buffer
 *
 * @return      0 if everyting encodes correctly, or an error code
 */
int ntlm_encode_chal_msg(struct ntlm_ctx *ctx,
                         uint32_t flags,
                         const char *target_name,
                         struct ntlm_buffer *challenge,
                         struct ntlm_buffer *target_info,
                         struct ntlm_buffer *message);


/**
 * @brief This function decodes a NTLMSSP CHALLENGE_MESSAGE.
 *
 * @param ctx           The ntlm context
 * @param buffer        A ntlm_buffer containing the raw NTLMSSP packet
 * @param flags         The challenge flags
 * @param target_name   The target name
 * @param challenge     A 64 bit value with the server challenge
 *                      The caller MUST provide a preallocated buffer of
 *                      appropriate length (8 bytes)
 * @param target_info   A buffer containing returned target_info data
 *
 * @return      0 if everyting encodes correctly, or an error code
 */
int ntlm_decode_chal_msg(struct ntlm_ctx *ctx,
                         struct ntlm_buffer *buffer,
                         uint32_t *flags, char **target_name,
                         struct ntlm_buffer *challenge,
                         struct ntlm_buffer *target_info);


/**
 * @brief This function encodes a AUTHENTICATE_MESSAGE which is the second
 * message a client will send to a serve.
 * It also updates the stage in the context.
 *
 * @param ctx           The ntlm context
 * @param flags         The flags
 * @param lm_chalresp   A LM or LMv2 response
 * @param nt_chalresp   A NTLM or NTLMv2 response
 * @param domain_name   The Domain name
 * @param user_name     The User name
 * @param workstation   The Workstation name
 * @param enc_sess_key  The session key
 * @param mic           A MIC of the messages
 * @param message       A ntlm_buffer containing the encoded message
 *
 * @return      0 if everyting encodes correctly, or an error code
 */
int ntlm_encode_auth_msg(struct ntlm_ctx *ctx,
                         uint32_t flags,
                         struct ntlm_buffer *lm_chalresp,
                         struct ntlm_buffer *nt_chalresp,
                         char *domain_name, char *user_name,
                         char *workstation,
                         struct ntlm_buffer *enc_sess_key,
                         struct ntlm_buffer *mic,
                         struct ntlm_buffer *message);

/**
 * @brief This function decodes a NTLMSSP AUTHENTICATE_MESSAGE.
 *
 * @param ctx           The ntlm context
 * @param buffer        A ntlm_buffer containing the raw NTLMSSP packet
 * @param flags         The negotiated flags
 * @param lm_chalresp   A LM or LMv2 response
 * @param nt_chalresp   A NTLM or NTLMv2 response
 * @param domain_name   The Domain name
 * @param user_name     The User name
 * @param workstation   The Workstation name
 * @param enc_sess_key  The session key
 * @param target_info   The target_info AV_PAIR embedded in the NT Response
 * @param mic           A MIC of the messages
 *                      Passing a pointer to a mic means the caller has
 *                      previously requested the presence of a MIC field from
 *                      the peer. If a MIC is not returned by the peer the
 *                      secoding will fail. If not MIC ha sbeen previously
 *                      requested set this pointer to NULL.
 *                      The caller must provide a preallocated buffer of
 *                      appropriate length (16 bytes)
 *
 * NOTE: the caller is reponsible for freeing all allocated buffers
 * on success.
 *
 * @return      0 if everyting encodes correctly, or an error code
 */
int ntlm_decode_auth_msg(struct ntlm_ctx *ctx,
                         struct ntlm_buffer *buffer,
                         uint32_t flags,
                         struct ntlm_buffer *lm_chalresp,
                         struct ntlm_buffer *nt_chalresp,
                         char **domain_name, char **user_name,
                         char **workstation,
                         struct ntlm_buffer *enc_sess_key,
                         struct ntlm_buffer *target_info,
                         struct ntlm_buffer *mic);

#endif /* _NTLM_H_ */
