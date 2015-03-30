/* Author: macote */

#include "FileHash.h"

void FileHash::AllocateBuffer()
{
	buffer_ = (PBYTE)VirtualAlloc(NULL, buffersize_, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

void FileHash::FreeBuffer()
{
	if (buffer_ != NULL)
	{
		VirtualFree(buffer_, 0, MEM_RELEASE);
	}
}

void FileHash::Compute()
{
	Initialize();
	DWORD bytesread = 0;
	FileHashBytesProcessedEventArgs fhbpea;
	fhbpea.bytesprocessed.QuadPart = 0;
	DWORD runningnotificationblocksize = 0;
	do
	{
		bytesread = filestream_.Read(buffer_, buffersize_);
		if (bytesread > 0)
		{
			Update(bytesread);
		}
		if (bytesprocessedevent_ != nullptr)
		{
			fhbpea.bytesprocessed.QuadPart += bytesread;
			runningnotificationblocksize += bytesread;
			if (runningnotificationblocksize >= bytesprocessednotificationblocksize_ || bytesread == 0)
			{
				if (bytesread > 0)
				{
					runningnotificationblocksize -= bytesprocessednotificationblocksize_;
				}
				bytesprocessedevent_(fhbpea);
			}
		}
	}
	while (bytesread > 0);
	Finalize();
	ConvertHashToDigestString();
}
