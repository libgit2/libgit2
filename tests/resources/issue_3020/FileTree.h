/* Author: macote */

#ifndef FILETREE_H_
#define FILETREE_H_

#include <string>
#include <Windows.h>

class IFileTreeAction
{
public:
	virtual ~IFileTreeAction() { }
	virtual void ProcessFile(const std::wstring& filepath) = 0;
};

class FileTree
{
public:
	FileTree(const std::wstring basepath, IFileTreeAction& fileaction) : basepath_(basepath), fileaction_(fileaction) { };
	void Process() const 
	{
		ProcessTree(basepath_);
	}
private:
	void ProcessTree(const std::wstring& path) const;
	const std::wstring basepath_;
	IFileTreeAction& fileaction_;
};

#endif /* FILETREE_H_ */
