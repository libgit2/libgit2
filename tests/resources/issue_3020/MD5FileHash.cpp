

void MD5FileHash::Update(const UINT32 bytecount)
{
	UINT32 index = context_.count[0];	// update bitcount
	context_.count[0] = index + (bytecount << 3);
	if (context_.count[0] < index)
	{
		context_.count[1]++;	// carry from low to high
	}
	context_.count[1] += bytecount >> 29;
	index = (index >> 3) & 63;	// bytes already in shsInfo->data
	UINT32 bytesleft = bytecount;
	if (index > 0)
	{
		// handle any leading odd-sized chunks
		PBYTE ctxbuffer = (PBYTE)context_.buffer + index;
		index = 64 - index;
		if (bytecount < index)
		{
			CopyMemory(ctxbuffer, buffer_, bytecount);
			return;
		}
		CopyMemory(ctxbuffer, buffer_, index);
		Transform(context_.state, (PUINT32)context_.buffer);
		ctxbuffer += index;
		bytesleft -= index;
	}
	// process data in 64-byte chunks
	PBYTE buffer = buffer_;
	while (bytesleft >= 64)
	{
		CopyMemory(context_.buffer, buffer, sizeof(context_.buffer));
		Transform(context_.state, (PUINT32)context_.buffer);
		buffer += 64;
		bytesleft -= 64;
	}
	// handle any remaining bytes of data.
	CopyMemory(context_.buffer, buffer, bytesleft);
}

void MD5FileHash::Finalize()
{
	UINT32 index = (context_.count[0] >> 3) & 63;	// compute number of bytes mod 64
	// set the first char of padding to 0x80.  this is safe since there is
	// always at least one byte free
	PBYTE ctxbuffer = context_.buffer + index;
	*ctxbuffer++ = 0x80;
	index = 64 - 1 - index;	// bytes of padding needed to make 64 bytes
	// pad out to 56 mod 64
	if (index < 8)
	{
		// two lots of padding:  pad the first block to 64 bytes
		ZeroMemory(ctxbuffer, index);
		Transform(context_.state, (PUINT32)context_.buffer);
		// now fill the next block with 56 bytes
		ZeroMemory(context_.buffer, 56);
	}
	else
	{
		ZeroMemory(ctxbuffer, index - 8);	// pad block to 56 bytes
	}
	// append length in bits and transform
	typedef union
	{
		BYTE c[64];
		UINT32 l[16];
	} CHAR64LONG16, *PCHAR64LONG16;
	PCHAR64LONG16 bufferlong = (PCHAR64LONG16)context_.buffer;
	bufferlong->l[14] = context_.count[0];
	bufferlong->l[15] = context_.count[1];
	Transform(context_.state, (PUINT32)context_.buffer);
	CopyMemory(hash_, context_.state, sizeof(hash_));
}

void MD5FileHash::ConvertHashToDigestString()
{
	std::wstringstream wss;
	wss << std::hex << std::setw(2) << std::setfill(L'0') << std::uppercase;
	for (UINT i = 0; i < sizeof(hash_); i++)
	{
		wss << hash_[i];
	}
	digest_.append(wss.str());
}

void MD5FileHash::Transform(UINT32 state[4], PUINT32 buffer)
{
	register UINT32 a, b, c, d;

	a = state[0];
	b = state[1];
	c = state[2];
	d = state[3];

#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

#define MD5STEP(f, w, x, y, z, data, s) (w += f(x, y, z) + data, w = w << s | w >> (32 - s), w += x)

	MD5STEP(F1, a, b, c, d, buffer[0]  + 0xD76AA478L, 7);
	MD5STEP(F1, d, a, b, c, buffer[1]  + 0xE8C7B756L, 12);
	MD5STEP(F1, c, d, a, b, buffer[2]  + 0x242070DBL, 17);
	MD5STEP(F1, b, c, d, a, buffer[3]  + 0xC1BDCEEEL, 22);
	MD5STEP(F1, a, b, c, d, buffer[4]  + 0xF57C0FAFL, 7);
	MD5STEP(F1, d, a, b, c, buffer[5]  + 0x4787C62AL, 12);
	MD5STEP(F1, c, d, a, b, buffer[6]  + 0xA8304613L, 17);
	MD5STEP(F1, b, c, d, a, buffer[7]  + 0xFD469501L, 22);
	MD5STEP(F1, a, b, c, d, buffer[8]  + 0x698098D8L, 7);
	MD5STEP(F1, d, a, b, c, buffer[9]  + 0x8B44F7AFL, 12);
	MD5STEP(F1, c, d, a, b, buffer[10] + 0xFFFF5BB1L, 17);
	MD5STEP(F1, b, c, d, a, buffer[11] + 0x895CD7BEL, 22);
	MD5STEP(F1, a, b, c, d, buffer[12] + 0x6B901122L, 7);
	MD5STEP(F1, d, a, b, c, buffer[13] + 0xFD987193L, 12);
	MD5STEP(F1, c, d, a, b, buffer[14] + 0xA679438EL, 17);
	MD5STEP(F1, b, c, d, a, buffer[15] + 0x49B40821L, 22);

	MD5STEP(F2, a, b, c, d, buffer[1]  + 0xF61E2562L, 5);
	MD5STEP(F2, d, a, b, c, buffer[6]  + 0xC040B340L, 9);
	MD5STEP(F2, c, d, a, b, buffer[11] + 0x265E5A51L, 14);
	MD5STEP(F2, b, c, d, a, buffer[0]  + 0xE9B6C7AAL, 20);
	MD5STEP(F2, a, b, c, d, buffer[5]  + 0xD62F105DL, 5);
	MD5STEP(F2, d, a, b, c, buffer[10] + 0x02441453L, 9);
	MD5STEP(F2, c, d, a, b, buffer[15] + 0xD8A1E681L, 14);
	MD5STEP(F2, b, c, d, a, buffer[4]  + 0xE7D3FBC8L, 20);
	MD5STEP(F2, a, b, c, d, buffer[9]  + 0x21E1CDE6L, 5);
	MD5STEP(F2, d, a, b, c, buffer[14] + 0xC33707D6L, 9);
	MD5STEP(F2, c, d, a, b, buffer[3]  + 0xF4D50D87L, 14);
	MD5STEP(F2, b, c, d, a, buffer[8]  + 0x455A14EDL, 20);
	MD5STEP(F2, a, b, c, d, buffer[13] + 0xA9E3E905L, 5);
	MD5STEP(F2, d, a, b, c, buffer[2]  + 0xFCEFA3F8L, 9);
	MD5STEP(F2, c, d, a, b, buffer[7]  + 0x676F02D9L, 14);
	MD5STEP(F2, b, c, d, a, buffer[12] + 0x8D2A4C8AL, 20);

	MD5STEP(F3, a, b, c, d, buffer[5]  + 0xFFFA3942L, 4);
	MD5STEP(F3, d, a, b, c, buffer[8]  + 0x8771F681L, 11);
	MD5STEP(F3, c, d, a, b, buffer[11] + 0x6D9D6122L, 16);
	MD5STEP(F3, b, c, d, a, buffer[14] + 0xFDE5380CL, 23);
	MD5STEP(F3, a, b, c, d, buffer[1]  + 0xA4BEEA44L, 4);
	MD5STEP(F3, d, a, b, c, buffer[4]  + 0x4BDECFA9L, 11);
	MD5STEP(F3, c, d, a, b, buffer[7]  + 0xF6BB4B60L, 16);
	MD5STEP(F3, b, c, d, a, buffer[10] + 0xBEBFBC70L, 23);
	MD5STEP(F3, a, b, c, d, buffer[13] + 0x289B7EC6L, 4);
	MD5STEP(F3, d, a, b, c, buffer[0]  + 0xEAA127FAL, 11);
	MD5STEP(F3, c, d, a, b, buffer[3]  + 0xD4EF3085L, 16);
	MD5STEP(F3, b, c, d, a, buffer[6]  + 0x04881D05L, 23);
	MD5STEP(F3, a, b, c, d, buffer[9]  + 0xD9D4D039L, 4);
	MD5STEP(F3, d, a, b, c, buffer[12] + 0xE6DB99E5L, 11);
	MD5STEP(F3, c, d, a, b, buffer[15] + 0x1FA27CF8L, 16);
	MD5STEP(F3, b, c, d, a, buffer[2]  + 0xC4AC5665L, 23);

	MD5STEP(F4, a, b, c, d, buffer[0]  + 0xF4292244L, 6);
	MD5STEP(F4, d, a, b, c, buffer[7]  + 0x432AFF97L, 10);
	MD5STEP(F4, c, d, a, b, buffer[14] + 0xAB9423A7L, 15);
	MD5STEP(F4, b, c, d, a, buffer[5]  + 0xFC93A039L, 21);
	MD5STEP(F4, a, b, c, d, buffer[12] + 0x655B59C3L, 6);
	MD5STEP(F4, d, a, b, c, buffer[3]  + 0x8F0CCC92L, 10);
	MD5STEP(F4, c, d, a, b, buffer[10] + 0xFFEFF47DL, 15);
	MD5STEP(F4, b, c, d, a, buffer[1]  + 0x85845DD1L, 21);
	MD5STEP(F4, a, b, c, d, buffer[8]  + 0x6FA87E4FL, 6);
	MD5STEP(F4, d, a, b, c, buffer[15] + 0xFE2CE6E0L, 10);
	MD5STEP(F4, c, d, a, b, buffer[6]  + 0xA3014314L, 15);
	MD5STEP(F4, b, c, d, a, buffer[13] + 0x4E0811A1L, 21);
	MD5STEP(F4, a, b, c, d, buffer[4]  + 0xF7537E82L, 6);
	MD5STEP(F4, d, a, b, c, buffer[11] + 0xBD3AF235L, 10);
	MD5STEP(F4, c, d, a, b, buffer[2]  + 0x2AD7D2BBL, 15);
	MD5STEP(F4, b, c, d, a, buffer[9]  + 0xEB86D391L, 21);

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;

	a = b = c = d = 0;
}
