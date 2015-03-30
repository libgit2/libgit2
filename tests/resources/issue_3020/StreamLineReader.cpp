/* Author: macote */

#include "StreamLineReader.h"

void StreamLineReader::AllocateBuffer()
{
	buffer_ = (PBYTE)VirtualAlloc(NULL, buffersize_, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

void StreamLineReader::FreeBuffer()
{
	if (buffer_ != NULL)
	{
		VirtualFree(buffer_, 0, MEM_RELEASE);
	}
}

DWORD StreamLineReader::ReadBytes()
{
	if (readlength_ - readindex_ == 0)
	{
		readindex_ = 0;
		readlength_ = filestream_.Read(buffer_, buffersize_);
	}
	return readlength_;
}

std::wstring StreamLineReader::ReadLine()
{
	BOOL eol = FALSE;
	std::string raw;
	while (!eol)
	{
		DWORD bufferbytes = readlength_ - readindex_;
		if (bufferbytes == 0)
		{
			bufferbytes = ReadBytes();
			if (bufferbytes == 0) break;
		}
		char* p = (char *)buffer_ + readindex_;
		while (*p != '\r' && *p != '\n' && p <= (char *)buffer_ + readindex_)
		{
			raw.append(p, 1);
			readindex_++;
			if (readlength_ - readindex_ == 0) break;
			p++;
		}
		if (*p == '\r' || *p == '\n')
		{
			eol = TRUE;
			readindex_++;
			if (*p == '\r' && (readlength_ - readindex_ > 0) && *(p + 1) == '\n')
			{
				readindex_++;
			}
		}
	}
	if (raw.size() > 0)
	{
		DWORD cchWideChar = MultiByteToWideChar(CP_UTF8, 0, raw.c_str(), -1, NULL, 0);
		WCHAR* wideChars = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, cchWideChar * sizeof(WCHAR));
		cchWideChar = MultiByteToWideChar(CP_UTF8, 0, raw.c_str(), -1, wideChars, cchWideChar);
		std::wstring line(wideChars);
		HeapFree(GetProcessHeap(), 0, wideChars);
		return line;
	}
	return L"";
}

BOOL StreamLineReader::EndOfStream()
{
	return ReadBytes() == 0;
}

void StreamLineReader::Close()
{
	filestream_.Close();
}