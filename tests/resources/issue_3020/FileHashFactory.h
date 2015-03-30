/* Author: macote */

#ifndef FILEHASHFACTORY_H_
#define FILEHASHFACTORY_H_

#include "FileHash.h"
#include "HashType.h"
#include "CRC32FileHash.h"
#include "MD5FileHash.h"
#include "SHA1FileHash.h"
#include <string>
#include <memory>
#include <Windows.h>

class FileHashFactory
{
public:
	static std::unique_ptr<FileHash> Create(HashType hashtype, const std::wstring& filepath)
	{ 
		if (hashtype == HashType::SHA1)
		{
			//return std::make_unique<SHA1FileHash>(filepath);
			return std::unique_ptr<SHA1FileHash>(new SHA1FileHash(filepath));
		}
		else if (hashtype == HashType::MD5)
		{
			//return std::make_unique<MD5FileHash>(filepath);
			return std::unique_ptr<MD5FileHash>(new MD5FileHash(filepath));
		}
		else if (hashtype == HashType::CRC32)
		{
			//return std::make_unique<CRC32FileHash>(filepath);
			return std::unique_ptr<CRC32FileHash>(new CRC32FileHash(filepath));
		}
		else
		{
			throw std::runtime_error("FileHashFactory.Create(): selected hash type is not supported.");
		}
	};
};

#endif /* FILEHASHFACTORY_H_ */
