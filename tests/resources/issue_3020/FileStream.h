/* Author: macote */

#ifndef FILESTREAM_H_
#define FILESTREAM_H_

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <Windows.h>

class FileStream
{
public:
	static const DWORD kDefaultBufferSize = 32768;
public:
	enum class Mode
	{
		Open,
		OpenNoBuffering,
		Create,
		Truncate,
		Append
	};
	FileStream(const std::wstring& filepath, Mode mode) : FileStream(filepath, mode, kDefaultBufferSize) { };
	FileStream(const std::wstring& filepath, Mode mode, const DWORD buffersize) : filepath_(filepath), mode_(mode), buffersize_(buffersize)
	{
		AllocateBuffer();
		OpenFile();
	};
	virtual ~FileStream()
	{
		Flush();
		CloseFile();
		FreeBuffer();
	};
	DWORD Read(PBYTE buffer, DWORD count);
	void Write(PBYTE buffer, DWORD count);
	void Flush();
	void Close();
	DWORD lasterror() const { return lasterror_; }
private:
	void AllocateBuffer();
	void OpenFile();
	DWORD Read(PBYTE buffer, DWORD offset, DWORD count);
	DWORD Write(PBYTE buffer, DWORD offset, DWORD count);
	void FlushWrite();
	void CloseFile();
	void FreeBuffer();
	DWORD readindex_ = 0;
	DWORD readlength_ = 0;
	DWORD writeindex_ = 0;
	PBYTE buffer_ = NULL;
	const std::wstring filepath_;
	Mode mode_;
	const DWORD buffersize_;
	HANDLE filehandle_ = NULL;
	DWORD lasterror_ = 0;
};

#endif /* FILESTREAM_H_ */
