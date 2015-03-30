/* Author: macote */

#ifndef STREAMLINEREADER_H_
#define STREAMLINEREADER_H_

#include "FileStream.h"
#include <string>
#include <Windows.h>

class StreamLineReader
{
private:
	static const DWORD kDefaultBufferSize = 32768;
public:
	enum class Encoding
	{
		UTF8
	};
	StreamLineReader(FileStream& filestream, Encoding encoding, const DWORD buffersize) : filestream_(filestream), encoding_(encoding), buffersize_(buffersize)
	{
		AllocateBuffer();
	}
	StreamLineReader(FileStream& filestream, Encoding encoding) : StreamLineReader(filestream, Encoding::UTF8, kDefaultBufferSize) { }
	StreamLineReader(FileStream& filestream) : StreamLineReader(filestream, Encoding::UTF8) { }
	~StreamLineReader()
	{
		FreeBuffer();
	}
	std::wstring ReadLine();
	BOOL EndOfStream();
	void Close();
private:
	void AllocateBuffer();
	void FreeBuffer();
	DWORD ReadBytes();
	FileStream& filestream_;
	Encoding encoding_;
	PBYTE buffer_;
	DWORD buffersize_;
	DWORD readindex_ = 0;
	DWORD readlength_ = 0;
};

#endif /* STREAMLINEREADER_H_ */
