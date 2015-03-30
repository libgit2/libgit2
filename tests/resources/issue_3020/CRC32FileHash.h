/* Author: macote */

#ifndef CRC32FILEHASH_H_
#define CRC32FILEHASH_H_

#include "FileHash.h"
#include <iomanip>
#include <sstream>
#include <string>
#include <Windows.h>

class CRC32FileHash : public FileHash
{
public:
#if _MSC_VER < 1900
	CRC32FileHash(const std::wstring& filepath, const DWORD buffersize) : FileHash(filepath, buffersize) { };
	CRC32FileHash(const std::wstring& filepath) : FileHash(filepath) { };
#else
	using FileHash::FileHash;
#endif
private:
	void Initialize();
	void Update(const UINT32 bytecount);
	void Finalize();
	void ConvertHashToDigestString();
	const static UINT32 kCRC32Table[];
	UINT32 hash_;
};

#endif /* CRC32FILEHASH_H_ */
