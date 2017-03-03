/***
* Copyright 2017 Marc Stevens <marc@marc-stevens.nl>, Dan Shumow (danshu@microsoft.com) 
* Distributed under the MIT Software License.
* See accompanying file LICENSE.txt or copy at
* https://opensource.org/licenses/MIT
***/

#include <string.h>
#include <memory.h>
#include <stdio.h>

#include "sha1.h"
#include "ubc_check.h"

#define rotate_right(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define rotate_left(x,n)  (((x)<<(n))|((x)>>(32-(n))))

#define sha1_f1(b,c,d) ((d)^((b)&((c)^(d))))
#define sha1_f2(b,c,d) ((b)^(c)^(d))
#define sha1_f3(b,c,d) (((b) & ((c)|(d))) | ((c)&(d)))
#define sha1_f4(b,c,d) ((b)^(c)^(d))

#define HASHCLASH_SHA1COMPRESS_ROUND1_STEP(a, b, c, d, e, m, t) \
	{ e += rotate_left(a, 5) + sha1_f1(b,c,d) + 0x5A827999 + m[t]; b = rotate_left(b, 30); }
#define HASHCLASH_SHA1COMPRESS_ROUND2_STEP(a, b, c, d, e, m, t) \
	{ e += rotate_left(a, 5) + sha1_f2(b,c,d) + 0x6ED9EBA1 + m[t]; b = rotate_left(b, 30); }
#define HASHCLASH_SHA1COMPRESS_ROUND3_STEP(a, b, c, d, e, m, t) \
	{ e += rotate_left(a, 5) + sha1_f3(b,c,d) + 0x8F1BBCDC + m[t]; b = rotate_left(b, 30); }
#define HASHCLASH_SHA1COMPRESS_ROUND4_STEP(a, b, c, d, e, m, t) \
	{ e += rotate_left(a, 5) + sha1_f4(b,c,d) + 0xCA62C1D6 + m[t]; b = rotate_left(b, 30); }

#define HASHCLASH_SHA1COMPRESS_ROUND1_STEP_BW(a, b, c, d, e, m, t) \
	{ b = rotate_right(b, 30); e -= rotate_left(a, 5) + sha1_f1(b,c,d) + 0x5A827999 + m[t]; }
#define HASHCLASH_SHA1COMPRESS_ROUND2_STEP_BW(a, b, c, d, e, m, t) \
	{ b = rotate_right(b, 30); e -= rotate_left(a, 5) + sha1_f2(b,c,d) + 0x6ED9EBA1 + m[t]; }
#define HASHCLASH_SHA1COMPRESS_ROUND3_STEP_BW(a, b, c, d, e, m, t) \
	{ b = rotate_right(b, 30); e -= rotate_left(a, 5) + sha1_f3(b,c,d) + 0x8F1BBCDC + m[t]; }
#define HASHCLASH_SHA1COMPRESS_ROUND4_STEP_BW(a, b, c, d, e, m, t) \
	{ b = rotate_right(b, 30); e -= rotate_left(a, 5) + sha1_f4(b,c,d) + 0xCA62C1D6 + m[t]; }

#define SHA1_STORE_STATE(i) states[i][0] = a; states[i][1] = b; states[i][2] = c; states[i][3] = d; states[i][4] = e;



void sha1_message_expansion(uint32_t W[80])
{
	unsigned i;

	for (i = 16; i < 80; ++i)
		W[i] = rotate_left(W[i - 3] ^ W[i - 8] ^ W[i - 14] ^ W[i - 16], 1);
}

void sha1_compression(uint32_t ihv[5], const uint32_t m[16])
{
	uint32_t a, b, c, d, e, W[80];
	unsigned i;

	memcpy(W, m, 16 * 4);
	for (i = 16; i < 80; ++i)
		W[i] = rotate_left(W[i - 3] ^ W[i - 8] ^ W[i - 14] ^ W[i - 16], 1);

	a = ihv[0], b = ihv[1], c = ihv[2], d = ihv[3], e = ihv[4];

	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(a, b, c, d, e, W, 0);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(e, a, b, c, d, W, 1);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(d, e, a, b, c, W, 2);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(c, d, e, a, b, W, 3);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(b, c, d, e, a, W, 4);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(a, b, c, d, e, W, 5);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(e, a, b, c, d, W, 6);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(d, e, a, b, c, W, 7);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(c, d, e, a, b, W, 8);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(b, c, d, e, a, W, 9);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(a, b, c, d, e, W, 10);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(e, a, b, c, d, W, 11);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(d, e, a, b, c, W, 12);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(c, d, e, a, b, W, 13);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(b, c, d, e, a, W, 14);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(a, b, c, d, e, W, 15);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(e, a, b, c, d, W, 16);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(d, e, a, b, c, W, 17);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(c, d, e, a, b, W, 18);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(b, c, d, e, a, W, 19);

	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(a, b, c, d, e, W, 20);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(e, a, b, c, d, W, 21);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(d, e, a, b, c, W, 22);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(c, d, e, a, b, W, 23);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(b, c, d, e, a, W, 24);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(a, b, c, d, e, W, 25);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(e, a, b, c, d, W, 26);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(d, e, a, b, c, W, 27);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(c, d, e, a, b, W, 28);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(b, c, d, e, a, W, 29);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(a, b, c, d, e, W, 30);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(e, a, b, c, d, W, 31);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(d, e, a, b, c, W, 32);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(c, d, e, a, b, W, 33);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(b, c, d, e, a, W, 34);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(a, b, c, d, e, W, 35);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(e, a, b, c, d, W, 36);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(d, e, a, b, c, W, 37);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(c, d, e, a, b, W, 38);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(b, c, d, e, a, W, 39);

	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(a, b, c, d, e, W, 40);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(e, a, b, c, d, W, 41);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(d, e, a, b, c, W, 42);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(c, d, e, a, b, W, 43);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(b, c, d, e, a, W, 44);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(a, b, c, d, e, W, 45);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(e, a, b, c, d, W, 46);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(d, e, a, b, c, W, 47);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(c, d, e, a, b, W, 48);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(b, c, d, e, a, W, 49);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(a, b, c, d, e, W, 50);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(e, a, b, c, d, W, 51);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(d, e, a, b, c, W, 52);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(c, d, e, a, b, W, 53);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(b, c, d, e, a, W, 54);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(a, b, c, d, e, W, 55);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(e, a, b, c, d, W, 56);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(d, e, a, b, c, W, 57);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(c, d, e, a, b, W, 58);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(b, c, d, e, a, W, 59);

	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(a, b, c, d, e, W, 60);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(e, a, b, c, d, W, 61);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(d, e, a, b, c, W, 62);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(c, d, e, a, b, W, 63);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(b, c, d, e, a, W, 64);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(a, b, c, d, e, W, 65);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(e, a, b, c, d, W, 66);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(d, e, a, b, c, W, 67);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(c, d, e, a, b, W, 68);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(b, c, d, e, a, W, 69);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(a, b, c, d, e, W, 70);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(e, a, b, c, d, W, 71);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(d, e, a, b, c, W, 72);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(c, d, e, a, b, W, 73);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(b, c, d, e, a, W, 74);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(a, b, c, d, e, W, 75);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(e, a, b, c, d, W, 76);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(d, e, a, b, c, W, 77);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(c, d, e, a, b, W, 78);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(b, c, d, e, a, W, 79);

	ihv[0] += a; ihv[1] += b; ihv[2] += c; ihv[3] += d; ihv[4] += e;
}



void sha1_compression_W(uint32_t ihv[5], const uint32_t W[80])
{
	uint32_t a = ihv[0], b = ihv[1], c = ihv[2], d = ihv[3], e = ihv[4];

	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(a, b, c, d, e, W, 0);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(e, a, b, c, d, W, 1);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(d, e, a, b, c, W, 2);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(c, d, e, a, b, W, 3);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(b, c, d, e, a, W, 4);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(a, b, c, d, e, W, 5);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(e, a, b, c, d, W, 6);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(d, e, a, b, c, W, 7);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(c, d, e, a, b, W, 8);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(b, c, d, e, a, W, 9);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(a, b, c, d, e, W, 10);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(e, a, b, c, d, W, 11);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(d, e, a, b, c, W, 12);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(c, d, e, a, b, W, 13);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(b, c, d, e, a, W, 14);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(a, b, c, d, e, W, 15);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(e, a, b, c, d, W, 16);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(d, e, a, b, c, W, 17);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(c, d, e, a, b, W, 18);
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(b, c, d, e, a, W, 19);

	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(a, b, c, d, e, W, 20);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(e, a, b, c, d, W, 21);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(d, e, a, b, c, W, 22);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(c, d, e, a, b, W, 23);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(b, c, d, e, a, W, 24);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(a, b, c, d, e, W, 25);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(e, a, b, c, d, W, 26);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(d, e, a, b, c, W, 27);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(c, d, e, a, b, W, 28);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(b, c, d, e, a, W, 29);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(a, b, c, d, e, W, 30);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(e, a, b, c, d, W, 31);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(d, e, a, b, c, W, 32);
 	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(c, d, e, a, b, W, 33);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(b, c, d, e, a, W, 34);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(a, b, c, d, e, W, 35);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(e, a, b, c, d, W, 36);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(d, e, a, b, c, W, 37);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(c, d, e, a, b, W, 38);
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(b, c, d, e, a, W, 39);

	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(a, b, c, d, e, W, 40);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(e, a, b, c, d, W, 41);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(d, e, a, b, c, W, 42);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(c, d, e, a, b, W, 43);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(b, c, d, e, a, W, 44);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(a, b, c, d, e, W, 45);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(e, a, b, c, d, W, 46);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(d, e, a, b, c, W, 47);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(c, d, e, a, b, W, 48);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(b, c, d, e, a, W, 49);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(a, b, c, d, e, W, 50);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(e, a, b, c, d, W, 51);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(d, e, a, b, c, W, 52);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(c, d, e, a, b, W, 53);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(b, c, d, e, a, W, 54);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(a, b, c, d, e, W, 55);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(e, a, b, c, d, W, 56);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(d, e, a, b, c, W, 57);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(c, d, e, a, b, W, 58);
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(b, c, d, e, a, W, 59);

	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(a, b, c, d, e, W, 60);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(e, a, b, c, d, W, 61);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(d, e, a, b, c, W, 62);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(c, d, e, a, b, W, 63);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(b, c, d, e, a, W, 64);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(a, b, c, d, e, W, 65);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(e, a, b, c, d, W, 66);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(d, e, a, b, c, W, 67);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(c, d, e, a, b, W, 68);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(b, c, d, e, a, W, 69);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(a, b, c, d, e, W, 70);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(e, a, b, c, d, W, 71);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(d, e, a, b, c, W, 72);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(c, d, e, a, b, W, 73);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(b, c, d, e, a, W, 74);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(a, b, c, d, e, W, 75);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(e, a, b, c, d, W, 76);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(d, e, a, b, c, W, 77);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(c, d, e, a, b, W, 78);
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(b, c, d, e, a, W, 79);

	ihv[0] += a; ihv[1] += b; ihv[2] += c; ihv[3] += d; ihv[4] += e;
}



void sha1_compression_states(uint32_t ihv[5], const uint32_t W[80], uint32_t states[80][5])
{
	uint32_t a = ihv[0], b = ihv[1], c = ihv[2], d = ihv[3], e = ihv[4];

#ifdef DOSTORESTATE00
	SHA1_STORE_STATE(0)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(a, b, c, d, e, W, 0);

#ifdef DOSTORESTATE01
	SHA1_STORE_STATE(1)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(e, a, b, c, d, W, 1);

#ifdef DOSTORESTATE02
	SHA1_STORE_STATE(2)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(d, e, a, b, c, W, 2);

#ifdef DOSTORESTATE03
	SHA1_STORE_STATE(3)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(c, d, e, a, b, W, 3);

#ifdef DOSTORESTATE04
	SHA1_STORE_STATE(4)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(b, c, d, e, a, W, 4);

#ifdef DOSTORESTATE05
	SHA1_STORE_STATE(5)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(a, b, c, d, e, W, 5);

#ifdef DOSTORESTATE06
	SHA1_STORE_STATE(6)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(e, a, b, c, d, W, 6);

#ifdef DOSTORESTATE07
	SHA1_STORE_STATE(7)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(d, e, a, b, c, W, 7);

#ifdef DOSTORESTATE08
	SHA1_STORE_STATE(8)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(c, d, e, a, b, W, 8);

#ifdef DOSTORESTATE09
	SHA1_STORE_STATE(9)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(b, c, d, e, a, W, 9);

#ifdef DOSTORESTATE10
	SHA1_STORE_STATE(10)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(a, b, c, d, e, W, 10);

#ifdef DOSTORESTATE11
	SHA1_STORE_STATE(11)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(e, a, b, c, d, W, 11);

#ifdef DOSTORESTATE12
	SHA1_STORE_STATE(12)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(d, e, a, b, c, W, 12);

#ifdef DOSTORESTATE13
	SHA1_STORE_STATE(13)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(c, d, e, a, b, W, 13);

#ifdef DOSTORESTATE14
	SHA1_STORE_STATE(14)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(b, c, d, e, a, W, 14);

#ifdef DOSTORESTATE15
	SHA1_STORE_STATE(15)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(a, b, c, d, e, W, 15);

#ifdef DOSTORESTATE16
	SHA1_STORE_STATE(16)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(e, a, b, c, d, W, 16);

#ifdef DOSTORESTATE17
	SHA1_STORE_STATE(17)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(d, e, a, b, c, W, 17);

#ifdef DOSTORESTATE18
	SHA1_STORE_STATE(18)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(c, d, e, a, b, W, 18);

#ifdef DOSTORESTATE19
	SHA1_STORE_STATE(19)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND1_STEP(b, c, d, e, a, W, 19);



#ifdef DOSTORESTATE20
	SHA1_STORE_STATE(20)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(a, b, c, d, e, W, 20);

#ifdef DOSTORESTATE21
	SHA1_STORE_STATE(21)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(e, a, b, c, d, W, 21);
	
#ifdef DOSTORESTATE22
	SHA1_STORE_STATE(22)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(d, e, a, b, c, W, 22);
	
#ifdef DOSTORESTATE23
	SHA1_STORE_STATE(23)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(c, d, e, a, b, W, 23);

#ifdef DOSTORESTATE24
	SHA1_STORE_STATE(24)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(b, c, d, e, a, W, 24);

#ifdef DOSTORESTATE25
	SHA1_STORE_STATE(25)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(a, b, c, d, e, W, 25);

#ifdef DOSTORESTATE26
	SHA1_STORE_STATE(26)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(e, a, b, c, d, W, 26);

#ifdef DOSTORESTATE27
	SHA1_STORE_STATE(27)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(d, e, a, b, c, W, 27);
	
#ifdef DOSTORESTATE28
	SHA1_STORE_STATE(28)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(c, d, e, a, b, W, 28);
	
#ifdef DOSTORESTATE29
	SHA1_STORE_STATE(29)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(b, c, d, e, a, W, 29);
	
#ifdef DOSTORESTATE30
	SHA1_STORE_STATE(30)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(a, b, c, d, e, W, 30);
	
#ifdef DOSTORESTATE31
	SHA1_STORE_STATE(31)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(e, a, b, c, d, W, 31);
	
#ifdef DOSTORESTATE32
	SHA1_STORE_STATE(32)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(d, e, a, b, c, W, 32);

#ifdef DOSTORESTATE33
	SHA1_STORE_STATE(33)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(c, d, e, a, b, W, 33);

#ifdef DOSTORESTATE34
	SHA1_STORE_STATE(34)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(b, c, d, e, a, W, 34);

#ifdef DOSTORESTATE35
	SHA1_STORE_STATE(35)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(a, b, c, d, e, W, 35);
	
#ifdef DOSTORESTATE36
	SHA1_STORE_STATE(36)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(e, a, b, c, d, W, 36);
	
#ifdef DOSTORESTATE37
	SHA1_STORE_STATE(37)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(d, e, a, b, c, W, 37);
	
#ifdef DOSTORESTATE38
	SHA1_STORE_STATE(38)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(c, d, e, a, b, W, 38);
	
#ifdef DOSTORESTATE39
	SHA1_STORE_STATE(39)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND2_STEP(b, c, d, e, a, W, 39);



#ifdef DOSTORESTATE40
	SHA1_STORE_STATE(40)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(a, b, c, d, e, W, 40);

#ifdef DOSTORESTATE41
	SHA1_STORE_STATE(41)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(e, a, b, c, d, W, 41);

#ifdef DOSTORESTATE42
	SHA1_STORE_STATE(42)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(d, e, a, b, c, W, 42);

#ifdef DOSTORESTATE43
	SHA1_STORE_STATE(43)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(c, d, e, a, b, W, 43);

#ifdef DOSTORESTATE44
	SHA1_STORE_STATE(44)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(b, c, d, e, a, W, 44);

#ifdef DOSTORESTATE45
	SHA1_STORE_STATE(45)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(a, b, c, d, e, W, 45);

#ifdef DOSTORESTATE46
	SHA1_STORE_STATE(46)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(e, a, b, c, d, W, 46);

#ifdef DOSTORESTATE47
	SHA1_STORE_STATE(47)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(d, e, a, b, c, W, 47);

#ifdef DOSTORESTATE48
	SHA1_STORE_STATE(48)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(c, d, e, a, b, W, 48);

#ifdef DOSTORESTATE49
	SHA1_STORE_STATE(49)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(b, c, d, e, a, W, 49);

#ifdef DOSTORESTATE50
	SHA1_STORE_STATE(50)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(a, b, c, d, e, W, 50);

#ifdef DOSTORESTATE51
	SHA1_STORE_STATE(51)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(e, a, b, c, d, W, 51);

#ifdef DOSTORESTATE52
	SHA1_STORE_STATE(52)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(d, e, a, b, c, W, 52);

#ifdef DOSTORESTATE53
	SHA1_STORE_STATE(53)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(c, d, e, a, b, W, 53);

#ifdef DOSTORESTATE54
	SHA1_STORE_STATE(54)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(b, c, d, e, a, W, 54);

#ifdef DOSTORESTATE55
	SHA1_STORE_STATE(55)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(a, b, c, d, e, W, 55);

#ifdef DOSTORESTATE56
	SHA1_STORE_STATE(56)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(e, a, b, c, d, W, 56);

#ifdef DOSTORESTATE57
	SHA1_STORE_STATE(57)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(d, e, a, b, c, W, 57);

#ifdef DOSTORESTATE58
	SHA1_STORE_STATE(58)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(c, d, e, a, b, W, 58);

#ifdef DOSTORESTATE59
	SHA1_STORE_STATE(59)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND3_STEP(b, c, d, e, a, W, 59);
	



#ifdef DOSTORESTATE60
	SHA1_STORE_STATE(60)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(a, b, c, d, e, W, 60);

#ifdef DOSTORESTATE61
	SHA1_STORE_STATE(61)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(e, a, b, c, d, W, 61);

#ifdef DOSTORESTATE62
	SHA1_STORE_STATE(62)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(d, e, a, b, c, W, 62);

#ifdef DOSTORESTATE63
	SHA1_STORE_STATE(63)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(c, d, e, a, b, W, 63);

#ifdef DOSTORESTATE64
	SHA1_STORE_STATE(64)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(b, c, d, e, a, W, 64);

#ifdef DOSTORESTATE65
	SHA1_STORE_STATE(65)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(a, b, c, d, e, W, 65);

#ifdef DOSTORESTATE66
	SHA1_STORE_STATE(66)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(e, a, b, c, d, W, 66);

#ifdef DOSTORESTATE67
	SHA1_STORE_STATE(67)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(d, e, a, b, c, W, 67);

#ifdef DOSTORESTATE68
	SHA1_STORE_STATE(68)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(c, d, e, a, b, W, 68);

#ifdef DOSTORESTATE69
	SHA1_STORE_STATE(69)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(b, c, d, e, a, W, 69);

#ifdef DOSTORESTATE70
	SHA1_STORE_STATE(70)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(a, b, c, d, e, W, 70);

#ifdef DOSTORESTATE71
	SHA1_STORE_STATE(71)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(e, a, b, c, d, W, 71);

#ifdef DOSTORESTATE72
	SHA1_STORE_STATE(72)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(d, e, a, b, c, W, 72);

#ifdef DOSTORESTATE73
	SHA1_STORE_STATE(73)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(c, d, e, a, b, W, 73);

#ifdef DOSTORESTATE74
	SHA1_STORE_STATE(74)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(b, c, d, e, a, W, 74);

#ifdef DOSTORESTATE75
	SHA1_STORE_STATE(75)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(a, b, c, d, e, W, 75);

#ifdef DOSTORESTATE76
	SHA1_STORE_STATE(76)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(e, a, b, c, d, W, 76);

#ifdef DOSTORESTATE77
	SHA1_STORE_STATE(77)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(d, e, a, b, c, W, 77);

#ifdef DOSTORESTATE78
	SHA1_STORE_STATE(78)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(c, d, e, a, b, W, 78);

#ifdef DOSTORESTATE79
	SHA1_STORE_STATE(79)
#endif
	HASHCLASH_SHA1COMPRESS_ROUND4_STEP(b, c, d, e, a, W, 79);



	ihv[0] += a; ihv[1] += b; ihv[2] += c; ihv[3] += d; ihv[4] += e;
}




#define SHA1_RECOMPRESS(t) \
void sha1recompress_fast_ ## t (uint32_t ihvin[5], uint32_t ihvout[5], const uint32_t me2[80], const uint32_t state[5]) \
{ \
	uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4]; \
	if (t > 79) HASHCLASH_SHA1COMPRESS_ROUND4_STEP_BW(b, c, d, e, a, me2, 79); \
	if (t > 78) HASHCLASH_SHA1COMPRESS_ROUND4_STEP_BW(c, d, e, a, b, me2, 78); \
	if (t > 77) HASHCLASH_SHA1COMPRESS_ROUND4_STEP_BW(d, e, a, b, c, me2, 77); \
	if (t > 76) HASHCLASH_SHA1COMPRESS_ROUND4_STEP_BW(e, a, b, c, d, me2, 76); \
	if (t > 75) HASHCLASH_SHA1COMPRESS_ROUND4_STEP_BW(a, b, c, d, e, me2, 75); \
	if (t > 74) HASHCLASH_SHA1COMPRESS_ROUND4_STEP_BW(b, c, d, e, a, me2, 74); \
	if (t > 73) HASHCLASH_SHA1COMPRESS_ROUND4_STEP_BW(c, d, e, a, b, me2, 73); \
	if (t > 72) HASHCLASH_SHA1COMPRESS_ROUND4_STEP_BW(d, e, a, b, c, me2, 72); \
	if (t > 71) HASHCLASH_SHA1COMPRESS_ROUND4_STEP_BW(e, a, b, c, d, me2, 71); \
	if (t > 70) HASHCLASH_SHA1COMPRESS_ROUND4_STEP_BW(a, b, c, d, e, me2, 70); \
	if (t > 69) HASHCLASH_SHA1COMPRESS_ROUND4_STEP_BW(b, c, d, e, a, me2, 69); \
	if (t > 68) HASHCLASH_SHA1COMPRESS_ROUND4_STEP_BW(c, d, e, a, b, me2, 68); \
	if (t > 67) HASHCLASH_SHA1COMPRESS_ROUND4_STEP_BW(d, e, a, b, c, me2, 67); \
	if (t > 66) HASHCLASH_SHA1COMPRESS_ROUND4_STEP_BW(e, a, b, c, d, me2, 66); \
	if (t > 65) HASHCLASH_SHA1COMPRESS_ROUND4_STEP_BW(a, b, c, d, e, me2, 65); \
	if (t > 64) HASHCLASH_SHA1COMPRESS_ROUND4_STEP_BW(b, c, d, e, a, me2, 64); \
	if (t > 63) HASHCLASH_SHA1COMPRESS_ROUND4_STEP_BW(c, d, e, a, b, me2, 63); \
	if (t > 62) HASHCLASH_SHA1COMPRESS_ROUND4_STEP_BW(d, e, a, b, c, me2, 62); \
	if (t > 61) HASHCLASH_SHA1COMPRESS_ROUND4_STEP_BW(e, a, b, c, d, me2, 61); \
	if (t > 60) HASHCLASH_SHA1COMPRESS_ROUND4_STEP_BW(a, b, c, d, e, me2, 60); \
	if (t > 59) HASHCLASH_SHA1COMPRESS_ROUND3_STEP_BW(b, c, d, e, a, me2, 59); \
	if (t > 58) HASHCLASH_SHA1COMPRESS_ROUND3_STEP_BW(c, d, e, a, b, me2, 58); \
	if (t > 57) HASHCLASH_SHA1COMPRESS_ROUND3_STEP_BW(d, e, a, b, c, me2, 57); \
	if (t > 56) HASHCLASH_SHA1COMPRESS_ROUND3_STEP_BW(e, a, b, c, d, me2, 56); \
	if (t > 55) HASHCLASH_SHA1COMPRESS_ROUND3_STEP_BW(a, b, c, d, e, me2, 55); \
	if (t > 54) HASHCLASH_SHA1COMPRESS_ROUND3_STEP_BW(b, c, d, e, a, me2, 54); \
	if (t > 53) HASHCLASH_SHA1COMPRESS_ROUND3_STEP_BW(c, d, e, a, b, me2, 53); \
	if (t > 52) HASHCLASH_SHA1COMPRESS_ROUND3_STEP_BW(d, e, a, b, c, me2, 52); \
	if (t > 51) HASHCLASH_SHA1COMPRESS_ROUND3_STEP_BW(e, a, b, c, d, me2, 51); \
	if (t > 50) HASHCLASH_SHA1COMPRESS_ROUND3_STEP_BW(a, b, c, d, e, me2, 50); \
	if (t > 49) HASHCLASH_SHA1COMPRESS_ROUND3_STEP_BW(b, c, d, e, a, me2, 49); \
	if (t > 48) HASHCLASH_SHA1COMPRESS_ROUND3_STEP_BW(c, d, e, a, b, me2, 48); \
	if (t > 47) HASHCLASH_SHA1COMPRESS_ROUND3_STEP_BW(d, e, a, b, c, me2, 47); \
	if (t > 46) HASHCLASH_SHA1COMPRESS_ROUND3_STEP_BW(e, a, b, c, d, me2, 46); \
	if (t > 45) HASHCLASH_SHA1COMPRESS_ROUND3_STEP_BW(a, b, c, d, e, me2, 45); \
	if (t > 44) HASHCLASH_SHA1COMPRESS_ROUND3_STEP_BW(b, c, d, e, a, me2, 44); \
	if (t > 43) HASHCLASH_SHA1COMPRESS_ROUND3_STEP_BW(c, d, e, a, b, me2, 43); \
	if (t > 42) HASHCLASH_SHA1COMPRESS_ROUND3_STEP_BW(d, e, a, b, c, me2, 42); \
	if (t > 41) HASHCLASH_SHA1COMPRESS_ROUND3_STEP_BW(e, a, b, c, d, me2, 41); \
	if (t > 40) HASHCLASH_SHA1COMPRESS_ROUND3_STEP_BW(a, b, c, d, e, me2, 40); \
	if (t > 39) HASHCLASH_SHA1COMPRESS_ROUND2_STEP_BW(b, c, d, e, a, me2, 39); \
	if (t > 38) HASHCLASH_SHA1COMPRESS_ROUND2_STEP_BW(c, d, e, a, b, me2, 38); \
	if (t > 37) HASHCLASH_SHA1COMPRESS_ROUND2_STEP_BW(d, e, a, b, c, me2, 37); \
	if (t > 36) HASHCLASH_SHA1COMPRESS_ROUND2_STEP_BW(e, a, b, c, d, me2, 36); \
	if (t > 35) HASHCLASH_SHA1COMPRESS_ROUND2_STEP_BW(a, b, c, d, e, me2, 35); \
	if (t > 34) HASHCLASH_SHA1COMPRESS_ROUND2_STEP_BW(b, c, d, e, a, me2, 34); \
	if (t > 33) HASHCLASH_SHA1COMPRESS_ROUND2_STEP_BW(c, d, e, a, b, me2, 33); \
	if (t > 32) HASHCLASH_SHA1COMPRESS_ROUND2_STEP_BW(d, e, a, b, c, me2, 32); \
	if (t > 31) HASHCLASH_SHA1COMPRESS_ROUND2_STEP_BW(e, a, b, c, d, me2, 31); \
	if (t > 30) HASHCLASH_SHA1COMPRESS_ROUND2_STEP_BW(a, b, c, d, e, me2, 30); \
	if (t > 29) HASHCLASH_SHA1COMPRESS_ROUND2_STEP_BW(b, c, d, e, a, me2, 29); \
	if (t > 28) HASHCLASH_SHA1COMPRESS_ROUND2_STEP_BW(c, d, e, a, b, me2, 28); \
	if (t > 27) HASHCLASH_SHA1COMPRESS_ROUND2_STEP_BW(d, e, a, b, c, me2, 27); \
	if (t > 26) HASHCLASH_SHA1COMPRESS_ROUND2_STEP_BW(e, a, b, c, d, me2, 26); \
	if (t > 25) HASHCLASH_SHA1COMPRESS_ROUND2_STEP_BW(a, b, c, d, e, me2, 25); \
	if (t > 24) HASHCLASH_SHA1COMPRESS_ROUND2_STEP_BW(b, c, d, e, a, me2, 24); \
	if (t > 23) HASHCLASH_SHA1COMPRESS_ROUND2_STEP_BW(c, d, e, a, b, me2, 23); \
	if (t > 22) HASHCLASH_SHA1COMPRESS_ROUND2_STEP_BW(d, e, a, b, c, me2, 22); \
	if (t > 21) HASHCLASH_SHA1COMPRESS_ROUND2_STEP_BW(e, a, b, c, d, me2, 21); \
	if (t > 20) HASHCLASH_SHA1COMPRESS_ROUND2_STEP_BW(a, b, c, d, e, me2, 20); \
	if (t > 19) HASHCLASH_SHA1COMPRESS_ROUND1_STEP_BW(b, c, d, e, a, me2, 19); \
	if (t > 18) HASHCLASH_SHA1COMPRESS_ROUND1_STEP_BW(c, d, e, a, b, me2, 18); \
	if (t > 17) HASHCLASH_SHA1COMPRESS_ROUND1_STEP_BW(d, e, a, b, c, me2, 17); \
	if (t > 16) HASHCLASH_SHA1COMPRESS_ROUND1_STEP_BW(e, a, b, c, d, me2, 16); \
	if (t > 15) HASHCLASH_SHA1COMPRESS_ROUND1_STEP_BW(a, b, c, d, e, me2, 15); \
	if (t > 14) HASHCLASH_SHA1COMPRESS_ROUND1_STEP_BW(b, c, d, e, a, me2, 14); \
	if (t > 13) HASHCLASH_SHA1COMPRESS_ROUND1_STEP_BW(c, d, e, a, b, me2, 13); \
	if (t > 12) HASHCLASH_SHA1COMPRESS_ROUND1_STEP_BW(d, e, a, b, c, me2, 12); \
	if (t > 11) HASHCLASH_SHA1COMPRESS_ROUND1_STEP_BW(e, a, b, c, d, me2, 11); \
	if (t > 10) HASHCLASH_SHA1COMPRESS_ROUND1_STEP_BW(a, b, c, d, e, me2, 10); \
	if (t > 9) HASHCLASH_SHA1COMPRESS_ROUND1_STEP_BW(b, c, d, e, a, me2, 9); \
	if (t > 8) HASHCLASH_SHA1COMPRESS_ROUND1_STEP_BW(c, d, e, a, b, me2, 8); \
	if (t > 7) HASHCLASH_SHA1COMPRESS_ROUND1_STEP_BW(d, e, a, b, c, me2, 7); \
	if (t > 6) HASHCLASH_SHA1COMPRESS_ROUND1_STEP_BW(e, a, b, c, d, me2, 6); \
	if (t > 5) HASHCLASH_SHA1COMPRESS_ROUND1_STEP_BW(a, b, c, d, e, me2, 5); \
	if (t > 4) HASHCLASH_SHA1COMPRESS_ROUND1_STEP_BW(b, c, d, e, a, me2, 4); \
	if (t > 3) HASHCLASH_SHA1COMPRESS_ROUND1_STEP_BW(c, d, e, a, b, me2, 3); \
	if (t > 2) HASHCLASH_SHA1COMPRESS_ROUND1_STEP_BW(d, e, a, b, c, me2, 2); \
	if (t > 1) HASHCLASH_SHA1COMPRESS_ROUND1_STEP_BW(e, a, b, c, d, me2, 1); \
	if (t > 0) HASHCLASH_SHA1COMPRESS_ROUND1_STEP_BW(a, b, c, d, e, me2, 0); \
	ihvin[0] = a; ihvin[1] = b; ihvin[2] = c; ihvin[3] = d; ihvin[4] = e; \
	a = state[0]; b = state[1]; c = state[2]; d = state[3]; e = state[4]; \
	if (t <= 0) HASHCLASH_SHA1COMPRESS_ROUND1_STEP(a, b, c, d, e, me2, 0); \
	if (t <= 1) HASHCLASH_SHA1COMPRESS_ROUND1_STEP(e, a, b, c, d, me2, 1); \
	if (t <= 2) HASHCLASH_SHA1COMPRESS_ROUND1_STEP(d, e, a, b, c, me2, 2); \
	if (t <= 3) HASHCLASH_SHA1COMPRESS_ROUND1_STEP(c, d, e, a, b, me2, 3); \
	if (t <= 4) HASHCLASH_SHA1COMPRESS_ROUND1_STEP(b, c, d, e, a, me2, 4); \
	if (t <= 5) HASHCLASH_SHA1COMPRESS_ROUND1_STEP(a, b, c, d, e, me2, 5); \
	if (t <= 6) HASHCLASH_SHA1COMPRESS_ROUND1_STEP(e, a, b, c, d, me2, 6); \
	if (t <= 7) HASHCLASH_SHA1COMPRESS_ROUND1_STEP(d, e, a, b, c, me2, 7); \
	if (t <= 8) HASHCLASH_SHA1COMPRESS_ROUND1_STEP(c, d, e, a, b, me2, 8); \
	if (t <= 9) HASHCLASH_SHA1COMPRESS_ROUND1_STEP(b, c, d, e, a, me2, 9); \
	if (t <= 10) HASHCLASH_SHA1COMPRESS_ROUND1_STEP(a, b, c, d, e, me2, 10); \
	if (t <= 11) HASHCLASH_SHA1COMPRESS_ROUND1_STEP(e, a, b, c, d, me2, 11); \
	if (t <= 12) HASHCLASH_SHA1COMPRESS_ROUND1_STEP(d, e, a, b, c, me2, 12); \
	if (t <= 13) HASHCLASH_SHA1COMPRESS_ROUND1_STEP(c, d, e, a, b, me2, 13); \
	if (t <= 14) HASHCLASH_SHA1COMPRESS_ROUND1_STEP(b, c, d, e, a, me2, 14); \
	if (t <= 15) HASHCLASH_SHA1COMPRESS_ROUND1_STEP(a, b, c, d, e, me2, 15); \
	if (t <= 16) HASHCLASH_SHA1COMPRESS_ROUND1_STEP(e, a, b, c, d, me2, 16); \
	if (t <= 17) HASHCLASH_SHA1COMPRESS_ROUND1_STEP(d, e, a, b, c, me2, 17); \
	if (t <= 18) HASHCLASH_SHA1COMPRESS_ROUND1_STEP(c, d, e, a, b, me2, 18); \
	if (t <= 19) HASHCLASH_SHA1COMPRESS_ROUND1_STEP(b, c, d, e, a, me2, 19); \
	if (t <= 20) HASHCLASH_SHA1COMPRESS_ROUND2_STEP(a, b, c, d, e, me2, 20); \
	if (t <= 21) HASHCLASH_SHA1COMPRESS_ROUND2_STEP(e, a, b, c, d, me2, 21); \
	if (t <= 22) HASHCLASH_SHA1COMPRESS_ROUND2_STEP(d, e, a, b, c, me2, 22); \
	if (t <= 23) HASHCLASH_SHA1COMPRESS_ROUND2_STEP(c, d, e, a, b, me2, 23); \
	if (t <= 24) HASHCLASH_SHA1COMPRESS_ROUND2_STEP(b, c, d, e, a, me2, 24); \
	if (t <= 25) HASHCLASH_SHA1COMPRESS_ROUND2_STEP(a, b, c, d, e, me2, 25); \
	if (t <= 26) HASHCLASH_SHA1COMPRESS_ROUND2_STEP(e, a, b, c, d, me2, 26); \
	if (t <= 27) HASHCLASH_SHA1COMPRESS_ROUND2_STEP(d, e, a, b, c, me2, 27); \
	if (t <= 28) HASHCLASH_SHA1COMPRESS_ROUND2_STEP(c, d, e, a, b, me2, 28); \
	if (t <= 29) HASHCLASH_SHA1COMPRESS_ROUND2_STEP(b, c, d, e, a, me2, 29); \
	if (t <= 30) HASHCLASH_SHA1COMPRESS_ROUND2_STEP(a, b, c, d, e, me2, 30); \
	if (t <= 31) HASHCLASH_SHA1COMPRESS_ROUND2_STEP(e, a, b, c, d, me2, 31); \
	if (t <= 32) HASHCLASH_SHA1COMPRESS_ROUND2_STEP(d, e, a, b, c, me2, 32); \
	if (t <= 33) HASHCLASH_SHA1COMPRESS_ROUND2_STEP(c, d, e, a, b, me2, 33); \
	if (t <= 34) HASHCLASH_SHA1COMPRESS_ROUND2_STEP(b, c, d, e, a, me2, 34); \
	if (t <= 35) HASHCLASH_SHA1COMPRESS_ROUND2_STEP(a, b, c, d, e, me2, 35); \
	if (t <= 36) HASHCLASH_SHA1COMPRESS_ROUND2_STEP(e, a, b, c, d, me2, 36); \
	if (t <= 37) HASHCLASH_SHA1COMPRESS_ROUND2_STEP(d, e, a, b, c, me2, 37); \
	if (t <= 38) HASHCLASH_SHA1COMPRESS_ROUND2_STEP(c, d, e, a, b, me2, 38); \
	if (t <= 39) HASHCLASH_SHA1COMPRESS_ROUND2_STEP(b, c, d, e, a, me2, 39); \
	if (t <= 40) HASHCLASH_SHA1COMPRESS_ROUND3_STEP(a, b, c, d, e, me2, 40); \
	if (t <= 41) HASHCLASH_SHA1COMPRESS_ROUND3_STEP(e, a, b, c, d, me2, 41); \
	if (t <= 42) HASHCLASH_SHA1COMPRESS_ROUND3_STEP(d, e, a, b, c, me2, 42); \
	if (t <= 43) HASHCLASH_SHA1COMPRESS_ROUND3_STEP(c, d, e, a, b, me2, 43); \
	if (t <= 44) HASHCLASH_SHA1COMPRESS_ROUND3_STEP(b, c, d, e, a, me2, 44); \
	if (t <= 45) HASHCLASH_SHA1COMPRESS_ROUND3_STEP(a, b, c, d, e, me2, 45); \
	if (t <= 46) HASHCLASH_SHA1COMPRESS_ROUND3_STEP(e, a, b, c, d, me2, 46); \
	if (t <= 47) HASHCLASH_SHA1COMPRESS_ROUND3_STEP(d, e, a, b, c, me2, 47); \
	if (t <= 48) HASHCLASH_SHA1COMPRESS_ROUND3_STEP(c, d, e, a, b, me2, 48); \
	if (t <= 49) HASHCLASH_SHA1COMPRESS_ROUND3_STEP(b, c, d, e, a, me2, 49); \
	if (t <= 50) HASHCLASH_SHA1COMPRESS_ROUND3_STEP(a, b, c, d, e, me2, 50); \
	if (t <= 51) HASHCLASH_SHA1COMPRESS_ROUND3_STEP(e, a, b, c, d, me2, 51); \
	if (t <= 52) HASHCLASH_SHA1COMPRESS_ROUND3_STEP(d, e, a, b, c, me2, 52); \
	if (t <= 53) HASHCLASH_SHA1COMPRESS_ROUND3_STEP(c, d, e, a, b, me2, 53); \
	if (t <= 54) HASHCLASH_SHA1COMPRESS_ROUND3_STEP(b, c, d, e, a, me2, 54); \
	if (t <= 55) HASHCLASH_SHA1COMPRESS_ROUND3_STEP(a, b, c, d, e, me2, 55); \
	if (t <= 56) HASHCLASH_SHA1COMPRESS_ROUND3_STEP(e, a, b, c, d, me2, 56); \
	if (t <= 57) HASHCLASH_SHA1COMPRESS_ROUND3_STEP(d, e, a, b, c, me2, 57); \
	if (t <= 58) HASHCLASH_SHA1COMPRESS_ROUND3_STEP(c, d, e, a, b, me2, 58); \
	if (t <= 59) HASHCLASH_SHA1COMPRESS_ROUND3_STEP(b, c, d, e, a, me2, 59); \
	if (t <= 60) HASHCLASH_SHA1COMPRESS_ROUND4_STEP(a, b, c, d, e, me2, 60); \
	if (t <= 61) HASHCLASH_SHA1COMPRESS_ROUND4_STEP(e, a, b, c, d, me2, 61); \
	if (t <= 62) HASHCLASH_SHA1COMPRESS_ROUND4_STEP(d, e, a, b, c, me2, 62); \
	if (t <= 63) HASHCLASH_SHA1COMPRESS_ROUND4_STEP(c, d, e, a, b, me2, 63); \
	if (t <= 64) HASHCLASH_SHA1COMPRESS_ROUND4_STEP(b, c, d, e, a, me2, 64); \
	if (t <= 65) HASHCLASH_SHA1COMPRESS_ROUND4_STEP(a, b, c, d, e, me2, 65); \
	if (t <= 66) HASHCLASH_SHA1COMPRESS_ROUND4_STEP(e, a, b, c, d, me2, 66); \
	if (t <= 67) HASHCLASH_SHA1COMPRESS_ROUND4_STEP(d, e, a, b, c, me2, 67); \
	if (t <= 68) HASHCLASH_SHA1COMPRESS_ROUND4_STEP(c, d, e, a, b, me2, 68); \
	if (t <= 69) HASHCLASH_SHA1COMPRESS_ROUND4_STEP(b, c, d, e, a, me2, 69); \
	if (t <= 70) HASHCLASH_SHA1COMPRESS_ROUND4_STEP(a, b, c, d, e, me2, 70); \
	if (t <= 71) HASHCLASH_SHA1COMPRESS_ROUND4_STEP(e, a, b, c, d, me2, 71); \
	if (t <= 72) HASHCLASH_SHA1COMPRESS_ROUND4_STEP(d, e, a, b, c, me2, 72); \
	if (t <= 73) HASHCLASH_SHA1COMPRESS_ROUND4_STEP(c, d, e, a, b, me2, 73); \
	if (t <= 74) HASHCLASH_SHA1COMPRESS_ROUND4_STEP(b, c, d, e, a, me2, 74); \
	if (t <= 75) HASHCLASH_SHA1COMPRESS_ROUND4_STEP(a, b, c, d, e, me2, 75); \
	if (t <= 76) HASHCLASH_SHA1COMPRESS_ROUND4_STEP(e, a, b, c, d, me2, 76); \
	if (t <= 77) HASHCLASH_SHA1COMPRESS_ROUND4_STEP(d, e, a, b, c, me2, 77); \
	if (t <= 78) HASHCLASH_SHA1COMPRESS_ROUND4_STEP(c, d, e, a, b, me2, 78); \
	if (t <= 79) HASHCLASH_SHA1COMPRESS_ROUND4_STEP(b, c, d, e, a, me2, 79); \
	ihvout[0] = ihvin[0] + a; ihvout[1] = ihvin[1] + b; ihvout[2] = ihvin[2] + c; ihvout[3] = ihvin[3] + d; ihvout[4] = ihvin[4] + e; \
} 

SHA1_RECOMPRESS(0)
SHA1_RECOMPRESS(1)
SHA1_RECOMPRESS(2)
SHA1_RECOMPRESS(3)
SHA1_RECOMPRESS(4)
SHA1_RECOMPRESS(5)
SHA1_RECOMPRESS(6)
SHA1_RECOMPRESS(7)
SHA1_RECOMPRESS(8)
SHA1_RECOMPRESS(9)

SHA1_RECOMPRESS(10)
SHA1_RECOMPRESS(11)
SHA1_RECOMPRESS(12)
SHA1_RECOMPRESS(13)
SHA1_RECOMPRESS(14)
SHA1_RECOMPRESS(15)
SHA1_RECOMPRESS(16)
SHA1_RECOMPRESS(17)
SHA1_RECOMPRESS(18)
SHA1_RECOMPRESS(19)

SHA1_RECOMPRESS(20)
SHA1_RECOMPRESS(21)
SHA1_RECOMPRESS(22)
SHA1_RECOMPRESS(23)
SHA1_RECOMPRESS(24)
SHA1_RECOMPRESS(25)
SHA1_RECOMPRESS(26)
SHA1_RECOMPRESS(27)
SHA1_RECOMPRESS(28)
SHA1_RECOMPRESS(29)

SHA1_RECOMPRESS(30)
SHA1_RECOMPRESS(31)
SHA1_RECOMPRESS(32)
SHA1_RECOMPRESS(33)
SHA1_RECOMPRESS(34)
SHA1_RECOMPRESS(35)
SHA1_RECOMPRESS(36)
SHA1_RECOMPRESS(37)
SHA1_RECOMPRESS(38)
SHA1_RECOMPRESS(39)

SHA1_RECOMPRESS(40)
SHA1_RECOMPRESS(41)
SHA1_RECOMPRESS(42)
SHA1_RECOMPRESS(43)
SHA1_RECOMPRESS(44)
SHA1_RECOMPRESS(45)
SHA1_RECOMPRESS(46)
SHA1_RECOMPRESS(47)
SHA1_RECOMPRESS(48)
SHA1_RECOMPRESS(49)

SHA1_RECOMPRESS(50)
SHA1_RECOMPRESS(51)
SHA1_RECOMPRESS(52)
SHA1_RECOMPRESS(53)
SHA1_RECOMPRESS(54)
SHA1_RECOMPRESS(55)
SHA1_RECOMPRESS(56)
SHA1_RECOMPRESS(57)
SHA1_RECOMPRESS(58)
SHA1_RECOMPRESS(59)

SHA1_RECOMPRESS(60)
SHA1_RECOMPRESS(61)
SHA1_RECOMPRESS(62)
SHA1_RECOMPRESS(63)
SHA1_RECOMPRESS(64)
SHA1_RECOMPRESS(65)
SHA1_RECOMPRESS(66)
SHA1_RECOMPRESS(67)
SHA1_RECOMPRESS(68)
SHA1_RECOMPRESS(69)

SHA1_RECOMPRESS(70)
SHA1_RECOMPRESS(71)
SHA1_RECOMPRESS(72)
SHA1_RECOMPRESS(73)
SHA1_RECOMPRESS(74)
SHA1_RECOMPRESS(75)
SHA1_RECOMPRESS(76)
SHA1_RECOMPRESS(77)
SHA1_RECOMPRESS(78)
SHA1_RECOMPRESS(79)

sha1_recompression_type sha1_recompression_step[80] =
{
	sha1recompress_fast_0, sha1recompress_fast_1, sha1recompress_fast_2, sha1recompress_fast_3, sha1recompress_fast_4, sha1recompress_fast_5, sha1recompress_fast_6, sha1recompress_fast_7, sha1recompress_fast_8, sha1recompress_fast_9,
	sha1recompress_fast_10, sha1recompress_fast_11, sha1recompress_fast_12, sha1recompress_fast_13, sha1recompress_fast_14, sha1recompress_fast_15, sha1recompress_fast_16, sha1recompress_fast_17, sha1recompress_fast_18, sha1recompress_fast_19,
	sha1recompress_fast_20, sha1recompress_fast_21, sha1recompress_fast_22, sha1recompress_fast_23, sha1recompress_fast_24, sha1recompress_fast_25, sha1recompress_fast_26, sha1recompress_fast_27, sha1recompress_fast_28, sha1recompress_fast_29,
	sha1recompress_fast_30, sha1recompress_fast_31, sha1recompress_fast_32, sha1recompress_fast_33, sha1recompress_fast_34, sha1recompress_fast_35, sha1recompress_fast_36, sha1recompress_fast_37, sha1recompress_fast_38, sha1recompress_fast_39,
	sha1recompress_fast_40, sha1recompress_fast_41, sha1recompress_fast_42, sha1recompress_fast_43, sha1recompress_fast_44, sha1recompress_fast_45, sha1recompress_fast_46, sha1recompress_fast_47, sha1recompress_fast_48, sha1recompress_fast_49,
	sha1recompress_fast_50, sha1recompress_fast_51, sha1recompress_fast_52, sha1recompress_fast_53, sha1recompress_fast_54, sha1recompress_fast_55, sha1recompress_fast_56, sha1recompress_fast_57, sha1recompress_fast_58, sha1recompress_fast_59,
	sha1recompress_fast_60, sha1recompress_fast_61, sha1recompress_fast_62, sha1recompress_fast_63, sha1recompress_fast_64, sha1recompress_fast_65, sha1recompress_fast_66, sha1recompress_fast_67, sha1recompress_fast_68, sha1recompress_fast_69,
	sha1recompress_fast_70, sha1recompress_fast_71, sha1recompress_fast_72, sha1recompress_fast_73, sha1recompress_fast_74, sha1recompress_fast_75, sha1recompress_fast_76, sha1recompress_fast_77, sha1recompress_fast_78, sha1recompress_fast_79,
};





void sha1_process(SHA1_CTX* ctx, const uint32_t block[16]) 
{
	unsigned i, j;
	uint32_t ubc_dv_mask[DVMASKSIZE];
	uint32_t ihvtmp[5];
	for (i=0; i < DVMASKSIZE; ++i)
		ubc_dv_mask[i]=0;
	ctx->ihv1[0] = ctx->ihv[0];
	ctx->ihv1[1] = ctx->ihv[1];
	ctx->ihv1[2] = ctx->ihv[2];
	ctx->ihv1[3] = ctx->ihv[3];
	ctx->ihv1[4] = ctx->ihv[4];
	memcpy(ctx->m1, block, 64);
	sha1_message_expansion(ctx->m1);
	if (ctx->detect_coll && ctx->ubc_check)
	{
		ubc_check(ctx->m1, ubc_dv_mask);
	}
	sha1_compression_states(ctx->ihv, ctx->m1, ctx->states);
	if (ctx->detect_coll)
	{
		for (i = 0; sha1_dvs[i].dvType != 0; ++i) 
		{
			if ((0 == ctx->ubc_check) || (((uint32_t)(1) << sha1_dvs[i].maskb) & ubc_dv_mask[sha1_dvs[i].maski]))
			{
				for (j = 0; j < 80; ++j)
					ctx->m2[j] = ctx->m1[j] ^ sha1_dvs[i].dm[j];
				(sha1_recompression_step[sha1_dvs[i].testt])(ctx->ihv2, ihvtmp, ctx->m2, ctx->states[sha1_dvs[i].testt]);
				// to verify SHA-1 collision detection code with collisions for reduced-step SHA-1
				if ((ihvtmp[0] == ctx->ihv[0] && ihvtmp[1] == ctx->ihv[1] && ihvtmp[2] == ctx->ihv[2] && ihvtmp[3] == ctx->ihv[3] && ihvtmp[4] == ctx->ihv[4])
					|| (ctx->reduced_round_coll && ctx->ihv1[0] == ctx->ihv2[0] && ctx->ihv1[1] == ctx->ihv2[1] && ctx->ihv1[2] == ctx->ihv2[2] && ctx->ihv1[3] == ctx->ihv2[3] && ctx->ihv1[4] == ctx->ihv2[4]))
				{
					ctx->found_collision = 1;
					// TODO: call callback
					if (ctx->callback != NULL)
						ctx->callback(ctx->total - 64, ctx->ihv1, ctx->ihv2, ctx->m1, ctx->m2);

					if (ctx->safe_hash) 
					{
						sha1_compression_W(ctx->ihv, ctx->m1);
						sha1_compression_W(ctx->ihv, ctx->m1);
					}

					break;
				}
			}
		}
	}
}





void swap_bytes(uint32_t val[16]) 
{
	unsigned i;
	for (i = 0; i < 16; ++i) 
	{
		val[i] = ((val[i] << 8) & 0xFF00FF00) | ((val[i] >> 8) & 0xFF00FF);
		val[i] = (val[i] << 16) | (val[i] >> 16);
	}
}

void SHA1DCInit(SHA1_CTX* ctx) 
{
	static const union { unsigned char bytes[4]; uint32_t value; } endianness = { { 0, 1, 2, 3 } };
	static const uint32_t littleendian = 0x03020100;
	ctx->total = 0;
	ctx->ihv[0] = 0x67452301;
	ctx->ihv[1] = 0xEFCDAB89;
	ctx->ihv[2] = 0x98BADCFE;
	ctx->ihv[3] = 0x10325476;
	ctx->ihv[4] = 0xC3D2E1F0;
	ctx->found_collision = 0;
	ctx->safe_hash = 1;
	ctx->ubc_check = 1;
	ctx->detect_coll = 1;
	ctx->reduced_round_coll = 0;
	ctx->bigendian = (endianness.value != littleendian);
	ctx->callback = NULL;
}

void SHA1DCSetSafeHash(SHA1_CTX* ctx, int safehash)
{
	if (safehash)
		ctx->safe_hash = 1;
	else
		ctx->safe_hash = 0;
}


void SHA1DCSetUseUBC(SHA1_CTX* ctx, int ubc_check)
{
	if (ubc_check)
		ctx->ubc_check = 1;
	else
		ctx->ubc_check = 0;
}

void SHA1DCSetUseDetectColl(SHA1_CTX* ctx, int detect_coll)
{
	if (detect_coll)
		ctx->detect_coll = 1;
	else
		ctx->detect_coll = 0;
}

void SHA1DCSetDetectReducedRoundCollision(SHA1_CTX* ctx, int reduced_round_coll)
{
	if (reduced_round_coll)
		ctx->reduced_round_coll = 1;
	else
		ctx->reduced_round_coll = 0;
}

void SHA1DCSetCallback(SHA1_CTX* ctx, collision_block_callback callback)
{
	ctx->callback = callback;
}

void SHA1DCUpdate(SHA1_CTX* ctx, const char* buf, unsigned len) 
{
	unsigned left, fill;
	if (len == 0) 
		return;

	left = ctx->total & 63;
	fill = 64 - left;

	if (left && len >= fill) 
	{
		ctx->total += fill;
		memcpy(ctx->buffer + left, buf, fill);
		if (!ctx->bigendian)
			swap_bytes((uint32_t*)(ctx->buffer));
		sha1_process(ctx, (uint32_t*)(ctx->buffer));
		buf += fill;
		len -= fill;
		left = 0;
	}
	while (len >= 64) 
	{
		ctx->total += 64;
		if (!ctx->bigendian) 
		{
			memcpy(ctx->buffer, buf, 64);
			swap_bytes((uint32_t*)(ctx->buffer));
			sha1_process(ctx, (uint32_t*)(ctx->buffer));
		}
		else
			sha1_process(ctx, (uint32_t*)(buf));
		buf += 64;
		len -= 64;
	}
	if (len > 0) 
	{
		ctx->total += len;
		memcpy(ctx->buffer + left, buf, len);
	}
}

static const unsigned char sha1_padding[64] =
{
	0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

int SHA1DCFinal(unsigned char output[20], SHA1_CTX *ctx)
{
	uint32_t last = ctx->total & 63;
	uint32_t padn = (last < 56) ? (56 - last) : (120 - last);
	uint64_t total;
	SHA1DCUpdate(ctx, (const char*)(sha1_padding), padn);
	
	total = ctx->total - padn;
	total <<= 3;
	ctx->buffer[56] = (unsigned char)(total >> 56);
	ctx->buffer[57] = (unsigned char)(total >> 48);
	ctx->buffer[58] = (unsigned char)(total >> 40);
	ctx->buffer[59] = (unsigned char)(total >> 32);
	ctx->buffer[60] = (unsigned char)(total >> 24);
	ctx->buffer[61] = (unsigned char)(total >> 16);
	ctx->buffer[62] = (unsigned char)(total >> 8);
	ctx->buffer[63] = (unsigned char)(total);
	if (!ctx->bigendian)
		swap_bytes((uint32_t*)(ctx->buffer));
	sha1_process(ctx, (uint32_t*)(ctx->buffer));
	output[0] = (unsigned char)(ctx->ihv[0] >> 24);
	output[1] = (unsigned char)(ctx->ihv[0] >> 16);
	output[2] = (unsigned char)(ctx->ihv[0] >> 8);
	output[3] = (unsigned char)(ctx->ihv[0]);
	output[4] = (unsigned char)(ctx->ihv[1] >> 24);
	output[5] = (unsigned char)(ctx->ihv[1] >> 16);
	output[6] = (unsigned char)(ctx->ihv[1] >> 8);
	output[7] = (unsigned char)(ctx->ihv[1]);
	output[8] = (unsigned char)(ctx->ihv[2] >> 24);
	output[9] = (unsigned char)(ctx->ihv[2] >> 16);
	output[10] = (unsigned char)(ctx->ihv[2] >> 8);
	output[11] = (unsigned char)(ctx->ihv[2]);
	output[12] = (unsigned char)(ctx->ihv[3] >> 24);
	output[13] = (unsigned char)(ctx->ihv[3] >> 16);
	output[14] = (unsigned char)(ctx->ihv[3] >> 8);
	output[15] = (unsigned char)(ctx->ihv[3]);
	output[16] = (unsigned char)(ctx->ihv[4] >> 24);
	output[17] = (unsigned char)(ctx->ihv[4] >> 16);
	output[18] = (unsigned char)(ctx->ihv[4] >> 8);
	output[19] = (unsigned char)(ctx->ihv[4]);
	return ctx->found_collision;
}
