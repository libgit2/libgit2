/* Author: macote */

#include "FileTree.h"

void FileTree::ProcessTree(const std::wstring& path) const 
{
	WIN32_FIND_DATAW findfiledata;
	HANDLE hFind;
	std::wstring pattern = path + L"*";
	hFind = FindFirstFileW(pattern.c_str(), &findfiledata);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do 
		{
			if (findfiledata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) 
			{
				if (lstrcmpW(findfiledata.cFileName, L".") != 0 && lstrcmpW(findfiledata.cFileName, L"..") != 0) 
				{
					std::wstring currentpath(path + findfiledata.cFileName + L"\\");
					ProcessTree(currentpath);
				}
			}
			else
			{
				std::wstring currentfile(path + findfiledata.cFileName);
				fileaction_.ProcessFile(currentfile);
			}
		} while (FindNextFileW(hFind, &findfiledata));
		FindClose(hFind);
	}
}
