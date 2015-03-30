/* Author: macote */

#ifndef HASHFILE_H_
#define HASHFILE_H_

#include "FileStream.h"
#include "StreamLineReader.h"
#include "StreamLineWriter.h"
#include <sstream>
#include <list>
#include <map>
#include <string>
#include <Windows.h>

class FileEntry
{
public:
	FileEntry(const std::wstring filepath, const LARGE_INTEGER size, const std::wstring digest) 
		: filepath_(filepath), size_(size), digest_(digest) { };
	std::wstring filepath() const { return filepath_; }
	LARGE_INTEGER size() const { return size_; }
	std::wstring digest() const { return digest_; }
private:
	const std::wstring filepath_;
	const LARGE_INTEGER size_;
	const std::wstring digest_;
};

class HashFile
{
public:
	static const FileEntry kFileEntryNull;
public:
	HashFile() { };
	~HashFile()
	{
		files_.clear();
	}
	void Save(const std::wstring& hashfilepath) const;
	void Load(const std::wstring& hashfilepath);
	void AddFileEntry(const std::wstring filepath, const LARGE_INTEGER li, const std::wstring digest);
	void RemoveFileEntry(const std::wstring& filepath);
	BOOL IsEmpty() const { return files_.size() == 0; }
	BOOL ContainsFileEntry(const std::wstring& filepath) const;
	const FileEntry& GetFileEntry(const std::wstring& filepath) const;
	std::list<std::wstring> GetFilePaths() const;
private:
	std::map<std::wstring, FileEntry, std::less<std::wstring>>::const_iterator FindEntry(const std::wstring& filepath) const;
	BOOL IsValidHashLine(const std::wstring& fileentryline) const;
	std::wstring LargeIntToString(const LARGE_INTEGER& li) const;
private:
	std::map<std::wstring, FileEntry, std::less<std::wstring>> files_;
};

#endif /* HASHFILE_H_ */
