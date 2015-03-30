/* Author: macote */

#ifndef SHA1FILEHASH_H_
#define SHA1FILEHASH_H_

#include "FileHash.h"
#include <iomanip>
#include <sstream>
#include <string>
#include <Windows.h>

struct SHA1Context
{
	UINT32 state[5];	// state (ABCDE)
	UINT32 count[2];	// bits
	BYTE buffer[64];	// input buffer
};

class SHA1FileHash : public FileHash
{
public:
#if _MSC_VER < 1900
	SHA1FileHash(const std::wstring& filepath, const DWORD buffersize) : FileHash(filepath, buffersize) { };
	SHA1FileHash(const std::wstring& filepath) : FileHash(filepath) { };
#else
	using FileHash::FileHash;
#endif
private:
	void Initialize();
	void Update(const UINT32 bytecount);
	void Finalize();
	void Transform(UINT32 state[5], PUINT32 buffer);
	void ConvertHashToDigestString();
	BYTE hash_[20];
	SHA1Context context_;
};

#endif /* SHA1FILEHASH_H_ */
