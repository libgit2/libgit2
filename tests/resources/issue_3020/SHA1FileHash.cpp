/* Author: macote */

#include "SHA1FileHash.h"

void SHA1FileHash::Initialize()
{
	context_.state[0] = 0x67452301;
	context_.state[1] = 0xEFCDAB89;
	context_.state[2] = 0x98BADCFE;
	context_.state[3] = 0x10325476;
	context_.state[4] = 0xC3D2E1F0;
	context_.count[0] = context_.count[1] = 0;
}

void SHA1FileHash::Update(const UINT32 bytecount)
{
	UINT32 i, index = (context_.count[0] >> 3) & 63;
	if ((context_.count[0] += bytecount << 3) < (bytecount << 3))
	{
		context_.count[1]++;
	}
	context_.count[1] += (bytecount >> 29);
	if ((index + bytecount) > 63)
	{
		i = 64 - index;
		CopyMemory(&context_.buffer[index], buffer_, i);
		Transform(context_.state, (PUINT32)context_.buffer);
		for (; i + 63 < bytecount; i += 64)
		{
			Transform(context_.state, (PUINT32)(buffer_ + i));
		}
		index = 0;
	}
	else
	{
		i = 0;
	}
	CopyMemory(&context_.buffer[index], buffer_ + i, bytecount - i);
}

void SHA1FileHash::Finalize()
{
	BYTE finalcount[8];
	for (UINT i = 0; i < 8; i++)
	{
		finalcount[i] = (BYTE)((context_.count[(i >= 4 ? 0 : 1)] >> ((3 - (i & 3)) * 8)) & 255);
	}
	CopyMemory(buffer_, "\200", 1);
	Update(1);
	CopyMemory(buffer_, "\0", 1);
	while ((context_.count[0] & 504) != 448)
	{
		Update(1);
	}
	CopyMemory(buffer_, finalcount, sizeof(finalcount));
	Update(8);
	for (UINT i = 0; i < sizeof(hash_); i++)
	{
		hash_[i] = (BYTE)((context_.state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255);
	}
}

void SHA1FileHash::ConvertHashToDigestString()
{
	std::wstringstream wss;
	wss << std::hex << std::setw(2) << std::setfill(L'0') << std::uppercase;
	for (UINT i = 0; i < sizeof(hash_); i++)
	{
		wss << hash_[i];
	}
	digest_.append(wss.str());
}

void SHA1FileHash::Transform(UINT32 state[5], PUINT32 buffer)
{
	typedef union {
		BYTE c[64];
		UINT32 l[16];
	} CHAR64LONG16, *PCHAR64LONG16;
	PCHAR64LONG16 block;
	block = (PCHAR64LONG16)buffer;

	register UINT32 a, b, c, d, e;
	a = state[0];
	b = state[1];
	c = state[2];
	d = state[3];
	e = state[4];

#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))
#define blk0(i) (block->l[i] = (rol(block->l[i], 24) & 0xFF00FF00) \
	| (rol(block->l[i], 8) & 0x00FF00FF))
#define blk(i) (block->l[i & 15] = rol(block->l[(i + 13) & 15] ^ block->l[(i + 8) & 15] \
	^ block->l[(i + 2) & 15] ^ block->l[i & 15], 1))
#define R0(v, w, x, y, z, i) z += ((w & (x ^ y)) ^ y) + blk0(i) + 0x5A827999 + rol(v, 5); w = rol(w, 30);
#define R1(v, w, x, y, z, i) z += ((w & (x ^ y)) ^ y) + blk(i) + 0x5A827999 + rol(v, 5); w = rol(w, 30);
#define R2(v, w, x, y, z, i) z += (w ^ x ^ y) + blk(i) + 0x6ED9EBA1 + rol(v, 5); w = rol(w, 30);
#define R3(v, w, x, y, z, i) z += (((w | x) & y) | (w & x)) + blk(i) + 0x8F1BBCDC + rol(v, 5); w = rol(w, 30);
#define R4(v, w, x, y, z, i) z += (w ^ x ^ y) + blk(i) + 0xCA62C1D6 + rol(v, 5); w = rol(w, 30);

	R0(a, b, c, d, e, 0); R0(e, a, b, c, d, 1); R0(d, e, a, b, c, 2); R0(c, d, e, a, b, 3);
	R0(b, c, d, e, a, 4); R0(a, b, c, d, e, 5); R0(e, a, b, c, d, 6); R0(d, e, a, b, c, 7);
	R0(c, d, e, a, b, 8); R0(b, c, d, e, a, 9); R0(a, b, c, d, e, 10); R0(e, a, b, c, d, 11);
	R0(d, e, a, b, c, 12); R0(c, d, e, a, b, 13); R0(b, c, d, e, a, 14); R0(a, b, c, d, e, 15);
	R1(e, a, b, c, d, 16); R1(d, e, a, b, c, 17); R1(c, d, e, a, b, 18); R1(b, c, d, e, a, 19);
	R2(a, b, c, d, e, 20); R2(e, a, b, c, d, 21); R2(d, e, a, b, c, 22); R2(c, d, e, a, b, 23);
	R2(b, c, d, e, a, 24); R2(a, b, c, d, e, 25); R2(e, a, b, c, d, 26); R2(d, e, a, b, c, 27);
	R2(c, d, e, a, b, 28); R2(b, c, d, e, a, 29); R2(a, b, c, d, e, 30); R2(e, a, b, c, d, 31);
	R2(d, e, a, b, c, 32); R2(c, d, e, a, b, 33); R2(b, c, d, e, a, 34); R2(a, b, c, d, e, 35);
	R2(e, a, b, c, d, 36); R2(d, e, a, b, c, 37); R2(c, d, e, a, b, 38); R2(b, c, d, e, a, 39);
	R3(a, b, c, d, e, 40); R3(e, a, b, c, d, 41); R3(d, e, a, b, c, 42); R3(c, d, e, a, b, 43);
	R3(b, c, d, e, a, 44); R3(a, b, c, d, e, 45); R3(e, a, b, c, d, 46); R3(d, e, a, b, c, 47);
	R3(c, d, e, a, b, 48); R3(b, c, d, e, a, 49); R3(a, b, c, d, e, 50); R3(e, a, b, c, d, 51);
	R3(d, e, a, b, c, 52); R3(c, d, e, a, b, 53); R3(b, c, d, e, a, 54); R3(a, b, c, d, e, 55);
	R3(e, a, b, c, d, 56); R3(d, e, a, b, c, 57); R3(c, d, e, a, b, 58); R3(b, c, d, e, a, 59);
	R4(a, b, c, d, e, 60); R4(e, a, b, c, d, 61); R4(d, e, a, b, c, 62); R4(c, d, e, a, b, 63);
	R4(b, c, d, e, a, 64); R4(a, b, c, d, e, 65); R4(e, a, b, c, d, 66); R4(d, e, a, b, c, 67);
	R4(c, d, e, a, b, 68); R4(b, c, d, e, a, 69); R4(a, b, c, d, e, 70); R4(e, a, b, c, d, 71);
	R4(d, e, a, b, c, 72); R4(c, d, e, a, b, 73); R4(b, c, d, e, a, 74); R4(a, b, c, d, e, 75);
	R4(e, a, b, c, d, 76); R4(d, e, a, b, c, 77); R4(c, d, e, a, b, 78); R4(b, c, d, e, a, 79);

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;

	a = b = c = d = e = 0;
}



