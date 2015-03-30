/* Author: macote */

#ifndef FILEHASH_H_
#define FILEHASH_H_

#include "FileStream.h"
#include <string>
#include <functional>
#include <Windows.h>

struct FileHashBytesProcessedEventArgs
{
	LARGE_INTEGER bytesprocessed;
};

class FileHash
{
public:
	static const DWORD kDefaultBufferSize = 32768;
	static const DWORD kDefaultBytesProcessedNotificationBlockSize = 1048576;
public:
	FileHash(const std::wstring& filepath) : FileHash(filepath, kDefaultBufferSize) { };
	FileHash(const std::wstring& filepath, const DWORD buffersize) 
		: buffersize_(buffersize), filestream_(FileStream(filepath, FileStream::Mode::OpenNoBuffering, buffersize))
	{
		bytesprocessedevent_ = nullptr;
		AllocateBuffer();
	}
	virtual ~FileHash()
	{
		FreeBuffer();
	};
	void Compute();
	std::wstring digest() const { return digest_; }
	void SetBytesProcessedEventHandler(std::function<void(FileHashBytesProcessedEventArgs)> handler)
	{
		SetBytesProcessedEventHandler(handler, kDefaultBytesProcessedNotificationBlockSize);
	}
	void SetBytesProcessedEventHandler(std::function<void(FileHashBytesProcessedEventArgs)> handler, 
		const DWORD bytesprocessednotificationblocksize)
	{
		bytesprocessedevent_ = handler;
		bytesprocessednotificationblocksize_ = bytesprocessednotificationblocksize;
	}
protected:
	virtual void Initialize() = 0;
	virtual void Update(const UINT32 bytes) = 0;
	virtual void Finalize() = 0;
	virtual void ConvertHashToDigestString() = 0;
	PBYTE buffer_ = NULL;
	std::wstring digest_;
private:
	void AllocateBuffer();
	void FreeBuffer();
	DWORD buffersize_;
	FileStream filestream_;
	DWORD bytesprocessednotificationblocksize_;
	std::function<void(FileHashBytesProcessedEventArgs)> bytesprocessedevent_;
};

#endif /* FILEHASH_H_ */
