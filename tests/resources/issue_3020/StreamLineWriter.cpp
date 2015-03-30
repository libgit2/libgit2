/* Author: macote */

#include "StreamLineWriter.h"

void StreamLineWriter::Write(std::wstring line)
{
	if (line.size() > 0)
	{
		DWORD cbMultiByte = WideCharToMultiByte(CP_UTF8, 0, line.c_str(), -1, NULL, 0, NULL, NULL);
		LPSTR bytes = (LPSTR)HeapAlloc(GetProcessHeap(), 0, cbMultiByte);
		cbMultiByte = WideCharToMultiByte(CP_UTF8, 0, line.c_str(), -1, bytes, cbMultiByte, NULL, NULL);
		filestream_.Write((PBYTE)bytes, cbMultiByte - 1);
		HeapFree(GetProcessHeap(), 0, bytes);
	}
}

void StreamLineWriter::WriteLine(std::wstring line)
{
	Write(line);
	WriteEOL();
}

void StreamLineWriter::WriteEOL()
{
	filestream_.Write((PBYTE)"\r\n", 2);
}

void StreamLineWriter::Close()
{
	filestream_.Close();
}