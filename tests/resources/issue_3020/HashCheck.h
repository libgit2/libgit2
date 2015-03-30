/* Author: macote */

#ifndef HASHCHECK_H_
#define HASHCHECK_H_

#define _CRT_SECURE_NO_WARNINGS

#include "HashType.h"
#include "HashFileProcessor.h"
#include <vector>
#include <string>
#include <algorithm>
#include <Windows.h>

class HashCheck
{
private:
	static LPCWSTR kHashFileBaseName;
public:
	HashCheck(std::vector<std::wstring> args) : args_(args)
	{
		Initialize();
	};
	int Process() const;
private:
	void Initialize();
	std::wstring GetAppFileName(LPCWSTR apptitle) const;
	BOOL ViewReport(LPCWSTR filepath) const;
private:
	std::vector<std::wstring> args_;
	std::wstring hashfilename_;
	std::wstring basepath_;
	std::wstring appfilename_;
	HashType hashtype_;
	BOOL silent_;
	BOOL checking_;
	BOOL updating_;
	BOOL skipcheck_;
};

#endif /* HASHCHECK_H_ */
