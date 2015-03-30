/* Author: macote */

#ifndef MD5FILEHASH_H_
#define MD5FILEHASH_H_

#include "FileHash.h"
#include <iomanip>
#include <sstream>
#include <string>
#include <Windows.h>

struct MD5Context
{
	UINT32 state[4];	// state (ABCD)
	UINT32 count[2];	// number of bits, modulo 2^64 (lsb first)
	BYTE buffer[64];	// input buffer
};

class MD5FileHash : public FileHash
{
public:
#if _MSC_VER < 1900
	MD5FileHash(const std::wstring& filepath, const DWORD buffersize) : FileHash(filepath, buffersize) { };
	MD5FileHash(const std::wstring& filepath) : FileHash(filepath) { };
#else
	using FileHash::FileHash;
#endif
private:
	void Initialize();
	void Update(const UINT32 bytecount);
	void Finalize();
	void Transform(UINT32 state[4], PUINT32 buffer);
	void ConvertHashToDigestString();
	BYTE hash_[16];
	MD5Context context_;
};

#endif /* MD5FILEHASH_H_ */
