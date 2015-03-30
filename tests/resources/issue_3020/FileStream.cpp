/* Author: macote */
/* Portions of this code was inspired by dotnet/corefx's Win32FileStream.cs */
/*

The MIT License (MIT)

Copyright (c) Microsoft Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include "FileStream.h"

void FileStream::AllocateBuffer()
{
	buffer_ = (PBYTE)VirtualAlloc(NULL, buffersize_, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

void FileStream::FreeBuffer()
{
	if (buffer_ != NULL)
	{
		VirtualFree(buffer_, 0, MEM_RELEASE);
	}
}

void FileStream::OpenFile()
{
	DWORD desiredaccess = (mode_ >= Mode::Create) ? GENERIC_WRITE : GENERIC_READ;
	DWORD createdisposition;
	switch (mode_)
	{
	case FileStream::Mode::Create:
		createdisposition = CREATE_NEW;
		break;
	case FileStream::Mode::Truncate:
		createdisposition = CREATE_ALWAYS;
		break;
	default:
		createdisposition = OPEN_EXISTING;
		break;
	}
	DWORD flagsandattributes = FILE_ATTRIBUTE_NORMAL;
	if (mode_ == Mode::OpenNoBuffering)
	{
		flagsandattributes |= FILE_FLAG_NO_BUFFERING;
	}
	filehandle_ = CreateFileW(filepath_.c_str(), desiredaccess, FILE_SHARE_READ,
		NULL, createdisposition, flagsandattributes, NULL);
	if (filehandle_ == INVALID_HANDLE_VALUE)
	{
		std::stringstream ss;
		ss << "FileStream.Open(CreateFileW()) failed with error ";
		ss << "0x" << std::hex << std::setw(8) << std::setfill('0') << std::uppercase;
		ss << GetLastError();
		throw std::runtime_error(ss.str());
	}
}

DWORD FileStream::Read(PBYTE buffer, DWORD offset, DWORD count)
{
	DWORD bytesread = 0;
	ReadFile(filehandle_, buffer + offset, count, &bytesread, NULL);
	return bytesread;
}

DWORD FileStream::Write(PBYTE buffer, DWORD offset, DWORD count)
{
	DWORD byteswritten = 0;
	WriteFile(filehandle_, buffer + offset, count, &byteswritten, NULL);
	return byteswritten;
}

void FileStream::FlushWrite()
{
	Write(buffer_, 0, writeindex_);
	writeindex_ = 0;
}

void FileStream::CloseFile()
{
	if (filehandle_ != INVALID_HANDLE_VALUE)
	{
		CloseHandle(filehandle_);
		filehandle_ = INVALID_HANDLE_VALUE;
	}
}

DWORD FileStream::Read(PBYTE buffer, DWORD count)
{
	DWORD bufferbytes = readlength_ - readindex_;
	BOOL eof = FALSE;
	if (bufferbytes == 0)
	{
		DWORD bytesread;
		if (count >= buffersize_)
		{
			bytesread = Read(buffer, 0, count);
			readindex_ = readlength_ = 0;
			return bytesread;
		}
		bytesread = Read(buffer_, 0, buffersize_);
		if (bytesread == 0) return 0;
		readindex_ = 0;
		readlength_ = bufferbytes = bytesread;
		eof = bytesread < buffersize_;
	}
	if (bufferbytes > count)
	{
		bufferbytes = count;
	}
	CopyMemory(buffer, buffer_ + readindex_, bufferbytes);
	readindex_ += bufferbytes;
	if (bufferbytes < count && !eof)
	{
		DWORD bytesread = Read(buffer, bufferbytes, count - bufferbytes);
		bufferbytes += bytesread;
		readindex_ = readlength_ = 0;
	}
	return bufferbytes;
}

void FileStream::Write(PBYTE buffer, DWORD count)
{
	DWORD bufferindex = 0;
	if (writeindex_ > 0)
	{
		DWORD bufferbytes = buffersize_ - writeindex_;
		if (bufferbytes > 0)
		{
			if (bufferbytes > count)
			{
				bufferbytes = count;
			}
			CopyMemory(buffer_ + writeindex_, buffer, bufferbytes);
			writeindex_ += bufferbytes;
			if (bufferbytes == count) return;
			bufferindex = bufferbytes;
			count -= bufferbytes;
		}
		Write(buffer_, 0, writeindex_);
		writeindex_ = 0;
	}
	if (count >= buffersize_)
	{
		Write(buffer, 0, count);
	}
	else if (count > 0)
	{
		CopyMemory(buffer_ + writeindex_, buffer + bufferindex, count);
		writeindex_ = count;
	}
}

void FileStream::Flush()
{
	if (writeindex_ > 0)
	{
		FlushWrite();
	}
}

void FileStream::Close()
{
	Flush();
	CloseFile();
}
